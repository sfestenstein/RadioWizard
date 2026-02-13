# GitHub Copilot Instructions for StarterCpp

This document provides context and guidelines for GitHub Copilot when working with the StarterCpp project.

## Project Overview

StarterCpp is a C++20 starter project template using:
- **Build System**: CMake 3.25+ with presets
- **Package Manager**: Conan 2.0
- **Compiler**: GCC 13+ or Clang 15+ (Linux/macOS)
- **Testing**: Google Test
- **Dependencies**: spdlog, protobuf, ZeroMQ (cppzmq), CZMQ, Zyre, Qt 6

## Project Structure

```
StarterCpp/
├── src/
│   ├── apps/               # Executables
│   │   ├── ZyrePublisherTest.cpp
│   │   ├── ZyreSubscriberTest.cpp
│   │   ├── HighBandwidthPublisherTester.cpp
│   │   ├── HighBandwidthSubscriberTester.cpp
│   │   └── RealTimeGraphsTest.cpp      # Qt app
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
│       └── proto/          # Protocol buffer library
│           └── proto-messages/ # .proto source files
├── tests/                  # Unit tests
│   ├── CommonUtilsTests/   # Tests for CommonUtils library
│   └── PubSubTests/        # Tests for PubSub library
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

### Adding a New Application

1. Create `src/apps/new_app_main.cpp`
2. Add to `src/apps/CMakeLists.txt`:
   ```cmake
   add_executable(new_app new_app_main.cpp)
   target_link_libraries(new_app PRIVATE StarterCpp::common_utils ...)
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
- `ProtoLib` - Protobuf library (alias: `StarterCpp::proto`)
- `ZyrePublisher` - Zyre publisher test application
- `ZyreSubscriber` - Zyre subscriber test application
- `HighBandwidthPublisher` - UDP multicast publisher test application
- `HighBandwidthSubscriber` - UDP multicast subscriber test application
- `RealTimeGraphs` - Qt widget library
- `RealTimeGraphsTest` - Qt interactive test application
- `CommonUtilsTests` - CommonUtils unit tests
- `PubSubTests` - PubSub unit tests
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
