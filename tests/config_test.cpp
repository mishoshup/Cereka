// config_test.cpp — Tests for ConfigManager
//
// Tests the property-based configuration system.

#include "config/config_manager.hpp"
#include "ui_config.hpp"
#include <gtest/gtest.h>
#include <sstream>

using namespace config;

class ConfigManagerTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        cm.initDefaults();
        ctx.uiCfg = &uiCfg;
        cm.setContext(ctx);
    }

    UiConfig uiCfg;
    ApplyContext ctx;
    ConfigManager cm;
};

TEST_F(ConfigManagerTest,
       InitSetsDefaults)
{
    EXPECT_EQ(cm.getValue("textbox.color"), "0 0 0 160");
    EXPECT_TRUE(cm.getValue("textbox.y").find("75%") != std::string::npos);
    EXPECT_TRUE(cm.getValue("textbox.h").find("25%") != std::string::npos);
}

TEST_F(ConfigManagerTest,
       ApplyTextboxColor)
{
    cm.apply("textbox.color", "255 128 64 200");
    EXPECT_EQ(cm.getValue("textbox.color"), "255 128 64 200");
}

TEST_F(ConfigManagerTest,
       ApplyTextboxDimensions)
{
    cm.apply("textbox.y", "70%");
    cm.apply("textbox.h", "30%");
    EXPECT_TRUE(cm.getValue("textbox.y").find("70%") != std::string::npos);
    EXPECT_TRUE(cm.getValue("textbox.h").find("30%") != std::string::npos);
}

TEST_F(ConfigManagerTest,
       ApplyPixelDimensions)
{
    cm.apply("textbox.y", "600");
    cm.apply("textbox.h", "200");
    EXPECT_TRUE(cm.getValue("textbox.y").find("600") != std::string::npos);
    EXPECT_TRUE(cm.getValue("textbox.h").find("200") != std::string::npos);
}

TEST_F(ConfigManagerTest,
       ApplyFontSize)
{
    cm.apply("font.size", "42");
    EXPECT_EQ(cm.getValue("font.size"), "42");
}

TEST_F(ConfigManagerTest,
       ApplyNameboxSettings)
{
    cm.apply("namebox.x", "100");
    cm.apply("namebox.y_offset", "-50");
    cm.apply("namebox.w", "250");
    cm.apply("namebox.h", "50");
    EXPECT_TRUE(cm.getValue("namebox.x").find("100") != std::string::npos);
    EXPECT_TRUE(cm.getValue("namebox.y_offset").find("-50") != std::string::npos);
    EXPECT_TRUE(cm.getValue("namebox.w").find("250") != std::string::npos);
    EXPECT_TRUE(cm.getValue("namebox.h").find("50") != std::string::npos);
}

TEST_F(ConfigManagerTest,
       ApplyButtonSettings)
{
    cm.apply("button.w", "500");
    cm.apply("button.h", "100");
    EXPECT_TRUE(cm.getValue("button.w").find("500") != std::string::npos);
    EXPECT_TRUE(cm.getValue("button.h").find("100") != std::string::npos);
}

TEST_F(ConfigManagerTest,
       ListProperties)
{
    auto props = cm.listProperties();
    EXPECT_GT(props.size(), 0);

    bool foundTextboxColor = false;
    bool foundFontSize = false;
    for (const auto &p : props) {
        if (p == "textbox.color")
            foundTextboxColor = true;
        if (p == "font.size")
            foundFontSize = true;
    }
    EXPECT_TRUE(foundTextboxColor);
    EXPECT_TRUE(foundFontSize);
}

TEST_F(ConfigManagerTest,
       SerializeProperties)
{
    cm.apply("textbox.color", "255 0 0 255");

    std::ostringstream oss;
    cm.serialize(oss);

    std::string serialized = oss.str();
    EXPECT_NE(serialized.find("textbox.color"), std::string::npos);
}

TEST_F(ConfigManagerTest,
       GetPropertyDef)
{
    const auto *def = cm.getDef("textbox.color");
    EXPECT_NE(def, nullptr);
    EXPECT_STREQ(def->key, "textbox.color");
    EXPECT_EQ(def->type, PropType::Color);
}

TEST_F(ConfigManagerTest,
       PropertyValueParsing)
{
    PropertyValue pv;
    pv.type = PropType::Color;
    pv.serialized = "255 128 64 200";

    SDL_Color color = pv.asColor();
    EXPECT_EQ(color.r, 255);
    EXPECT_EQ(color.g, 128);
    EXPECT_EQ(color.b, 64);
    EXPECT_EQ(color.a, 200);
}

TEST_F(ConfigManagerTest,
       DimParsingPercentage)
{
    PropertyValue pv;
    pv.type = PropType::Dim;
    pv.serialized = "75%";

    Dim d = pv.asDim();
    EXPECT_TRUE(d.relative);
    EXPECT_EQ(d.value, 0.75f);
}

TEST_F(ConfigManagerTest,
       DimParsingPixels)
{
    PropertyValue pv;
    pv.type = PropType::Dim;
    pv.serialized = "600";

    Dim d = pv.asDim();
    EXPECT_FALSE(d.relative);
    EXPECT_EQ(d.value, 600.0f);
}
