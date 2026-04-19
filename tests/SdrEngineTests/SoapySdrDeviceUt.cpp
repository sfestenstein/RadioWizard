#include <gtest/gtest.h>
#include "SoapySdrDevice.h"
#include "SdrTypes.h"

#include <memory>

// ============================================================================
// Construction
// ============================================================================

TEST(SoapySdrDeviceTest, DefaultConstruction_NotOpen)
{
   SdrEngine::SoapySdrDevice device;
   EXPECT_FALSE(device.isOpen());
   EXPECT_FALSE(device.isStreaming());
}

TEST(SoapySdrDeviceTest, GetName_WhenNotOpen_ReturnsDefault)
{
   SdrEngine::SoapySdrDevice device;
   EXPECT_EQ(device.getName(), "SoapySDR (not open)");
}

// ============================================================================
// Configuration without hardware
// ============================================================================

TEST(SoapySdrDeviceTest, SetCenterFrequency_WhenNotOpen_ReturnsFalse)
{
   SdrEngine::SoapySdrDevice device;
   EXPECT_FALSE(device.setCenterFrequency(100'000'000));
}

TEST(SoapySdrDeviceTest, GetCenterFrequency_WhenNotOpen_ReturnsZero)
{
   SdrEngine::SoapySdrDevice device;
   EXPECT_EQ(device.getCenterFrequency(), 0u);
}

TEST(SoapySdrDeviceTest, SetSampleRate_WhenNotOpen_ReturnsFalse)
{
   SdrEngine::SoapySdrDevice device;
   EXPECT_FALSE(device.setSampleRate(2'400'000));
}

TEST(SoapySdrDeviceTest, GetSampleRate_WhenNotOpen_ReturnsZero)
{
   SdrEngine::SoapySdrDevice device;
   EXPECT_EQ(device.getSampleRate(), 0u);
}

TEST(SoapySdrDeviceTest, SetAutoGain_WhenNotOpen_ReturnsFalse)
{
   SdrEngine::SoapySdrDevice device;
   EXPECT_FALSE(device.setAutoGain(true));
}

TEST(SoapySdrDeviceTest, SetGain_WhenNotOpen_ReturnsFalse)
{
   SdrEngine::SoapySdrDevice device;
   EXPECT_FALSE(device.setGain(200));
}

TEST(SoapySdrDeviceTest, GetGain_WhenNotOpen_ReturnsZero)
{
   SdrEngine::SoapySdrDevice device;
   EXPECT_EQ(device.getGain(), 0);
}

TEST(SoapySdrDeviceTest, GetGainValues_WhenNotOpen_ReturnsEmpty)
{
   SdrEngine::SoapySdrDevice device;
   EXPECT_TRUE(device.getGainValues().empty());
}

TEST(SoapySdrDeviceTest, StartStreaming_WhenNotOpen_ReturnsFalse)
{
   SdrEngine::SoapySdrDevice device;
   EXPECT_FALSE(device.startStreaming([](const SdrEngine::IqSample*, std::size_t) {}));
}

// ============================================================================
// Device enumeration (may or may not find hardware)
// ============================================================================

TEST(SoapySdrDeviceTest, EnumerateDevices_DoesNotCrash)
{
   SdrEngine::SoapySdrDevice device;
   // Should not crash regardless of whether hardware is present.
   const auto devices = device.enumerateDevices();
   static_cast<void>(devices);
}

// ============================================================================
// Close / stop when not open
// ============================================================================

TEST(SoapySdrDeviceTest, Close_WhenNotOpen_DoesNotCrash)
{
   SdrEngine::SoapySdrDevice device;
   device.close();
   EXPECT_FALSE(device.isOpen());
}

TEST(SoapySdrDeviceTest, StopStreaming_WhenNotStreaming_DoesNotCrash)
{
   SdrEngine::SoapySdrDevice device;
   device.stopStreaming();
   EXPECT_FALSE(device.isStreaming());
}

// ============================================================================
// Integration with SdrEngine (uses ISdrDevice interface)
// ============================================================================

TEST(SoapySdrDeviceTest, CanBeUsedAsSdrDevice)
{
   auto device = std::make_unique<SdrEngine::SoapySdrDevice>();
   SdrEngine::ISdrDevice* iface = device.get();
   EXPECT_FALSE(iface->isOpen());
   EXPECT_EQ(iface->getName(), "SoapySDR (not open)");
}

// ============================================================================
// Open with invalid index
// ============================================================================

TEST(SoapySdrDeviceTest, Open_NegativeIndex_ReturnsFalse)
{
   SdrEngine::SoapySdrDevice device;
   EXPECT_FALSE(device.open(-1));
}
