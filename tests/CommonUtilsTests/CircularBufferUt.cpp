#include <gtest/gtest.h>

#include "CircularBuffer.h"

#include <complex>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

using CommonUtils::CircularBuffer;

// ============================================================================
// Construction
// ============================================================================

TEST(CircularBufferTest, Constructor_ValidCapacity_CreatesEmptyBuffer)
{
   CircularBuffer<int> buf(10);
   EXPECT_EQ(buf.size(), 0U);
   EXPECT_EQ(buf.capacity(), 10U);
   EXPECT_TRUE(buf.empty());
   EXPECT_FALSE(buf.full());
}

TEST(CircularBufferTest, Constructor_CapacityOne_IsValid)
{
   CircularBuffer<int> buf(1);
   EXPECT_EQ(buf.capacity(), 1U);
   EXPECT_TRUE(buf.empty());
}

TEST(CircularBufferTest, Constructor_ZeroCapacity_Throws)
{
   EXPECT_THROW(CircularBuffer<int>(0), std::invalid_argument);
}

TEST(CircularBufferTest, Constructor_LargeCapacity_Succeeds)
{
   CircularBuffer<int> buf(100000);
   EXPECT_EQ(buf.capacity(), 100000U);
   EXPECT_TRUE(buf.empty());
}

// ============================================================================
// Push single element
// ============================================================================

TEST(CircularBufferTest, PushSingle_OneElement_SizeIsOne)
{
   CircularBuffer<int> buf(5);
   buf.push(42);
   EXPECT_EQ(buf.size(), 1U);
   EXPECT_FALSE(buf.empty());
   EXPECT_FALSE(buf.full());
}

TEST(CircularBufferTest, PushSingle_FillToCapacity_BecomesFull)
{
   CircularBuffer<int> buf(3);
   buf.push(1);
   buf.push(2);
   buf.push(3);
   EXPECT_EQ(buf.size(), 3U);
   EXPECT_TRUE(buf.full());
}

TEST(CircularBufferTest, PushSingle_OverwritesOldest_SizeStaysAtCapacity)
{
   CircularBuffer<int> buf(3);
   buf.push(1);
   buf.push(2);
   buf.push(3);
   buf.push(4); // overwrites 1
   EXPECT_EQ(buf.size(), 3U);
   EXPECT_TRUE(buf.full());
}

TEST(CircularBufferTest, PushSingle_CapacityOne_AlwaysOverwrites)
{
   CircularBuffer<int> buf(1);
   buf.push(10);
   EXPECT_EQ(buf.back(), 10);
   EXPECT_TRUE(buf.full());

   buf.push(20);
   EXPECT_EQ(buf.back(), 20);
   EXPECT_EQ(buf.size(), 1U);
}

// ============================================================================
// Push range (pointer + count)
// ============================================================================

TEST(CircularBufferTest, PushRange_ArrayOfElements_AllStored)
{
   CircularBuffer<int> buf(10);
   int data[] = {10, 20, 30, 40, 50};
   buf.push(data, 5);
   EXPECT_EQ(buf.size(), 5U);

   auto vec = buf.toVector();
   EXPECT_EQ(vec, (std::vector<int>{10, 20, 30, 40, 50}));
}

TEST(CircularBufferTest, PushRange_MoreThanCapacity_OnlyNewestKept)
{
   CircularBuffer<int> buf(3);
   int data[] = {1, 2, 3, 4, 5};
   buf.push(data, 5);
   EXPECT_EQ(buf.size(), 3U);
   EXPECT_TRUE(buf.full());

   auto vec = buf.toVector();
   EXPECT_EQ(vec, (std::vector<int>{3, 4, 5}));
}

TEST(CircularBufferTest, PushRange_ZeroCount_NoChange)
{
   CircularBuffer<int> buf(5);
   int data[] = {1};
   buf.push(data, 0);
   EXPECT_TRUE(buf.empty());
}

// ============================================================================
// Push vector
// ============================================================================

TEST(CircularBufferTest, PushVector_AllElementsStored)
{
   CircularBuffer<int> buf(10);
   std::vector<int> data{100, 200, 300};
   buf.push(data);
   EXPECT_EQ(buf.size(), 3U);

   auto vec = buf.toVector();
   EXPECT_EQ(vec, (std::vector<int>{100, 200, 300}));
}

TEST(CircularBufferTest, PushVector_EmptyVector_NoChange)
{
   CircularBuffer<int> buf(5);
   buf.push(std::vector<int>{});
   EXPECT_TRUE(buf.empty());
}

// ============================================================================
// operator[]
// ============================================================================

TEST(CircularBufferTest, SubscriptOperator_BeforeWrap_ReturnsChronological)
{
   CircularBuffer<int> buf(5);
   buf.push(10);
   buf.push(20);
   buf.push(30);
   EXPECT_EQ(buf[0], 10); // oldest
   EXPECT_EQ(buf[1], 20);
   EXPECT_EQ(buf[2], 30); // newest
}

TEST(CircularBufferTest, SubscriptOperator_AfterWrap_ReturnsChronological)
{
   CircularBuffer<int> buf(3);
   buf.push(1);
   buf.push(2);
   buf.push(3);
   buf.push(4); // overwrites 1
   buf.push(5); // overwrites 2

   EXPECT_EQ(buf[0], 3); // oldest surviving
   EXPECT_EQ(buf[1], 4);
   EXPECT_EQ(buf[2], 5); // newest
}

TEST(CircularBufferTest, SubscriptOperator_AfterMultipleWraps_CorrectOrder)
{
   CircularBuffer<int> buf(3);
   for (int i = 1; i <= 10; ++i)
   {
      buf.push(i);
   }
   // Should contain 8, 9, 10
   EXPECT_EQ(buf[0], 8);
   EXPECT_EQ(buf[1], 9);
   EXPECT_EQ(buf[2], 10);
}

// ============================================================================
// at()
// ============================================================================

TEST(CircularBufferTest, At_ValidIndex_ReturnsSameAsSubscript)
{
   CircularBuffer<int> buf(5);
   buf.push(10);
   buf.push(20);
   buf.push(30);
   EXPECT_EQ(buf.at(0), buf[0]);
   EXPECT_EQ(buf.at(1), buf[1]);
   EXPECT_EQ(buf.at(2), buf[2]);
}

TEST(CircularBufferTest, At_IndexEqualToSize_Throws)
{
   CircularBuffer<int> buf(5);
   buf.push(1);
   buf.push(2);
   EXPECT_THROW(buf.at(2), std::out_of_range);
}

TEST(CircularBufferTest, At_IndexBeyondSize_Throws)
{
   CircularBuffer<int> buf(5);
   buf.push(1);
   EXPECT_THROW(buf.at(100), std::out_of_range);
}

TEST(CircularBufferTest, At_EmptyBuffer_Throws)
{
   CircularBuffer<int> buf(5);
   EXPECT_THROW(buf.at(0), std::out_of_range);
}

// ============================================================================
// back()
// ============================================================================

TEST(CircularBufferTest, Back_AfterSinglePush_ReturnsThatElement)
{
   CircularBuffer<int> buf(5);
   buf.push(42);
   EXPECT_EQ(buf.back(), 42);
}

TEST(CircularBufferTest, Back_AfterMultiplePushes_ReturnsMostRecent)
{
   CircularBuffer<int> buf(5);
   buf.push(1);
   buf.push(2);
   buf.push(3);
   EXPECT_EQ(buf.back(), 3);
}

TEST(CircularBufferTest, Back_AfterWrap_ReturnsMostRecent)
{
   CircularBuffer<int> buf(3);
   for (int i = 1; i <= 7; ++i)
   {
      buf.push(i);
   }
   EXPECT_EQ(buf.back(), 7);
}

TEST(CircularBufferTest, Back_EmptyBuffer_Throws)
{
   CircularBuffer<int> buf(5);
   EXPECT_THROW(buf.back(), std::out_of_range);
}

// ============================================================================
// toVector()
// ============================================================================

TEST(CircularBufferTest, ToVector_EmptyBuffer_ReturnsEmpty)
{
   CircularBuffer<int> buf(5);
   EXPECT_TRUE(buf.toVector().empty());
}

TEST(CircularBufferTest, ToVector_PartiallyFilled_ReturnsInOrder)
{
   CircularBuffer<int> buf(5);
   buf.push(10);
   buf.push(20);
   auto vec = buf.toVector();
   EXPECT_EQ(vec, (std::vector<int>{10, 20}));
}

TEST(CircularBufferTest, ToVector_FullBuffer_ReturnsChronological)
{
   CircularBuffer<int> buf(3);
   buf.push(1);
   buf.push(2);
   buf.push(3);
   auto vec = buf.toVector();
   EXPECT_EQ(vec, (std::vector<int>{1, 2, 3}));
}

TEST(CircularBufferTest, ToVector_AfterWrap_ReturnsChronological)
{
   CircularBuffer<int> buf(3);
   buf.push(1);
   buf.push(2);
   buf.push(3);
   buf.push(4);
   buf.push(5);
   auto vec = buf.toVector();
   EXPECT_EQ(vec, (std::vector<int>{3, 4, 5}));
}

// ============================================================================
// size() / capacity() / full() / empty()
// ============================================================================

TEST(CircularBufferTest, SizeGrowsUntilCapacity)
{
   CircularBuffer<int> buf(4);
   EXPECT_EQ(buf.size(), 0U);
   buf.push(1);
   EXPECT_EQ(buf.size(), 1U);
   buf.push(2);
   EXPECT_EQ(buf.size(), 2U);
   buf.push(3);
   EXPECT_EQ(buf.size(), 3U);
   buf.push(4);
   EXPECT_EQ(buf.size(), 4U);
   buf.push(5); // wraps
   EXPECT_EQ(buf.size(), 4U);
}

TEST(CircularBufferTest, CapacityNeverChanges)
{
   CircularBuffer<int> buf(4);
   EXPECT_EQ(buf.capacity(), 4U);
   for (int i = 0; i < 100; ++i)
   {
      buf.push(i);
   }
   EXPECT_EQ(buf.capacity(), 4U);
}

TEST(CircularBufferTest, FullAndEmpty_AreMutuallyExclusive)
{
   CircularBuffer<int> buf(2);
   EXPECT_TRUE(buf.empty());
   EXPECT_FALSE(buf.full());

   buf.push(1);
   EXPECT_FALSE(buf.empty());
   EXPECT_FALSE(buf.full());

   buf.push(2);
   EXPECT_FALSE(buf.empty());
   EXPECT_TRUE(buf.full());
}

// ============================================================================
// clear()
// ============================================================================

TEST(CircularBufferTest, Clear_ResetsToEmpty)
{
   CircularBuffer<int> buf(5);
   buf.push(1);
   buf.push(2);
   buf.push(3);
   buf.clear();
   EXPECT_TRUE(buf.empty());
   EXPECT_EQ(buf.size(), 0U);
   EXPECT_FALSE(buf.full());
   EXPECT_EQ(buf.capacity(), 5U);
}

TEST(CircularBufferTest, Clear_CanPushAgainAfterClear)
{
   CircularBuffer<int> buf(3);
   buf.push(1);
   buf.push(2);
   buf.push(3);
   buf.clear();

   buf.push(10);
   buf.push(20);
   EXPECT_EQ(buf.size(), 2U);
   EXPECT_EQ(buf[0], 10);
   EXPECT_EQ(buf[1], 20);
}

TEST(CircularBufferTest, Clear_AfterWrap_WorksCorrectly)
{
   CircularBuffer<int> buf(3);
   for (int i = 0; i < 10; ++i)
   {
      buf.push(i);
   }
   buf.clear();
   EXPECT_TRUE(buf.empty());

   buf.push(100);
   EXPECT_EQ(buf.size(), 1U);
   EXPECT_EQ(buf.back(), 100);
   EXPECT_EQ(buf[0], 100);
}

TEST(CircularBufferTest, Clear_EmptyBuffer_IsNoOp)
{
   CircularBuffer<int> buf(5);
   buf.clear();
   EXPECT_TRUE(buf.empty());
   EXPECT_EQ(buf.capacity(), 5U);
}

// ============================================================================
// Wrap-around correctness (stress)
// ============================================================================

TEST(CircularBufferTest, WrapAround_ExactlyTwoFullCycles_CorrectContents)
{
   constexpr std::size_t CAP = 5;
   CircularBuffer<size_t> buf(CAP);

   // Push exactly 2 * capacity elements (0..9)
   for (size_t i = 0; i < static_cast<int>(CAP * 2); ++i)
   {
      buf.push(i);
   }
   EXPECT_EQ(buf.size(), CAP);
   auto vec = buf.toVector();
   EXPECT_EQ(vec, (std::vector<size_t>{5, 6, 7, 8, 9}));
}

TEST(CircularBufferTest, WrapAround_NonMultipleOverflow_CorrectContents)
{
   constexpr std::size_t CAP = 4;
   CircularBuffer<int> buf(CAP);

   // Push 7 elements into capacity-4 buffer
   for (int i = 1; i <= 7; ++i)
   {
      buf.push(i);
   }
   auto vec = buf.toVector();
   EXPECT_EQ(vec, (std::vector<int>{4, 5, 6, 7}));
}

// ============================================================================
// Template with different types
// ============================================================================

TEST(CircularBufferTest, StringType_WorksCorrectly)
{
   CircularBuffer<std::string> buf(3);
   buf.push("alpha");
   buf.push("beta");
   buf.push("gamma");
   buf.push("delta"); // overwrites "alpha"

   EXPECT_EQ(buf[0], "beta");
   EXPECT_EQ(buf[1], "gamma");
   EXPECT_EQ(buf[2], "delta");
   EXPECT_EQ(buf.back(), "delta");
}

TEST(CircularBufferTest, DoubleType_WorksCorrectly)
{
   CircularBuffer<double> buf(2);
   buf.push(3.14);
   buf.push(2.72);
   EXPECT_DOUBLE_EQ(buf[0], 3.14);
   EXPECT_DOUBLE_EQ(buf[1], 2.72);
   EXPECT_DOUBLE_EQ(buf.back(), 2.72);
}

TEST(CircularBufferTest, ComplexFloatType_WorksCorrectly)
{
   CircularBuffer<std::complex<float>> buf(3);
   buf.push(std::complex<float>{1.0F, 2.0F});
   buf.push(std::complex<float>{3.0F, 4.0F});
   EXPECT_EQ(buf[0], (std::complex<float>{1.0F, 2.0F}));
   EXPECT_EQ(buf.back(), (std::complex<float>{3.0F, 4.0F}));
}

TEST(CircularBufferTest, VectorOfFloats_WorksCorrectly)
{
   CircularBuffer<std::vector<float>> buf(2);
   buf.push(std::vector<float>{1.0F, 2.0F, 3.0F});
   buf.push(std::vector<float>{4.0F, 5.0F, 6.0F});

   EXPECT_EQ(buf[0], (std::vector<float>{1.0F, 2.0F, 3.0F}));
   EXPECT_EQ(buf[1], (std::vector<float>{4.0F, 5.0F, 6.0F}));

   buf.push(std::vector<float>{7.0F}); // overwrites first
   EXPECT_EQ(buf[0], (std::vector<float>{4.0F, 5.0F, 6.0F}));
   EXPECT_EQ(buf[1], (std::vector<float>{7.0F}));
}

// ============================================================================
// Edge cases
// ============================================================================

TEST(CircularBufferTest, BackAndSubscript_AreConsistent)
{
   CircularBuffer<int> buf(4);
   for (int i = 1; i <= 20; ++i)
   {
      buf.push(i);
      EXPECT_EQ(buf.back(), i);
      EXPECT_EQ(buf[buf.size() - 1], i);
   }
}

TEST(CircularBufferTest, ToVector_AfterClearAndRefill_CorrectOrder)
{
   CircularBuffer<int> buf(3);
   buf.push(1);
   buf.push(2);
   buf.push(3);
   buf.clear();
   buf.push(10);
   buf.push(20);

   auto vec = buf.toVector();
   EXPECT_EQ(vec, (std::vector<int>{10, 20}));
}

TEST(CircularBufferTest, PushVector_ExactlyCapacity_FillsCompletely)
{
   CircularBuffer<int> buf(4);
   buf.push(std::vector<int>{1, 2, 3, 4});
   EXPECT_TRUE(buf.full());
   auto vec = buf.toVector();
   EXPECT_EQ(vec, (std::vector<int>{1, 2, 3, 4}));
}

TEST(CircularBufferTest, MultipleClearCycles_StayConsistent)
{
   CircularBuffer<int> buf(3);
   for (int cycle = 0; cycle < 5; ++cycle)
   {
      buf.clear();
      for (int i = 0; i < 5; ++i)
      {
         buf.push((cycle * 10) + i);
      }
      EXPECT_EQ(buf.size(), 3U);
      EXPECT_TRUE(buf.full());
      // Should contain the last 3 pushed: cycle*10+2, +3, +4
      EXPECT_EQ(buf[0], (cycle * 10) + 2);
      EXPECT_EQ(buf[1], (cycle * 10) + 3);
      EXPECT_EQ(buf[2], (cycle * 10) + 4);
   }
}
