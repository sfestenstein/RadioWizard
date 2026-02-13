# Project Design

This document describes the architecture and design decisions of the RadioWizard project.

## Overview

RadioWizard is a C++20 application for Software Defined Radio (SDR) control, spectrum and I/Q data observation, signal isolation, and signal demodulation. It interfaces with SDR hardware to capture RF data, visualizes signals in real time, and provides a distributed processing architecture for streaming high-bandwidth I/Q samples between components.

## Architecture

### Component Diagram

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                              Applications                                        │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────────┐       │
│  │ ZyrePublisher   │  │ ZyreSubscriber  │  │ HighBandwidth Pub/Sub       │       │
│  │ (Zyre P2P)      │  │ (Zyre P2P)      │  │ (UDP Multicast)             │       │
│  └────────┬────────┘  └────────┬────────┘  └──────────────┬──────────────┘       │
│           │                    │                          │                       │
│  ┌─────────────────────────────────────┐  ┌────────────────────────────────┐      │
│  │ RealTimeGraphsTest (Qt 6)            │  │ Vita49 File / Perf / RoundTrip│      │
│  └───────────────┬─────────────────────┘  └──────────────┬─────────────────┘      │
│                      │                                   │                       │
├──────────────────────┼───────────────────────────────────┼───────────────────────┤
│                      │         Libraries                 │                       │
│  ┌───────────────────────────────────────────────────────────────────────────┐   │
│  │                         PubSub Library                                     │   │
│  │  ┌───────────────┐  ┌───────────────┐  ┌─────────────────────────────┐    │   │
│  │  │   ZyreNode    │  │ ZyrePublisher │  │ HighBandwidthPublisher      │    │   │
│  │  │   (base)      │  │ ZyreSubscriber│  │ HighBandwidthSubscriber     │    │   │
│  │  └───────────────┘  └───────────────┘  └─────────────────────────────┘    │   │
│  └───────────────────────────────────────────────────────────────────────────┘   │
│  ┌───────────────────────────────────────────────────────────────────────────┐   │
│  │              RealTimeGraphs Library (Qt 6)                                  │   │
│  │  ┌─────────────────┐ ┌───────────────────┐ ┌───────────────────────────┐  │   │
│  │  │ SpectrumWidget  │ │ WaterfallWidget   │ │ ConstellationWidget       │  │   │
│  │  └─────────────────┘ └───────────────────┘ └───────────────────────────┘  │   │
│  └───────────────────────────────────────────────────────────────────────────┘   │
│  ┌───────────────────────────────────────────────────────────────────────────┐   │
│  │                    Vita49_2 Library                                          │   │
│  │  ┌─────────────────┐ ┌───────────────────┐ ┌───────────────────────────┐  │   │
│  │  │  PacketHeader   │ │ SignalDataPacket  │ │ ContextPacket             │  │   │
│  │  │  Vita49Types    │ │ Vita49Codec       │ │ ByteSwap                  │  │   │
│  │  └─────────────────┘ └───────────────────┘ └───────────────────────────┘  │   │
│  └───────────────────────────────────────────────────────────────────────────┘   │
│  ┌─────────────────┐                    ┌─────────────────┐                      │
│  │   Proto Lib     │                    │  CommonUtils    │                      │
│  │   (protobuf)    │                    │  (utilities)    │                      │
│  └─────────────────┘                    └─────────────────┘                      │
│                                                                                  │
├──────────────────────────────────────────────────────────────────────────────────┤
│                         External Dependencies                                    │
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐  │
│  │ spdlog │ │protobuf│ │ ZeroMQ │ │ cppzmq │ │  CZMQ  │ │  Zyre  │ │  Qt 6  │  │
│  └────────┘ └────────┘ └────────┘ └────────┘ └────────┘ └────────┘ └────────┘  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### Libraries

#### CommonUtils Library (`src/libs/CommonUtils/`)

The CommonUtils library provides reusable components for common tasks:

- **GeneralLogger**: An async logging wrapper around spdlog providing:
  - Dual-logger system (general + trace)
  - Convenience macros (GPCRIT, GPERROR, GPWARN, GPINFO, GPDEBUG, GPTRACE)
  - Async logging with configurable queue size
  - Thread-safe initialization

- **Timer**: A basic timer class:
  - Periodic and single-shot modes
  - Callback-based design
  - Thread-safe start/stop operations
  - Millisecond precision

- **SnoozableTimer**: An extended timer with snooze capability:
  - Inherits from Timer
  - Snooze functionality to extend timeout
  - Useful for implementing watchdog patterns

- **DataHandler**: Data handling utilities (header-only):
  - Template-based data processing
  - Flexible data transformation support

#### PubSub Library (`src/libs/PubSub/`)

The PubSub library provides two messaging patterns:

**Zyre-based Messaging** (peer-to-peer discovery):

- **ZyreNode**: Base class for Zyre nodes:
  - Automatic peer discovery via UDP beaconing
  - Node lifecycle management (start/stop)
  - Thread-safe operation

- **ZyrePublisher**: Publishes messages via Zyre:
  - Inherits from ZyreNode
  - Publishes protobuf messages to topics
  - Automatic peer discovery

- **ZyreSubscriber**: Subscribes to messages via Zyre:
  - Inherits from ZyreNode
  - Topic-based subscription with callbacks
  - Background receive thread

**High-Bandwidth Messaging** (UDP multicast):

- **HighBandwidthPublisher**: Fast UDP multicast publisher:
  - Raw UDP multicast for minimal overhead
  - Automatic message fragmentation for large payloads
  - Fire-and-forget semantics (unreliable but fast)
  - Ideal for sensor data, telemetry, video frames

- **HighBandwidthSubscriber**: Fast UDP multicast subscriber:
  - Joins multicast group for receiving
  - Automatic fragment reassembly
  - Configurable reassembly timeout
  - Thread-safe subscription (can subscribe before or after start)

#### RealTimeGraphs Library (`src/libs/RealTimeGraphs/`)

The RealTimeGraphs library provides custom QPainter-based widgets for real-time
signal visualization.

- **SpectrumWidget**: Real-time spectrum (frequency-domain) display
- **WaterfallWidget**: Waterfall / spectrogram display
- **ConstellationWidget**: IQ constellation diagram
- **ColorMap**: Configurable color-map utilities used by the widgets

Dependencies: Qt6::Core, Qt6::Gui, Qt6::Widgets, Qt6::OpenGLWidgets, spdlog,
CommonUtils.

CMake enables `AUTOMOC` on this target so that Qt signals/slots are processed
automatically.

#### Proto Library (`src/libs/proto/`)

The protocol buffer library compiles `.proto` files from `src/libs/proto/proto-messages/` into C++ classes:

- **sensor_data.proto**: Sensor readings with metadata, location, and batching
- **commands.proto**: Command/response pattern for SDR control RPC
- **configuration.proto**: Application and SDR configuration structures

#### Vita49_2 Library (`src/libs/Vita49_2/`)

The Vita49_2 library implements the VITA 49.2 standard for signal data and context
packet encoding/decoding, enabling interoperability with other SDR and signal
processing systems:

- **PacketHeader**: VITA 49 packet header parsing and construction
- **SignalDataPacket**: Encode and decode signal (I/Q) data packets
- **ContextPacket**: Encode and decode context packets carrying metadata (frequency, bandwidth, gain, etc.)
- **Vita49Codec**: High-level codec for reading/writing VITA 49 packet streams to files
- **Vita49Types**: Type definitions and constants for the VITA 49.2 standard
- **ByteSwap**: Endian conversion utilities for network byte order compliance

### Applications (`src/TestApps/`)

#### ZyrePublisher (`src/TestApps/ZyrePublisherTest.cpp`)

Demonstrates:
- Zyre peer-to-peer publishing
- Protocol buffer serialization (SensorReading, SensorDataBatch, Command)
- Periodic message publishing
- GeneralLogger usage

#### ZyreSubscriber (`src/TestApps/ZyreSubscriberTest.cpp`)

Demonstrates:
- Zyre peer-to-peer subscription
- Protocol buffer deserialization
- Topic-based message handling
- Formatted logging with spdlog

#### HighBandwidthPublisher (`src/TestApps/HighBandwidthPublisherTester.cpp`)

Demonstrates:
- High-frequency UDP multicast publishing
- Large message fragmentation
- I/Q and sensor data streaming

#### HighBandwidthSubscriber (`src/TestApps/HighBandwidthSubscriberTester.cpp`)

Demonstrates:
- UDP multicast subscription
- Fragment reassembly
- High-throughput I/Q data reception

#### RealTimeGraphsTest (`src/TestApps/RealTimeGraphsTest.cpp`)

Demonstrates:
- Interactive Qt application using the RealTimeGraphs widget library
- Spectrum, waterfall, and constellation displays for SDR data

#### Vita49FileCodec (`src/TestApps/Vita49FileCodec.cpp`)

Demonstrates:
- Encoding and decoding VITA 49.2 packets to/from files
- Round-trip validation of signal data and context packets

#### Vita49PerfBenchmark (`src/TestApps/Vita49PerfBenchmark.cpp`)

Demonstrates:
- Performance benchmarking of VITA 49 packet encode/decode
- Throughput measurement for real-time processing viability

#### Vita49RoundTripTest (`src/TestApps/Vita49RoundTripTest.cpp`)

Demonstrates:
- End-to-end round-trip validation of VITA 49 packet construction and parsing
- Data integrity verification

## Design Decisions

### Build System

**CMake** was chosen as the build system because:
- Industry standard for C++ projects
- Excellent IDE integration
- Cross-platform support
- Modern features (presets, toolchain files)

**Conan 2.0** was chosen for package management because:
- Mature ecosystem with many packages
- First-class CMake integration
- Cross-platform support
- Binary package caching

### Code Quality

**clang-format** ensures consistent code style:
- 3-space indentation
- Allman brace style (braces on new line)
- 100-character line limit

**clang-tidy** provides static analysis:
- Modern C++ best practices
- Bug detection
- Performance suggestions
- Naming conventions

### Testing Strategy

**Google Test** was chosen because:
- Widely used in industry
- Feature-rich (fixtures, mocking, parameterized tests)
- Good IDE integration
- Clear test output

Test organization:
- One test file per source file
- Tests mirror the source structure
- Fixtures for common setup/teardown

### Sanitizers

Address Sanitizer (ASan) and Undefined Behavior Sanitizer (UBSan) are enabled in debug builds to catch:
- Memory leaks
- Buffer overflows
- Use-after-free
- Undefined behavior

### Code Coverage

Coverage is collected using gcov/lcov:
- Line and branch coverage
- HTML report generation
- CI integration with Codecov

## Future Considerations

Areas for potential enhancement:

1. **SDR Hardware Drivers**: Integrate SoapySDR or direct device libraries (RTL-SDR, USRP, HackRF) for hardware control
2. **Signal Demodulation**: Implement demodulation chains (AM, FM, SSB, PSK, QAM, etc.)
3. **Signal Isolation**: Automatic signal detection and isolation from wideband captures
4. **DSP Pipeline**: Build a modular DSP processing pipeline (filters, decimation, channelization)
5. **Benchmarking**: Add Google Benchmark for DSP performance testing
6. **Documentation**: Add Doxygen for API documentation
7. **Packaging**: Add CPack for installers/packages
8. **Embedded Targets**: Add toolchain files for cross-compilation to SDR platforms
9. **Fuzzing**: Add libFuzzer for fuzz testing of packet parsers
