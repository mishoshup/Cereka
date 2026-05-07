#include <gtest/gtest.h>
#include "text/markup_parser.hpp"

using namespace cereka::text;

TEST(MarkupParserTest, PlainTextUnchanged)
{
    auto segs = ParseMarkup("Hello world");
    ASSERT_EQ(segs.size(), 1);
    EXPECT_EQ(segs[0].text, "Hello world");
    EXPECT_FALSE(segs[0].style.bold);
}

TEST(MarkupParserTest, BoldTag)
{
    auto segs = ParseMarkup("Hello <b>world</b>!");
    ASSERT_EQ(segs.size(), 3);
    EXPECT_EQ(segs[0].text, "Hello ");
    EXPECT_FALSE(segs[0].style.bold);
    EXPECT_EQ(segs[1].text, "world");
    EXPECT_TRUE(segs[1].style.bold);
    EXPECT_EQ(segs[2].text, "!");
    EXPECT_FALSE(segs[2].style.bold);
}

TEST(MarkupParserTest, NestedTags)
{
    auto segs = ParseMarkup("<b>bold <i>bold+italic</i></b>");
    ASSERT_GE(segs.size(), 2u);
    EXPECT_TRUE(segs[0].style.bold);
    EXPECT_FALSE(segs[0].style.italic);
    EXPECT_TRUE(segs[1].style.bold);
    EXPECT_TRUE(segs[1].style.italic);
}

TEST(MarkupParserTest, ColorTag)
{
    auto segs = ParseMarkup("<color=#ff0000>red</color>");
    ASSERT_EQ(segs.size(), 1);
    EXPECT_EQ(segs[0].style.color.r, 255);
    EXPECT_EQ(segs[0].style.color.g, 0);
    EXPECT_EQ(segs[0].style.color.b, 0);
}

TEST(MarkupParserTest, AngleEscape)
{
    auto segs = ParseMarkup("2 << 3 >> 1");
    ASSERT_EQ(segs.size(), 1);
    EXPECT_EQ(segs[0].text, "2 < 3 > 1");
}

TEST(MarkupParserTest, UnclosedTagGraceful)
{
    auto segs = ParseMarkup("<b>unclosed");
    ASSERT_EQ(segs.size(), 1);
    EXPECT_TRUE(segs[0].style.bold);
    EXPECT_EQ(segs[0].text, "unclosed");
}

TEST(MarkupParserTest, MultipleTagsSameSegment)
{
    auto segs = ParseMarkup("<b><i>both</i></b>");
    ASSERT_EQ(segs.size(), 1);
    EXPECT_TRUE(segs[0].style.bold);
    EXPECT_TRUE(segs[0].style.italic);
}

TEST(MarkupParserTest, EmptyString)
{
    auto segs = ParseMarkup("");
    EXPECT_TRUE(segs.empty());
}

TEST(MarkupParserTest, AllStyleTags)
{
    auto segs = ParseMarkup("<b>B</b> <i>I</i> <u>U</u> <s>S</s>");
    ASSERT_EQ(segs.size(), 7);
    EXPECT_TRUE(segs[0].style.bold);
    EXPECT_FALSE(segs[2].style.bold);
    EXPECT_TRUE(segs[2].style.italic);
    EXPECT_TRUE(segs[4].style.underline);
    EXPECT_TRUE(segs[6].style.strikethrough);
}

TEST(MarkupParserTest, TrailingTextAfterClose)
{
    auto segs = ParseMarkup("<b>bold</b> trailing");
    ASSERT_EQ(segs.size(), 2);
    EXPECT_TRUE(segs[0].style.bold);
    EXPECT_EQ(segs[0].text, "bold");
    EXPECT_FALSE(segs[1].style.bold);
    EXPECT_EQ(segs[1].text, " trailing");
}

TEST(MarkupParserTest, NoSpuriousStyleOnPlainText)
{
    auto segs = ParseMarkup("a < b > c");
    ASSERT_EQ(segs.size(), 1);
    EXPECT_EQ(segs[0].text, "a < b > c");
}
