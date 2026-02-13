# GitHub Copilot Instructions for RadioWizard

This document provides context and guidelines for GitHub Copilot when working with the RadioWizard project.

## Project Overview

RadioWizard is a C++20 application for Software Defined Radio (SDR) control, spectrum and I/Q data observation, signal isolation, and signal demodulation. It uses:
- **Build System**: CMake 3.25+ with presets
- **Package Manager**: Conan 2.0
- **Compiler**: GCC 13+ or Clang 15+ (Linux/macOS)
- **Testing**: Google Test
- **Dependencies**: spdlog, protobuf, ZeroMQ (cppzmq), CZMQ, Zyre, Qt 6, FFTW3, liquid-dsp, librtlsdr (system)

## Project Structure

```
RadioWizard/
├── src/
│   ├── TestApps/            # Test/demo executables
│   │   ├── ZyrePublisherTest.cpp
│   │   ├── ZyreSubscriberTest.cpp
│   │   ├── HighBandwidthPublisherTester.cpp
│   │   ├── HighBandwidthSubscriberTester.cpp
│   │   ├── RealTimeGraphsTest.cpp      # Qt visualization demo
│   │   ├── Vita49FileCodec.cpp         # VITA 49 file encode/decode
│   │   ├── Vita49PerfBenchmark.cpp     # VITA 49 performance benchmark
│   │   └── Vita49RoundTripTest.cpp     # VITA 49 round-trip validation
│   └── libs/               # Libraries
│       ├── CommonUtils/    # Common utility library
│       │   ├── GeneralLogger.h   # Async spdlog wrapper with macros
│       │   ├── GeneralLogger.cpp
│       │   ├── Timer.h           # Basic timer class
│       │   ├── Timer.cpp
│       │   ├── SnoozableTimer.h  # Timer with snooze capability
│       │   ├── SnoozableTimer.cpp
│       │   ├── CircularBuffer.h  # Lock-free circular buffer
│       │   └── DataHandler.h     # Data handling (header-only)
│       ├── PubSub/         # Publish-Subscribe library
│       │   ├── ZyreNode.h        # Base Zyre node class
│       │   ├── ZyrePublisher.h   # Zyre-based publisher
│       │   ├── ZyreSubscriber.h  # Zyre-based subscriber
│       │   ├── HighBandwidthPublisher.h   # UDP multicast publisher
│       │   └── HighBandwidthSubscriber.h  # UDP multicast subscriber
│       ├── RealTimeGraphs/ # Qt widget library
│       │   ├── SpectrumWidget.h      # Real-time spectrum display
│       │   ├── WaterfallWidget.h     # Waterfall/spectrogram display
│       │   ├── ConstellationWidget.h # IQ constellation display
│       │   └── ColorMap.h            # Color map utilities
│       ├── Vita49_2/       # VITA 49.2 signal data packet library
│       │   ├── PacketHeader.h        # VITA 49 packet header
│       │   ├── SignalDataPacket.h    # Signal data packet encode/decode
│       │   ├── ContextPacket.h       # Context packet encode/decode
│       │   ├── Vita49Codec.h         # High-level codec for file I/O
│       │   ├── Vita49Types.h         # VITA 49 type definitions
│       │   └── ByteSwap.h           # Endian conversion utilities
│       └── proto/          # Protocol buffer library
│           └── proto-messages/ # .proto source files
├── tests/                  # Unit tests
│   ├── CommonUtilsTests/   # Tests for CommonUtils library
│   ├── PubSubTests/        # Tests for PubSub library
│   └── Vita49_2Tests/      # Tests for VITA 49.2 library
├── docs/                   # Documentation
└── .github/                # CI/CD and this file
```

## Coding Conventions

### Style Guide

- **Indentation**: 3 spaces (no tabs)
- **Braces**: Allman style (opening brace on new line)
- **Line length**: 100 characters maximum
- **Naming**:
  - Classes: `PascalCase`
  - Functions/Methods: `camelCase`
  - Variables: `camelCase`
  - Member variables: `_` prefix (e.g., `_value`)
  - Static members: `s_` prefix (e.g., `s_instance`)

### Code Example

```cpp
namespace CommonUtils
{

class MyClass
{
public:
   MyClass();

   void processData(int value);
   int getValue() const;

private:
   void internalMethod();

   int _value;
   static int s_counter;
};

} // namespace CommonUtils
```

### Header Structure

```cpp
#ifndef MYCLASS_H_
#define MYCLASS_H_

// Project headers
// Third-party headers
// System headers

#include "CommonUtils/GeneralLogger.h"

#include <spdlog/spdlog.h>

#include <memory>
#include <string>

// ... class definition ...

#endif // MYCLASS_H_
```

## Common Tasks

### Adding a New CommonUtils Class

1. Create `src/libs/CommonUtils/NewClass.h`
2. Create `src/libs/CommonUtils/NewClass.cpp` (auto-discovered via `file(GLOB)`)
3. Create `tests/CommonUtilsTests/NewClassUt.cpp` (auto-discovered via `file(GLOB)`)
4. Re-run CMake configure to pick up new files

### Adding a New PubSub Class

1. Create `src/libs/PubSub/NewClass.h`
2. Create `src/libs/PubSub/NewClass.cpp` (auto-discovered via `file(GLOB)`)
3. Create `tests/PubSubTests/NewClassUt.cpp` (auto-discovered via `file(GLOB)`)
4. Re-run CMake configure to pick up new files

### Adding a New Proto Message

1. Create or edit file in `src/libs/proto/proto-messages/` directory
2. Proto files are auto-discovered via glob in `src/libs/proto/CMakeLists.txt`
3. Include generated header as `#include "message_name.pb.h"`

### Adding a New RealTimeGraphs Widget (Qt 6)

1. Create `src/libs/RealTimeGraphs/NewWidget.h`
2. Create `src/libs/RealTimeGraphs/NewWidget.cpp` (auto-discovered via `file(GLOB)`)
3. AUTOMOC is enabled; Qt signals/slots are processed automatically
4. Re-run CMake configure to pick up new files

### Adding a New Vita49_2 Class

1. Create `src/libs/Vita49_2/NewClass.h`
2. Create `src/libs/Vita49_2/NewClass.cpp` (auto-discovered via `file(GLOB)`)
3. Create `tests/Vita49_2Tests/NewClassUt.cpp` (auto-discovered via `file(GLOB)`)
4. Re-run CMake configure to pick up new files

### Adding a New Application

1. Create `src/TestApps/new_app_main.cpp`
2. Add to `src/TestApps/CMakeLists.txt`:
   ```cmake
   add_executable(new_app new_app_main.cpp)
   target_link_libraries(new_app PRIVATE RadioWizard::common_utils ...)
   ```

## Build Commands

```bash
# Install dependencies (both configurations to unified folder)
conan install . --output-folder=build --build=missing -s build_type=Release -s compiler.cppstd=20

# Configure
cmake --preset debug

# Build
cmake --build --preset debug

# Test
ctest --preset debug

# Coverage
cmake --build --preset coverage --target CommonUtilsCoverage
```

## CMake Targets

- `CommonUtils` - CommonUtils shared library
- `PubSubLib` - PubSub shared library (Zyre and HighBandwidth messaging)
- `ProtoLib` - Protobuf library (alias: `RadioWizard::proto`)
- `Vita49_2` - VITA 49.2 signal data packet library
- `RealTimeGraphs` - Qt widget library for spectrum/waterfall/constellation display
- `ZyrePublisher` - Zyre publisher test application
- `ZyreSubscriber` - Zyre subscriber test application
- `HighBandwidthPublisher` - UDP multicast publisher test application
- `HighBandwidthSubscriber` - UDP multicast subscriber test application
- `RealTimeGraphsTest` - Qt interactive test application
- `Vita49FileCodec` - VITA 49 file encode/decode application
- `Vita49PerfBenchmark` - VITA 49 performance benchmark application
- `Vita49RoundTripTest` - VITA 49 round-trip validation application
- `CommonUtilsTests` - CommonUtils unit tests
- `PubSubTests` - PubSub unit tests
- `Vita49_2Tests` - VITA 49.2 unit tests
- `CommonUtilsCoverage` - Coverage report target (when `ENABLE_COVERAGE=ON`)
- `PubSubCoverage` - Coverage report target (when `ENABLE_COVERAGE=ON`)

## Dependencies Available

When suggesting code, these libraries are available:

| Library | Include | Namespace/Usage |
|---------|---------|------------------|
| spdlog | `<spdlog/spdlog.h>` | `spdlog::info()` or `CommonUtils::GeneralLogger` |
| protobuf | `"message.pb.h"` | `messages::MessageType` |
| ZeroMQ | `<zmq.hpp>` | `zmq::context_t`, `zmq::socket_t` |
| CZMQ | `<czmq.h>` | `zsock_t`, `zactor_t` |
| Zyre | `<zyre.h>` | `zyre_t` |
| Qt 6 | `<QWidget>`, `<QPainter>` | `Qt6::Core`, `Qt6::Gui`, `Qt6::Widgets`, `Qt6::OpenGLWidgets` |
| FFTW3 | `<fftw3.h>` | `fftw_plan`, `fftw_execute()` |
| liquid-dsp | `<liquid/liquid.h>` | `firfilt_crcf`, `nco_crcf`, etc. |
| RTL-SDR | `<rtl-sdr.h>` | `rtlsdr_open()`, `rtlsdr_read_async()` (system dep, `PkgConfig::RTLSDR`) |
| Google Test | `<gtest/gtest.h>` | `TEST()`, `EXPECT_EQ()` |

## Testing Patterns

```cpp
#include <gtest/gtest.h>
#include "MyClass.h"

// Test naming: TestSuiteName, TestName
TEST(MyClassTest, MethodName_Condition_ExpectedResult)
{
   // Arrange
   CommonUtils::MyClass instance;

   // Act
   auto result = instance.doSomething();

   // Assert
   EXPECT_EQ(result, expected);
}

// For tests needing setup/teardown, use fixtures:
class MyClassFixture : public ::testing::Test
{
protected:
   void SetUp() override { }
   void TearDown() override { }

   CommonUtils::MyClass _instance;
};

TEST_F(MyClassFixture, MethodName_WithFixture_ExpectedResult)
{
   EXPECT_TRUE(_instance.isValid());
}
```

## Error Handling Patterns

- Use exceptions for recoverable errors
- Use `std::optional` for values that may not exist
- Log errors using `GPERROR()` macro from GeneralLogger
- Use RAII for resource management

## Thread Safety

- Use `std::mutex` with `std::lock_guard` or `std::unique_lock`
- Use `std::atomic` for simple flags/counters
- The `SnoozableTimer` class provides thread-safe snooze functionality
- The `GeneralLogger` provides thread-safe async logging

## Important Notes

1. **No raw pointers for ownership** - Use `std::unique_ptr` or `std::shared_ptr`
2. **Prefer `std::string_view`** for read-only string parameters
3. **Use `[[nodiscard]]`** for functions whose return value should not be ignored
4. **Mark destructors `override`** in derived classes
5. **Use `= default`/`= delete`** for special member functions

## File Modification Guidelines

When modifying files:
- Always run clang-format before committing
- Add corresponding unit tests for new functionality
- Update documentation if adding public API
- Follow existing patterns in the codebase
