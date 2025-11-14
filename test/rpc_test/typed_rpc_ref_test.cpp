#include "rpc_server.h"
#include "rpc_client.h"
#include "scheduler.h"
#include "fiber.h"
#include "logger.h"
#include <cassert>
#include <cmath>

struct DivideInput {
    int dividend;
    int divisor;
};

struct DivideOutput {
    int quotient;
    int remainder;
};

struct Point {
    double x;
    double y;
};

struct PolarCoord {
    double radius;
    double angle;
};

struct Rectangle {
    Point topLeft;
    Point bottomRight;
};

struct RectStats {
    double area;
    double perimeter;
    Point center;
};

// 容器测试结构
struct VectorInput {
    std::vector<int> numbers;
};

struct VectorOutput {
    int sum;
    int max;
    int min;
};

struct MapInput {
    std::map<std::string, int> scores;
};

struct MapOutput {
    int total;
    std::string top_student;
    std::vector<std::string> passed;  // 嵌套容器
};

struct ComplexContainer {
    std::vector<Point> points;
    std::map<std::string, std::vector<int>> data;
    std::unordered_map<int, std::string> id_map;
};

// ============================================================================
// RPC服务端业务逻辑（纯函数，无需关心序列化）
// ============================================================================

std::optional<std::string> divide(const DivideInput& input, DivideOutput& output) {
    LOG_INFO("Server: divide({} / {}) called", input.dividend, input.divisor);
    
    // 检查除零错误
    if (input.divisor == 0) {
        return "Division by zero";
    }
    
    output.quotient = input.dividend / input.divisor;
    output.remainder = input.dividend % input.divisor;
    return std::nullopt;
}

std::optional<std::string> cartesianToPolar(const Point& p, PolarCoord& output) {
    LOG_INFO("Server: cartesianToPolar({}, {}) called", p.x, p.y);
    
    output.radius = std::sqrt(p.x * p.x + p.y * p.y);
    output.angle = std::atan2(p.y, p.x);
    return std::nullopt;
}

std::optional<std::string> analyzeRectangle(const Rectangle& rect, RectStats& output) {
    LOG_INFO("Server: analyzeRectangle called - topLeft({}, {}), bottomRight({}, {})",
             rect.topLeft.x, rect.topLeft.y, rect.bottomRight.x, rect.bottomRight.y);
    
    double width = rect.bottomRight.x - rect.topLeft.x;
    double height = rect.bottomRight.y - rect.topLeft.y;
    
    // 检查无效矩形
    if (width <= 0 || height <= 0) {
        return "Invalid rectangle: width and height must be positive";
    }
    
    output.area = width * height;
    output.perimeter = 2 * (width + height);
    output.center.x = (rect.topLeft.x + rect.bottomRight.x) / 2;
    output.center.y = (rect.topLeft.y + rect.bottomRight.y) / 2;
    return std::nullopt;
}

std::optional<std::string> processVector(const VectorInput& input, VectorOutput& output) {
    LOG_INFO("Server: processVector called with {} numbers", input.numbers.size());
    
    if (input.numbers.empty()) {
        return "Vector is empty";
    }
    
    output.sum = 0;
    output.max = input.numbers[0];
    output.min = input.numbers[0];
    
    for (int n : input.numbers) {
        output.sum += n;
        output.max = std::max(output.max, n);
        output.min = std::min(output.min, n);
    }
    
    return std::nullopt;
}

std::optional<std::string> processMap(const MapInput& input, MapOutput& output) {
    LOG_INFO("Server: processMap called with {} students", input.scores.size());
    
    if (input.scores.empty()) {
        return "Map is empty";
    }
    
    output.total = 0;
    int max_score = 0;
    
    for (const auto& [name, score] : input.scores) {
        output.total += score;
        if (score > max_score) {
            max_score = score;
            output.top_student = name;
        }
        if (score >= 60) {
            output.passed.push_back(name);
        }
    }
    
    return std::nullopt;
}

std::optional<std::string> processComplexContainer(const ComplexContainer& input, ComplexContainer& output) {
    LOG_INFO("Server: processComplexContainer called");
    
    // 复制并处理points
    output.points = input.points;
    for (auto& p : output.points) {
        p.x *= 2;
        p.y *= 2;
    }
    
    // 复制并处理data
    output.data = input.data;
    for (auto& [key, vec] : output.data) {
        for (auto& val : vec) {
            val *= 10;
        }
    }
    
    // 复制id_map
    output.id_map = input.id_map;
    
    return std::nullopt;
}

void setupServer(rpc::RpcServer& server) {
    server.registerHandler("divide", divide);
    server.registerHandler("cartesianToPolar", cartesianToPolar);
    server.registerHandler("analyzeRectangle", analyzeRectangle);
    server.registerHandler("processVector", processVector);
    server.registerHandler("processMap", processMap);
    server.registerHandler("processComplexContainer", processComplexContainer);
}

// ============================================================================
// 主测试
// ============================================================================

void testTypedRpcWithRefOutput() {
    auto server = rpc::RpcServer::Make();
    setupServer(*server);
    
    server->start(9095);
    LOG_INFO("=== Typed RPC Server (with ref output) started on port 9095 ===");
    
    fiber::WaitGroup wg;
    wg.add(1);
    
    fiber::Fiber::go([&wg]() {
        auto client_ptr = rpc::RpcClient::Make();
        auto& client = *client_ptr;
        
        if (!client.connect("127.0.0.1", 9095)) {
            LOG_ERROR("Client: failed to connect");
            return;
        }
        
        LOG_INFO("=== Running Typed RPC Tests with Reference Output ===\n");
        
        try {
            // ====================================================================
            // 测试1: 基本调用
            // ====================================================================
            LOG_INFO("--- Test 1: Basic Call ---");
            
            DivideInput input1{.dividend = 17, .divisor = 5};
            DivideOutput output1;
            
            auto error1 = client.call("divide", input1, output1);
            assert(!error1.has_value());
            
            LOG_INFO("✓ divide(17 / 5) -> quotient: {}, remainder: {}", 
                     output1.quotient, output1.remainder);
            assert(output1.quotient == 3 && output1.remainder == 2);
            
            // ====================================================================
            // 测试2: 错误处理（除零）
            // ====================================================================
            LOG_INFO("\n--- Test 2: Error Handling (Divide by Zero) ---");
            
            DivideInput input2{.dividend = 10, .divisor = 0};
            DivideOutput output2;
            
            auto error2 = client.call("divide", input2, output2);
            assert(error2.has_value());
            LOG_INFO("✓ divide(10 / 0) correctly failed with error: {}", error2.value());
            
            // ====================================================================
            // 测试3: 复杂结构
            // ====================================================================
            LOG_INFO("\n--- Test 3: Complex Struct ---");
            
            Point p{.x = 3.0, .y = 4.0};
            PolarCoord polar;
            
            auto error3 = client.call("cartesianToPolar", p, polar);
            assert(!error3.has_value());
            
            LOG_INFO("✓ cartesianToPolar(3, 4) -> radius: {:.2f}, angle: {:.2f}", 
                     polar.radius, polar.angle);
            assert(std::abs(polar.radius - 5.0) < 0.01);
            
            // ====================================================================
            // 测试4: 嵌套结构
            // ====================================================================
            LOG_INFO("\n--- Test 4: Nested Struct ---");
            
            Rectangle rect{
                .topLeft = {.x = 0, .y = 0},
                .bottomRight = {.x = 5, .y = 10}
            };
            RectStats stats;
            
            auto error4 = client.call("analyzeRectangle", rect, stats);
            assert(!error4.has_value());
            
            LOG_INFO("✓ analyzeRectangle -> area: {:.1f}, perimeter: {:.1f}, center: ({:.1f}, {:.1f})", 
                     stats.area, stats.perimeter, stats.center.x, stats.center.y);
            assert(std::abs(stats.area - 50.0) < 0.01);
            assert(std::abs(stats.center.x - 2.5) < 0.01);
            
            // ====================================================================
            // 测试5: 错误处理（无效矩形）
            // ====================================================================
            LOG_INFO("\n--- Test 5: Error Handling (Invalid Rectangle) ---");
            
            Rectangle invalid_rect{
                .topLeft = {.x = 5, .y = 10},
                .bottomRight = {.x = 0, .y = 0}  // 反向矩形
            };
            RectStats invalid_stats;
            
            auto error5 = client.call("analyzeRectangle", invalid_rect, invalid_stats);
            assert(error5.has_value());
            LOG_INFO("✓ Invalid rectangle correctly failed with error: {}", error5.value());
            
            // ====================================================================
            // 测试6: Vector容器
            // ====================================================================
            LOG_INFO("\n--- Test 6: Vector Container ---");
            
            VectorInput vec_input{.numbers = {10, 5, 20, 3, 15}};
            VectorOutput vec_output;
            
            auto error6 = client.call("processVector", vec_input, vec_output);
            assert(!error6.has_value());
            
            LOG_INFO("✓ processVector([10,5,20,3,15]) -> sum: {}, max: {}, min: {}", 
                     vec_output.sum, vec_output.max, vec_output.min);
            assert(vec_output.sum == 53);
            assert(vec_output.max == 20);
            assert(vec_output.min == 3);
            
            // ====================================================================
            // 测试7: Map容器
            // ====================================================================
            LOG_INFO("\n--- Test 7: Map Container ---");
            
            MapInput map_input{
                .scores = {
                    {"Alice", 85},
                    {"Bob", 55},
                    {"Charlie", 92},
                    {"David", 78}
                }
            };
            MapOutput map_output;
            
            auto error7 = client.call("processMap", map_input, map_output);
            assert(!error7.has_value());
            
            LOG_INFO("✓ processMap -> total: {}, top: {}, passed: {}", 
                     map_output.total, map_output.top_student, map_output.passed.size());
            assert(map_output.total == 310);
            assert(map_output.top_student == "Charlie");
            assert(map_output.passed.size() == 3);  // Alice, Charlie, David passed
            
            // ====================================================================
            // 测试8: 复杂嵌套容器
            // ====================================================================
            LOG_INFO("\n--- Test 8: Complex Nested Containers ---");
            
            ComplexContainer complex_input{
                .points = {{1.0, 2.0}, {3.0, 4.0}},
                .data = {
                    {"series1", {1, 2, 3}},
                    {"series2", {4, 5}}
                },
                .id_map = {
                    {100, "user1"},
                    {200, "user2"}
                }
            };
            ComplexContainer complex_output;
            
            auto error8 = client.call("processComplexContainer", complex_input, complex_output);
            assert(!error8.has_value());
            
            LOG_INFO("✓ processComplexContainer -> points scaled by 2, data scaled by 10");
            assert(complex_output.points.size() == 2);
            assert(complex_output.points[0].x == 2.0);
            assert(complex_output.points[0].y == 4.0);
            assert(complex_output.data["series1"][0] == 10);
            assert(complex_output.data["series2"][1] == 50);
            assert(complex_output.id_map[100] == "user1");
            
            LOG_INFO("\n=== All Tests PASSED (including container tests) ===");
            LOG_INFO("\nKey Features:");
            LOG_INFO("  ✓ Unified interface: optional<string> call(method, input, output)");
            LOG_INFO("  ✓ Server handlers: optional<string> func(input, output)");
            LOG_INFO("  ✓ Structs need NO serialization code");
            LOG_INFO("  ✓ Boost.PFR handles everything automatically");
            LOG_INFO("  ✓ Automatic error propagation from server to client");
            LOG_INFO("  ✓ Vector, Map, UnorderedMap fully supported");
            LOG_INFO("  ✓ Nested containers work seamlessly");
            
        } catch (const std::exception& e) {
            LOG_ERROR("✗ Exception: {}", e.what());
        }
        
        client.disconnect();
        wg.done();
    });
    
    wg.wait();
}

FIBER_MAIN() {
    LOG_INFO("================= Typed RPC with Reference Output Test =====================\n");
    testTypedRpcWithRefOutput();
    LOG_INFO("\n==================== Test Completed ====================");
    return 0;
}
