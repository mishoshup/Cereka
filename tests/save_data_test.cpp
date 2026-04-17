// save_data_test.cpp — Tests for SerializableSaveData
//
// Tests Glaze JSON serialization/deserialization of game saves.

#include "save_data.hpp"
#include <gtest/gtest.h>
#include <string>

using namespace cereka;

TEST(SaveDataTest,
     EmptySaveSerializesToValidJson)
{
    SerializableSaveData data;
    std::string json;

    bool success = saveDataToJson(data, json);
    EXPECT_TRUE(success);
    EXPECT_FALSE(json.empty());

    // Should be valid JSON object
    EXPECT_TRUE(json.front() == '{');
    EXPECT_TRUE(json.back() == '}');
}

TEST(SaveDataTest,
     RoundtripSerializationPreservesData)
{
    SerializableSaveData original;
    original.timestamp = "2024-01-15 14:30";
    original.programCounter = 42;
    original.background = "beach.png";
    original.bgm = "theme.ogg";
    original.state = "WaitingForInput";
    original.speaker = "alice";
    original.name = "Alice";
    original.text = "Hello world!";
    original.displayedChars = 5;
    original.skipMode = true;
    original.skipDepth = 3;
    original.variables["score"] = "100";
    original.variables["name"] = "Test";

    // Serialize
    std::string json;
    ASSERT_TRUE(saveDataToJson(original, json));

    // Deserialize
    SerializableSaveData loaded;
    ASSERT_TRUE(jsonToSaveData(loaded, json));

    // Verify
    EXPECT_EQ(loaded.timestamp, original.timestamp);
    EXPECT_EQ(loaded.programCounter, original.programCounter);
    EXPECT_EQ(loaded.background, original.background);
    EXPECT_EQ(loaded.bgm, original.bgm);
    EXPECT_EQ(loaded.state, original.state);
    EXPECT_EQ(loaded.speaker, original.speaker);
    EXPECT_EQ(loaded.name, original.name);
    EXPECT_EQ(loaded.text, original.text);
    EXPECT_EQ(loaded.displayedChars, original.displayedChars);
    EXPECT_EQ(loaded.skipMode, original.skipMode);
    EXPECT_EQ(loaded.skipDepth, original.skipDepth);
    EXPECT_EQ(loaded.variables["score"], "100");
    EXPECT_EQ(loaded.variables["name"], "Test");
}

TEST(SaveDataTest,
     CharactersSerializeCorrectly)
{
    SerializableSaveData original;
    original.timestamp = "2024-01-15";

    SerializableCharacter ch1;
    ch1.id = "alice";
    ch1.file = "alice_happy.png";
    ch1.position = "center";
    original.characters.push_back(ch1);

    SerializableCharacter ch2;
    ch2.id = "bob";
    ch2.file = "bob_idle.png";
    ch2.position = "left";
    original.characters.push_back(ch2);

    // Roundtrip
    std::string json;
    ASSERT_TRUE(saveDataToJson(original, json));

    SerializableSaveData loaded;
    ASSERT_TRUE(jsonToSaveData(loaded, json));

    ASSERT_EQ(loaded.characters.size(), 2);
    EXPECT_EQ(loaded.characters[0].id, "alice");
    EXPECT_EQ(loaded.characters[0].file, "alice_happy.png");
    EXPECT_EQ(loaded.characters[0].position, "center");
    EXPECT_EQ(loaded.characters[1].id, "bob");
    EXPECT_EQ(loaded.characters[1].position, "left");
}

TEST(SaveDataTest,
     CallStackSerializesCorrectly)
{
    SerializableSaveData original;
    original.callStack = {1, 5, 10, 42};

    std::string json;
    ASSERT_TRUE(saveDataToJson(original, json));

    SerializableSaveData loaded;
    ASSERT_TRUE(jsonToSaveData(loaded, json));

    ASSERT_EQ(loaded.callStack.size(), 4);
    EXPECT_EQ(loaded.callStack[0], 1);
    EXPECT_EQ(loaded.callStack[3], 42);
}

TEST(SaveDataTest,
     EmptyJsonReturnsTrue)
{
    SerializableSaveData data;
    EXPECT_TRUE(jsonToSaveData(data, ""));
    EXPECT_TRUE(jsonToSaveData(data, "{}"));
}
