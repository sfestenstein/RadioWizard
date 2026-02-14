#include <gtest/gtest.h>
#include "SdrEngine.h"
#include "RtlSdrDevice.h"
#include "SdrTypes.h"

#include <memory>

// ============================================================================
// Construction & defaults
// ============================================================================

TEST(SdrEngineTest, DefaultConstruction_NotRunning)
{
   SdrEngine::SdrEngine engine;
   EXPECT_FALSE(engine.isRunning());
   EXPECT_EQ(engine.getDevice(), nullptr);
}

TEST(SdrEngineTest, DefaultFftSize_Is2048)
{
   SdrEngine::SdrEngine engine;
   EXPECT_EQ(engine.getFftSize(), 2048);
}

TEST(SdrEngineTest, DefaultCenterFreq_Is100MHz)
{
   SdrEngine::SdrEngine engine;
   EXPECT_EQ(engine.getCenterFrequency(), 100'000'000u);
}

TEST(SdrEngineTest, DefaultSampleRate_Is2_4MSps)
{
   SdrEngine::SdrEngine engine;
   EXPECT_EQ(engine.getSampleRate(), 2'400'000u);
}

// ============================================================================
// Configuration (no device needed)
// ============================================================================

TEST(SdrEngineTest, SetCenterFrequency_StoresValue)
{
   SdrEngine::SdrEngine engine;
   engine.setCenterFrequency(433'920'000);
   EXPECT_EQ(engine.getCenterFrequency(), 433'920'000u);
}

TEST(SdrEngineTest, SetSampleRate_StoresValue)
{
   SdrEngine::SdrEngine engine;
   engine.setSampleRate(1'024'000);
   EXPECT_EQ(engine.getSampleRate(), 1'024'000u);
}

TEST(SdrEngineTest, SetFftSize_ChangesSize)
{
   SdrEngine::SdrEngine engine;
   engine.setFftSize(4096);
   EXPECT_EQ(engine.getFftSize(), 4096);
}

TEST(SdrEngineTest, SetWindowFunction_ChangesFunction)
{
   SdrEngine::SdrEngine engine;
   engine.setWindowFunction(SdrEngine::WindowFunction::Hanning);
   EXPECT_EQ(engine.getWindowFunction(), SdrEngine::WindowFunction::Hanning);
}

// ============================================================================
// Device management
// ============================================================================

TEST(SdrEngineTest, SetDevice_DeviceIsAccessible)
{
   SdrEngine::SdrEngine engine;
   engine.setDevice(std::make_unique<SdrEngine::RtlSdrDevice>());
   EXPECT_NE(engine.getDevice(), nullptr);
}

TEST(SdrEngineTest, StartWithoutDevice_ReturnsFalse)
{
   SdrEngine::SdrEngine engine;
   EXPECT_FALSE(engine.start());
   EXPECT_FALSE(engine.isRunning());
}

// ============================================================================
// DataHandler access
// ============================================================================

TEST(SdrEngineTest, SpectrumDataHandler_IsAccessible)
{
   SdrEngine::SdrEngine engine;
   auto& handler = engine.spectrumDataHandler();
   int id = handler.registerListener([](const std::shared_ptr<const SdrEngine::SpectrumData>&) {});
   EXPECT_GE(id, 0);
   handler.unregisterListener(id);
}

TEST(SdrEngineTest, IqDataHandler_IsAccessible)
{
   SdrEngine::SdrEngine engine;
   auto& handler = engine.iqDataHandler();
   int id = handler.registerListener([](const std::shared_ptr<const SdrEngine::IqBuffer>&) {});
   EXPECT_GE(id, 0);
   handler.unregisterListener(id);
}
