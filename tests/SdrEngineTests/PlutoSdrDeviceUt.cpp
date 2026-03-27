#include <gtest/gtest.h>
#include "PlutoSdrDevice.h"
#include "SdrTypes.h"

#include <memory>

// ============================================================================
// Construction
// ============================================================================

TEST(PlutoSdrDeviceTest, DefaultConstruction_NotOpen)
{
   SdrEngine::PlutoSdrDevice device;
   EXPECT_FALSE(device.isOpen());
   EXPECT_FALSE(device.isStreaming());
}

TEST(PlutoSdrDeviceTest, GetName_WhenNotOpen_ReturnsDefault)
{
   SdrEngine::PlutoSdrDevice device;
   EXPECT_EQ(device.getName(), "ADALM-PLUTO (not open)");
}

// ============================================================================
// Configuration without hardware
// ============================================================================

TEST(PlutoSdrDeviceTest, SetCenterFrequency_WhenNotOpen_ReturnsFalse)
{
   SdrEngine::PlutoSdrDevice device;
   EXPECT_FALSE(device.setCenterFrequency(100'000'000));
}

TEST(PlutoSdrDeviceTest, GetCenterFrequency_WhenNotOpen_ReturnsZero)
{
   SdrEngine::PlutoSdrDevice device;
   EXPECT_EQ(device.getCenterFrequency(), 0u);
}

TEST(PlutoSdrDeviceTest, SetSampleRate_WhenNotOpen_ReturnsFalse)
{
   SdrEngine::PlutoSdrDevice device;
   EXPECT_FALSE(device.setSampleRate(2'400'000));
}

TEST(PlutoSdrDeviceTest, GetSampleRate_WhenNotOpen_ReturnsZero)
{
   SdrEngine::PlutoSdrDevice device;
   EXPECT_EQ(device.getSampleRate(), 0u);
}

TEST(PlutoSdrDeviceTest, SetAutoGain_WhenNotOpen_ReturnsFalse)
{
   SdrEngine::PlutoSdrDevice device;
   EXPECT_FALSE(device.setAutoGain(true));
}

TEST(PlutoSdrDeviceTest, SetGain_WhenNotOpen_ReturnsFalse)
{
   SdrEngine::PlutoSdrDevice device;
   EXPECT_FALSE(device.setGain(200));
}

TEST(PlutoSdrDeviceTest, GetGain_WhenNotOpen_ReturnsZero)
{
   SdrEngine::PlutoSdrDevice device;
   EXPECT_EQ(device.getGain(), 0);
}

TEST(PlutoSdrDeviceTest, StartStreaming_WhenNotOpen_ReturnsFalse)
{
   SdrEngine::PlutoSdrDevice device;
   EXPECT_FALSE(device.startStreaming([](const uint8_t*, std::size_t) {}));
}

// ============================================================================
// Gain range
// ============================================================================

TEST(PlutoSdrDeviceTest, GetGainValues_ReturnsAD9361Range)
{
   SdrEngine::PlutoSdrDevice device;
   const auto gains = device.getGainValues();

   // AD9361: -1 dB to +73 dB in 1 dB steps → 75 entries in tenths-of-dB.
   EXPECT_EQ(gains.size(), 75u);
   EXPECT_EQ(gains.front(), -10);   // -1.0 dB in tenths
   EXPECT_EQ(gains.back(), 730);    // 73.0 dB in tenths
}

// ============================================================================
// Device enumeration (may or may not find hardware)
// ============================================================================

TEST(PlutoSdrDeviceTest, EnumerateDevices_DoesNotCrash)
{
   SdrEngine::PlutoSdrDevice device;
   // Should not crash regardless of whether hardware is present.
   const auto devices = device.enumerateDevices();
   // We can't assert a specific count since hardware may not be attached.
   static_cast<void>(devices);
}

// ============================================================================
// Close / stop when not open
// ============================================================================

TEST(PlutoSdrDeviceTest, Close_WhenNotOpen_DoesNotCrash)
{
   SdrEngine::PlutoSdrDevice device;
   device.close();   // Should be a no-op.
   EXPECT_FALSE(device.isOpen());
}

TEST(PlutoSdrDeviceTest, StopStreaming_WhenNotStreaming_DoesNotCrash)
{
   SdrEngine::PlutoSdrDevice device;
   device.stopStreaming();   // Should be a no-op.
   EXPECT_FALSE(device.isStreaming());
}

// ============================================================================
// Integration with SdrEngine (uses ISdrDevice interface)
// ============================================================================

TEST(PlutoSdrDeviceTest, CanBeUsedAsSdrDevice)
{
   // Verify that PlutoSdrDevice can be stored as unique_ptr<ISdrDevice>.
   auto device = std::make_unique<SdrEngine::PlutoSdrDevice>();
   SdrEngine::ISdrDevice* iface = device.get();
   EXPECT_FALSE(iface->isOpen());
   EXPECT_EQ(iface->getName(), "ADALM-PLUTO (not open)");
}

// ============================================================================
// Open with invalid index
// ============================================================================

TEST(PlutoSdrDeviceTest, Open_NegativeIndex_ReturnsFalse)
{
   SdrEngine::PlutoSdrDevice device;
   EXPECT_FALSE(device.open(-1));
}
