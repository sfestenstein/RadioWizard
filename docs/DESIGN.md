# Project Design

This document describes the architecture and design decisions of the StarterCpp project.

## Overview

StarterCpp is designed as a production-ready C++ project template that demonstrates modern C++ best practices, build system configuration, and software engineering patterns.

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
│  ┌─────────────────────────────────────┐                                      │
│  │ RealTimeGraphsTest (Qt 6)            │                                      │
│  └───────────────┬─────────────────────┘                                      │
│                      │                                                           │
├──────────────────────┼───────────────────────────────────────────────────────────┤
│                      │         Libraries                                         │
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
- **commands.proto**: Command/response pattern for RPC
- **configuration.proto**: Application configuration structures

### Applications

#### ZyrePublisher (`src/apps/ZyrePublisherTest.cpp`)

Demonstrates:
- Zyre peer-to-peer publishing
- Protocol buffer serialization (SensorReading, SensorDataBatch, Command)
- Periodic message publishing
- GeneralLogger usage

#### ZyreSubscriber (`src/apps/ZyreSubscriberTest.cpp`)

Demonstrates:
- Zyre peer-to-peer subscription
- Protocol buffer deserialization
- Topic-based message handling
- Formatted logging with spdlog

#### HighBandwidthPublisher (`src/apps/HighBandwidthPublisherTester.cpp`)

Demonstrates:
- High-frequency UDP multicast publishing
- Large message fragmentation
- Sensor data streaming

#### HighBandwidthSubscriber (`src/apps/HighBandwidthSubscriberTester.cpp`)

Demonstrates:
- UDP multicast subscription
- Fragment reassembly
- High-throughput message reception

#### RealTimeGraphsTest (`src/apps/RealTimeGraphsTest.cpp`)

Demonstrates:
- Interactive Qt application using the RealTimeGraphs widget library
- Spectrum, waterfall, and constellation displays

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

1. **Benchmarking**: Add Google Benchmark for performance testing
2. **Documentation**: Add Doxygen for API documentation
3. **Packaging**: Add CPack for installers/packages
4. **Cross-compilation**: Add toolchain files for embedded targets
5. **Fuzzing**: Add libFuzzer for fuzz testing
