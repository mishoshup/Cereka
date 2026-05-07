#include <gtest/gtest.h>
#include "cereka_rollback_manager.hpp"

using namespace cereka;

TEST(RollbackManagerTest, DefaultCapacity)
{
    RollbackManager rm;
    EXPECT_EQ(rm.capacity(), 200);
    EXPECT_FALSE(rm.canRollback());
}

TEST(RollbackManagerTest, SetCapacity)
{
    RollbackManager rm;
    rm.setCapacity(50);
    EXPECT_EQ(rm.capacity(), 50);
}

TEST(RollbackManagerTest, ZeroCapacityDisables)
{
    RollbackManager rm;
    rm.setCapacity(0);
    EXPECT_FALSE(rm.canRollback());
    EXPECT_EQ(rm.count(), 0);
}

TEST(RollbackManagerTest, CountIncrementsOnCapture)
{
    RollbackManager rm(10);
    EXPECT_EQ(rm.count(), 0);
}

TEST(RollbackManagerTest, ClearResetsState)
{
    RollbackManager rm(10);
    rm.clear();
    EXPECT_EQ(rm.count(), 0);
    EXPECT_FALSE(rm.canRollback());
}

TEST(RollbackManagerTest, HistoryTextsEmptyInitially)
{
    RollbackManager rm(10);
    auto texts = rm.historyTexts();
    EXPECT_TRUE(texts.empty());
}

TEST(RollbackManagerTest, CanRollbackAfterCapture)
{
    RollbackManager rm(10);
    EXPECT_FALSE(rm.canRollback());
}
