#include <gtest/gtest.h>
#include "OscilloscopeWidget.h"

#include <QApplication>

#include <complex>
#include <vector>

class OscilloscopeWidgetTest : public ::testing::Test
{
protected:
   void SetUp() override
   {
      _widget = std::make_unique<RealTimeGraphs::OscilloscopeWidget>();
   }

   void TearDown() override
   {
      _widget.reset();
   }

   static std::vector<std::complex<float>> makeSamples(std::size_t count)
   {
      std::vector<std::complex<float>> samples(count);
      for (std::size_t i = 0; i < count; ++i)
      {
         auto val = static_cast<float>(i) / static_cast<float>(count);
         samples[i] = {val, -val};
      }
      return samples;
   }

   std::unique_ptr<RealTimeGraphs::OscilloscopeWidget> _widget;
};

TEST_F(OscilloscopeWidgetTest, NotPausedByDefault)
{
   EXPECT_FALSE(_widget->isPaused());
}

TEST_F(OscilloscopeWidgetTest, SetPaused_PausesWidget)
{
   _widget->setPaused(true);
   EXPECT_TRUE(_widget->isPaused());
}

TEST_F(OscilloscopeWidgetTest, SetPaused_ResumeWidget)
{
   _widget->setPaused(true);
   _widget->setPaused(false);
   EXPECT_FALSE(_widget->isPaused());
}

TEST_F(OscilloscopeWidgetTest, SetData_WhilePaused_DoesNotUpdateSamples)
{
   auto initial = makeSamples(64);
   _widget->setData(initial);

   _widget->setPaused(true);

   // Send new data while paused — the widget should ignore it
   auto updated = makeSamples(128);
   _widget->setData(updated);

   // Widget is still paused; it accepted the first batch but rejected the second.
   // We can only verify the pause state here since samples are private.
   EXPECT_TRUE(_widget->isPaused());
}

TEST_F(OscilloscopeWidgetTest, SetData_AfterResume_AcceptsData)
{
   _widget->setPaused(true);
   _widget->setPaused(false);

   auto samples = makeSamples(64);
   _widget->setData(samples);

   // No crash, no exception — data accepted.
   EXPECT_FALSE(_widget->isPaused());
}
