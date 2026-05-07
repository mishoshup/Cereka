#include <gtest/gtest.h>
#include "scene_graph.hpp"

using namespace cereka;

TEST(SceneGraphTest, CreateAndFindNode)
{
    SceneGraph sg;
    auto *n = sg.createNode("test");
    ASSERT_NE(n, nullptr);
    EXPECT_EQ(n->id, "test");
    EXPECT_TRUE(sg.hasNode("test"));
    EXPECT_EQ(sg.findNode("test"), n);
}

TEST(SceneGraphTest, RemoveNode)
{
    SceneGraph sg;
    sg.createNode("test");
    EXPECT_TRUE(sg.hasNode("test"));
    sg.removeNode("test");
    EXPECT_FALSE(sg.hasNode("test"));
    EXPECT_EQ(sg.findNode("test"), nullptr);
}

TEST(SceneGraphTest, CreateDuplicateIdReturnsSameNode)
{
    SceneGraph sg;
    auto *a = sg.createNode("dup");
    auto *b = sg.createNode("dup");
    EXPECT_EQ(a, b);
}

TEST(SceneGraphTest, RemoveNonexistentNodeDoesNothing)
{
    SceneGraph sg;
    EXPECT_NO_THROW(sg.removeNode("nonexistent"));
}

TEST(SceneGraphTest, NodeHasDefaults)
{
    SceneGraph sg;
    auto *n = sg.createNode("defaults");
    EXPECT_FLOAT_EQ(n->local.x, 0.5f);
    EXPECT_FLOAT_EQ(n->local.y, 0.5f);
    EXPECT_FLOAT_EQ(n->local.scaleX, 1.0f);
    EXPECT_FLOAT_EQ(n->local.scaleY, 1.0f);
    EXPECT_FLOAT_EQ(n->local.rotationDeg, 0.0f);
    EXPECT_FLOAT_EQ(n->local.opacity, 1.0f);
    EXPECT_TRUE(n->visible);
}

TEST(SceneGraphTest, WorldEqualsLocalForRootNode)
{
    SceneGraph sg;
    auto *n = sg.createNode("root");
    n->local.scaleX = 2.0f;
    n->local.opacity = 0.5f;

    sg.updateTransforms();

    EXPECT_FLOAT_EQ(n->world.scaleX, 2.0f);
    EXPECT_FLOAT_EQ(n->world.opacity, 0.5f);
}

TEST(SceneGraphTest, SetTransformParsesCorrectly)
{
    SceneGraph sg;
    sg.setTransform("myNode", "pos(0.2,0.8) scale(1.5) rotate(45) opacity(0.5)");
    auto *n = sg.findNode("myNode");
    ASSERT_NE(n, nullptr);
    EXPECT_FLOAT_EQ(n->local.x, 0.2f);
    EXPECT_FLOAT_EQ(n->local.y, 0.8f);
    EXPECT_FLOAT_EQ(n->local.scaleX, 1.5f);
    EXPECT_FLOAT_EQ(n->local.rotationDeg, 45.0f);
    EXPECT_FLOAT_EQ(n->local.opacity, 0.5f);
}

TEST(SceneGraphTest, SetTransformPartialUpdate)
{
    SceneGraph sg;
    sg.createNode("n");
    sg.setTransform("n", "scale(3.0)");
    auto *n = sg.findNode("n");
    ASSERT_NE(n, nullptr);
    EXPECT_FLOAT_EQ(n->local.x, 0.5f);
    EXPECT_FLOAT_EQ(n->local.scaleX, 3.0f);
}

TEST(SceneGraphTest, SetTransformIgnoresUnknownKeyword)
{
    SceneGraph sg;
    EXPECT_NO_THROW(sg.setTransform("unk", "unknownkeyword(1) pos(0.1,0.2)"));
    auto *n = sg.findNode("unk");
    ASSERT_NE(n, nullptr);
    EXPECT_FLOAT_EQ(n->local.x, 0.1f);
}

TEST(SceneGraphTest, VisitOrder)
{
    SceneGraph sg;
    sg.createNode("first");
    sg.createNode("second");

    std::vector<std::string> visited;
    sg.visit([&](const SceneNode &node) { visited.push_back(node.id); });

    ASSERT_EQ(visited.size(), 2u);
    EXPECT_EQ(visited[0], "first");
    EXPECT_EQ(visited[1], "second");
}

TEST(SceneGraphTest, SetTransformAutoCreatesNode)
{
    SceneGraph sg;
    EXPECT_FALSE(sg.hasNode("autoNode"));
    sg.setTransform("autoNode", "pos(0.1,0.9)");
    EXPECT_TRUE(sg.hasNode("autoNode"));
    auto *n = sg.findNode("autoNode");
    ASSERT_NE(n, nullptr);
    EXPECT_FLOAT_EQ(n->local.x, 0.1f);
    EXPECT_FLOAT_EQ(n->local.y, 0.9f);
}

TEST(SceneGraphTest, ClearResetsAllNodes)
{
    SceneGraph sg;
    sg.createNode("a");
    sg.createNode("b");
    EXPECT_TRUE(sg.hasNode("a"));
    EXPECT_TRUE(sg.hasNode("b"));
    sg.Clear();
    EXPECT_FALSE(sg.hasNode("a"));
    EXPECT_FALSE(sg.hasNode("b"));
}
