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
│   ├── RadioWizardMain/      # Main Qt application (SDR controls + visualizations)
│   ├── TestApps/             # Test/demo executables
│   └── libs/
│       ├── CommonUtils/      # Common utilities (logging, timers, buffers)
│       ├── PubSub/           # Publish-Subscribe messaging (Zyre + UDP multicast)
│       ├── SdrEngine/        # SDR device abstraction + DSP pipeline
│       ├── RealTimeGraphs/   # Qt 6 real-time visualization widgets
│       ├── Vita49_2/         # VITA 49.2 signal data packet library
│       └── proto/            # Protocol buffer definitions
├── tests/                    # Unit tests (one directory per library)
├── docs/                     # Documentation
└── .github/                  # CI/CD and this file
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

### Adding a New Library Class

All libraries use `file(GLOB)` for source discovery. The pattern is the same for
CommonUtils, PubSub, SdrEngine, RealTimeGraphs, and Vita49_2:

1. Create `src/libs/<Library>/NewClass.h`
2. Create `src/libs/<Library>/NewClass.cpp`
3. Create `tests/<Library>Tests/NewClassUt.cpp`
4. Re-run CMake configure to pick up new files

RealTimeGraphs has AUTOMOC enabled; Qt signals/slots are processed automatically.

### Adding a New Proto Message

1. Create or edit file in `src/libs/proto/proto-messages/` directory
2. Proto files are auto-discovered via glob in `src/libs/proto/CMakeLists.txt`
3. Include generated header as `#include "message_name.pb.h"`

### Adding a New Test Application

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

## CMake Target Naming Conventions

- **Libraries**: Named after their directory (e.g., `CommonUtils`, `SdrEngine`, `RealTimeGraphs`, `Vita49_2`). Exception: `PubSubLib` for PubSub, `ProtoLib` (alias `RadioWizard::proto`) for proto.
- **Unit tests**: `UnitTests_<LibraryName>` (e.g., `UnitTests_CommonUtils`)
- **Coverage**: `<LibraryName>Coverage` (e.g., `CommonUtilsCoverage`) — available when `ENABLE_COVERAGE=ON`
- **Main application**: `RadioWizardMain`
- **Test applications**: Named after their purpose (defined in `src/TestApps/CMakeLists.txt`)

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
