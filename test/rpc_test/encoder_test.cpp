#include "encoder.h"
#include "raft_serializer.h"
#include "scheduler.h"
#include "logger.h"
#include <gtest/gtest.h>
#include <vector>
#include <string>

using namespace rpc;

// Test basic int encoding/decoding
TEST(EncoderTest, BasicInt) {
    auto encoder = Encoder::New();
    encoder->Encode(42);
    encoder->Encode(100);
    encoder->Encode(-50);
    
    std::string data = encoder->Bytes();
    EXPECT_FALSE(data.empty());
    
    auto decoder = Decoder::New(data);
    int v1, v2, v3;
    EXPECT_TRUE(decoder->Decode(v1));
    EXPECT_TRUE(decoder->Decode(v2));
    EXPECT_TRUE(decoder->Decode(v3));
    EXPECT_FALSE(decoder->HasMore());
    
    EXPECT_EQ(v1, 42);
    EXPECT_EQ(v2, 100);
    EXPECT_EQ(v3, -50);
}

// Test string encoding/decoding
TEST(EncoderTest, String) {
    auto encoder = Encoder::New();
    encoder->Encode(std::string("hello"));
    encoder->Encode(std::string("world"));
    encoder->Encode(std::string(""));
    
    std::string data = encoder->Bytes();
    
    auto decoder = Decoder::New(data);
    std::string s1, s2, s3;
    EXPECT_TRUE(decoder->Decode(s1));
    EXPECT_TRUE(decoder->Decode(s2));
    EXPECT_TRUE(decoder->Decode(s3));
    
    EXPECT_EQ(s1, "hello");
    EXPECT_EQ(s2, "world");
    EXPECT_EQ(s3, "");
}

// Test vector encoding/decoding
TEST(EncoderTest, Vector) {
    auto encoder = Encoder::New();
    
    std::vector<int> vec1{1, 2, 3, 4, 5};
    std::vector<std::string> vec2{"a", "b", "c"};
    
    encoder->Encode(vec1);
    encoder->Encode(vec2);
    
    std::string data = encoder->Bytes();
    
    auto decoder = Decoder::New(data);
    std::vector<int> decoded_vec1;
    std::vector<std::string> decoded_vec2;
    
    EXPECT_TRUE(decoder->Decode(decoded_vec1));
    EXPECT_TRUE(decoder->Decode(decoded_vec2));
    
    EXPECT_EQ(decoded_vec1, vec1);
    EXPECT_EQ(decoded_vec2, vec2);
}

// Test mixed types (like Raft persist)
TEST(EncoderTest, MixedTypes) {
    auto encoder = Encoder::New();
    
    int term = 5;
    int votedFor = 2;
    std::vector<int> logs{1, 2, 3};
    int lastIncludedIndex = 10;
    int lastIncludedTerm = 3;
    
    encoder->Encode(term);
    encoder->Encode(votedFor);
    encoder->Encode(logs);
    encoder->Encode(lastIncludedIndex);
    encoder->Encode(lastIncludedTerm);
    
    std::string data = encoder->Bytes();
    
    auto decoder = Decoder::New(data);
    int decoded_term, decoded_votedFor;
    std::vector<int> decoded_logs;
    int decoded_lastIncludedIndex, decoded_lastIncludedTerm;
    
    EXPECT_TRUE(decoder->Decode(decoded_term));
    EXPECT_TRUE(decoder->Decode(decoded_votedFor));
    EXPECT_TRUE(decoder->Decode(decoded_logs));
    EXPECT_TRUE(decoder->Decode(decoded_lastIncludedIndex));
    EXPECT_TRUE(decoder->Decode(decoded_lastIncludedTerm));
    
    EXPECT_EQ(decoded_term, term);
    EXPECT_EQ(decoded_votedFor, votedFor);
    EXPECT_EQ(decoded_logs, logs);
    EXPECT_EQ(decoded_lastIncludedIndex, lastIncludedIndex);
    EXPECT_EQ(decoded_lastIncludedTerm, lastIncludedTerm);
}

// Test LogEntry encoding/decoding
TEST(EncoderTest, LogEntry) {
    auto encoder = Encoder::New();
    
    raft::LogEntry entry1(std::string("cmd1"), 1, 1, 0);
    raft::LogEntry entry2(std::string("cmd2"), 2, 2, 0);
    raft::LogEntry entry3;  // Empty entry
    
    encoder->Encode(entry1);
    encoder->Encode(entry2);
    encoder->Encode(entry3);
    
    std::string data = encoder->Bytes();
    
    auto decoder = Decoder::New(data);
    raft::LogEntry decoded1, decoded2, decoded3;
    
    EXPECT_TRUE(decoder->Decode(decoded1));
    EXPECT_TRUE(decoder->Decode(decoded2));
    EXPECT_TRUE(decoder->Decode(decoded3));
    
    EXPECT_EQ(decoded1.term, 1);
    EXPECT_EQ(decoded1.index, 1);
    EXPECT_EQ(decoded1.log_from, 0);
    EXPECT_TRUE(decoded1.command.has_value());
    EXPECT_EQ(std::any_cast<std::string>(decoded1.command), "cmd1");
    
    EXPECT_EQ(decoded2.term, 2);
    EXPECT_EQ(decoded2.index, 2);
    EXPECT_EQ(std::any_cast<std::string>(decoded2.command), "cmd2");
    
    EXPECT_EQ(decoded3.term, 0);
    EXPECT_EQ(decoded3.index, 0);
}

// Test vector of LogEntry
TEST(EncoderTest, LogEntryVector) {
    auto encoder = Encoder::New();
    
    std::vector<raft::LogEntry> logs;
    logs.push_back(raft::LogEntry(std::string(""), 0, 0, -1));  // SENTRY
    logs.push_back(raft::LogEntry(std::string("cmd1"), 1, 1, 0));
    logs.push_back(raft::LogEntry(std::string("cmd2"), 1, 2, 0));
    logs.push_back(raft::LogEntry(std::string("cmd3"), 2, 3, 1));
    
    encoder->Encode(logs);
    
    std::string data = encoder->Bytes();
    
    auto decoder = Decoder::New(data);
    std::vector<raft::LogEntry> decoded_logs;
    
    EXPECT_TRUE(decoder->Decode(decoded_logs));
    EXPECT_EQ(decoded_logs.size(), 4);
    
    // Check SENTRY entry
    EXPECT_EQ(decoded_logs[0].term, 0);
    EXPECT_EQ(decoded_logs[0].index, 0);
    EXPECT_EQ(decoded_logs[0].log_from, -1);
    
    // Check first command
    EXPECT_EQ(decoded_logs[1].term, 1);
    EXPECT_EQ(decoded_logs[1].index, 1);
    EXPECT_EQ(std::any_cast<std::string>(decoded_logs[1].command), "cmd1");
    
    // Check last command
    EXPECT_EQ(decoded_logs[3].term, 2);
    EXPECT_EQ(decoded_logs[3].index, 3);
    EXPECT_EQ(decoded_logs[3].log_from, 1);
    EXPECT_EQ(std::any_cast<std::string>(decoded_logs[3].command), "cmd3");
}

// Test Raft persist/readPersist pattern
TEST(EncoderTest, RaftPersistPattern) {
    auto encoder = Encoder::New();
    
    // Simulate Raft persist
    int currentTerm = 5;
    int votedFor = 2;
    std::vector<raft::LogEntry> logs;
    logs.push_back(raft::LogEntry(std::string(""), 0, 0, -1));
    logs.push_back(raft::LogEntry(std::string("set x 1"), 1, 1, 0));
    logs.push_back(raft::LogEntry(std::string("set y 2"), 1, 2, 0));
    logs.push_back(raft::LogEntry(std::string("get x"), 2, 3, 1));
    int lastIncludedIndex = 0;
    int lastIncludedTerm = 0;
    
    encoder->Encode(currentTerm);
    encoder->Encode(votedFor);
    encoder->Encode(logs);
    encoder->Encode(lastIncludedIndex);
    encoder->Encode(lastIncludedTerm);
    
    std::string raftState = encoder->Bytes();
    
    // Simulate Raft readPersist
    auto decoder = Decoder::New(raftState);
    
    int restored_term;
    int restored_votedFor;
    std::vector<raft::LogEntry> restored_logs;
    int restored_lastIncludedIndex;
    int restored_lastIncludedTerm;
    
    EXPECT_TRUE(decoder->Decode(restored_term));
    EXPECT_TRUE(decoder->Decode(restored_votedFor));
    EXPECT_TRUE(decoder->Decode(restored_logs));
    EXPECT_TRUE(decoder->Decode(restored_lastIncludedIndex));
    EXPECT_TRUE(decoder->Decode(restored_lastIncludedTerm));
    
    EXPECT_EQ(restored_term, currentTerm);
    EXPECT_EQ(restored_votedFor, votedFor);
    EXPECT_EQ(restored_logs.size(), logs.size());
    EXPECT_EQ(restored_lastIncludedIndex, lastIncludedIndex);
    EXPECT_EQ(restored_lastIncludedTerm, lastIncludedTerm);
    
    // Verify log content
    for (size_t i = 0; i < logs.size(); i++) {
        EXPECT_EQ(restored_logs[i].term, logs[i].term);
        EXPECT_EQ(restored_logs[i].index, logs[i].index);
        EXPECT_EQ(restored_logs[i].log_from, logs[i].log_from);
    }
}

// Test empty data
TEST(EncoderTest, EmptyData) {
    auto decoder = Decoder::New("");
    int value;
    EXPECT_FALSE(decoder->Decode(value));
    EXPECT_FALSE(decoder->HasMore());
}

// Test decoder bounds
TEST(EncoderTest, DecoderBounds) {
    auto encoder = Encoder::New();
    encoder->Encode(42);
    encoder->Encode(100);
    
    std::string data = encoder->Bytes();
    auto decoder = Decoder::New(data);
    
    int v1, v2, v3;
    EXPECT_TRUE(decoder->Decode(v1));
    EXPECT_TRUE(decoder->HasMore());
    EXPECT_TRUE(decoder->Decode(v2));
    EXPECT_FALSE(decoder->HasMore());
    EXPECT_FALSE(decoder->Decode(v3));  // Out of bounds
    
    EXPECT_EQ(decoder->Position(), 2);
}

// Test decoder reset
TEST(EncoderTest, DecoderReset) {
    auto encoder = Encoder::New();
    encoder->Encode(42);
    encoder->Encode(100);
    
    std::string data = encoder->Bytes();
    auto decoder = Decoder::New(data);
    
    int v1, v2;
    EXPECT_TRUE(decoder->Decode(v1));
    EXPECT_TRUE(decoder->Decode(v2));
    EXPECT_EQ(v1, 42);
    EXPECT_EQ(v2, 100);
    
    // Reset and decode again
    decoder->Reset();
    EXPECT_EQ(decoder->Position(), 0);
    EXPECT_TRUE(decoder->HasMore());
    
    int v3, v4;
    EXPECT_TRUE(decoder->Decode(v3));
    EXPECT_TRUE(decoder->Decode(v4));
    EXPECT_EQ(v3, 42);
    EXPECT_EQ(v4, 100);
}

// Test encoder clear
TEST(EncoderTest, EncoderClear) {
    auto encoder = Encoder::New();
    encoder->Encode(42);
    encoder->Encode(100);
    
    std::string data1 = encoder->Bytes();
    EXPECT_FALSE(data1.empty());
    
    encoder->Clear();
    encoder->Encode(200);
    
    std::string data2 = encoder->Bytes();
    
    auto decoder = Decoder::New(data2);
    int value;
    EXPECT_TRUE(decoder->Decode(value));
    EXPECT_EQ(value, 200);
    EXPECT_FALSE(decoder->HasMore());
}

// Test bool type
TEST(EncoderTest, Bool) {
    auto encoder = Encoder::New();
    encoder->Encode(true);
    encoder->Encode(false);
    encoder->Encode(true);
    
    std::string data = encoder->Bytes();
    
    auto decoder = Decoder::New(data);
    bool b1, b2, b3;
    EXPECT_TRUE(decoder->Decode(b1));
    EXPECT_TRUE(decoder->Decode(b2));
    EXPECT_TRUE(decoder->Decode(b3));
    
    EXPECT_TRUE(b1);
    EXPECT_FALSE(b2);
    EXPECT_TRUE(b3);
}

// Test double type
TEST(EncoderTest, Double) {
    auto encoder = Encoder::New();
    encoder->Encode(3.14159);
    encoder->Encode(-2.71828);
    
    std::string data = encoder->Bytes();
    
    auto decoder = Decoder::New(data);
    double d1, d2;
    EXPECT_TRUE(decoder->Decode(d1));
    EXPECT_TRUE(decoder->Decode(d2));
    
    EXPECT_DOUBLE_EQ(d1, 3.14159);
    EXPECT_DOUBLE_EQ(d2, -2.71828);
}

FIBER_MAIN() {
    LOG_INFO("=== Starting Encoder/Decoder Tests ===");
    
    ::testing::InitGoogleTest();
    int result = RUN_ALL_TESTS();
    
    LOG_INFO("=== Tests Completed ===");
    return result;
}
