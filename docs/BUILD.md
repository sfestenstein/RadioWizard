# Build Guide

This document provides detailed instructions for building the RadioWizard project — a C++20 application for Software Defined Radio control, spectrum and I/Q data observation, signal isolation, and signal demodulation.

## Prerequisites

### All Platforms

- **CMake 3.25+**: [Download](https://cmake.org/download/)
- **Python 3.8+**: For Conan package manager
- **Conan 2.0+**: `pip install conan`
- **Ninja** (recommended): [Download](https://ninja-build.org/)

### Linux (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y \
   build-essential \
   cmake \
   ninja-build \
   python3-pip \
   lcov

# Qt 6 requires OpenGL, X11 and XCB development headers
sudo apt-get install -y \
   libgl-dev \
   libx11-xcb-dev \
   libfontenc-dev \
   libxkbfile-dev \
   libxcb-cursor-dev \
   libxcb-icccm4-dev \
   libxcb-keysyms1-dev \
   libxcb-shape0-dev \
   libxcb-xkb-dev \
   libxrender-dev

# RTL-SDR driver library (system dependency — not available via Conan)
sudo apt-get install -y librtlsdr-dev

pip3 install conan
```

### macOS

```bash
# Install Xcode command line tools
xcode-select --install

# Install Homebrew packages
brew install cmake ninja python lcov

# RTL-SDR driver library (system dependency — not available via Conan)
brew install librtlsdr

pip3 install conan
```

## Initial Setup

### 1. Configure Conan Profile

First-time setup - detect your compiler:

```bash
conan profile detect --force
```

This creates a default profile at `~/.conan2/profiles/default`.

### 2. Verify Profile

```bash
conan profile show
```

## Build Configurations

### Debug Build (Recommended for Development)

```bash
# Install all configurations to unified build folder
conan install . --output-folder=build --build=missing -s build_type=Release -s compiler.cppstd=20

# Configure and Build Debug
cmake --preset debug
cmake --build --preset debug

# Test
ctest --preset debug
```

> **Note:** The first build can take 20–60+ minutes as Conan compiles Qt 6 and its
> transitive dependencies from source. Subsequent builds use the Conan cache.

### Release Build

```bash
# Configure and Build Release
cmake --preset release
cmake --build --preset release
```

### Coverage Build

```bash
# Configure with coverage
cmake --preset coverage
cmake --build --preset coverage

# Run tests and generate coverage (per-library targets)
ctest --preset coverage
cmake --build --preset coverage --target CommonUtilsCoverage
cmake --build --preset coverage --target PubSubCoverage
cmake --build --preset coverage --target Vita49_2Coverage

# View report
# Open build/coverage/CommonUtilsCoverage/index.html
```

## Build Options

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TESTS` | ON | Build unit tests |
| `ENABLE_COVERAGE` | OFF | Enable code coverage instrumentation |
| `ENABLE_SANITIZERS` | OFF | Enable ASan and UBSan |
| `ENABLE_CLANG_TIDY` | OFF | Run clang-tidy during build |

### Conan Options

| Option | Default | Description |
|--------|---------|-------------|
| `shared` | False | Build shared libraries |
| `fPIC` | True | Position independent code |
| `build_tests` | True | Install test dependencies |
| `enable_coverage` | False | Pass to CMake |
| `enable_sanitizers` | False | Pass to CMake |

## Manual Build (Without Presets)

If CMake presets don't work in your environment:

```bash
# Create build directory
mkdir -p build/manual
cd build/manual

# Install Conan dependencies
conan install ../.. --output-folder=. --build=missing -s build_type=Debug

# Configure
cmake ../.. \
   -G "Ninja" \
   -DCMAKE_BUILD_TYPE=Debug \
   -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake \
   -DBUILD_TESTS=ON

# Build
cmake --build .

# Test
ctest --output-on-failure
```

## Running Applications

After building:

```bash
# Main RadioWizard application (Qt GUI with SDR engine)
./build/debug/bin/RadioWizardMain

# Zyre-based pub/sub (peer-to-peer discovery)
./build/debug/bin/ZyreSubscriber   # In terminal 1
./build/debug/bin/ZyrePublisher    # In terminal 2

# High-bandwidth UDP multicast pub/sub
./build/debug/bin/HighBandwidthSubscriber   # In terminal 1
./build/debug/bin/HighBandwidthPublisher    # In terminal 2

# RealTimeGraphs interactive test
./build/debug/bin/RealTimeGraphsTest

# VITA 49 utilities
./build/debug/bin/Vita49FileCodec
./build/debug/bin/Vita49PerfBenchmark
./build/debug/bin/Vita49RoundTripTest
```

For the Zyre pub/sub, you should see sensor data messages being published every second and received by the subscriber.

## Troubleshooting

### Conan Can't Find Package

```bash
# Update Conan remote index
conan remote update conancenter --url https://center.conan.io

# Clear Conan cache and rebuild
conan remove "*" -c
conan install . --output-folder=build/debug --build=missing
```

### CMake Can't Find Conan Packages

Ensure you're using the toolchain file:
```bash
cmake -DCMAKE_TOOLCHAIN_FILE=build/debug/conan_toolchain.cmake ...
```

### Protobuf Compiler Not Found

Conan should provide protoc. If not found:
```bash
# Check Conan installed protobuf
conan list "protobuf/*"

# Manually specify path
cmake -DProtobuf_PROTOC_EXECUTABLE=/path/to/protoc ...
```

### Qt / RealTimeGraphs Build Issues

If CMake fails with `Could not find a package configuration file provided by "Qt6"`:
- Ensure `conan install` completed successfully — Qt 6 is a required dependency and
  its packages must be available for the CMake configure step.

If the Qt Conan build itself fails:
- Qt 6 requires OpenGL development headers on your system. On Linux:
  ```bash
  sudo apt-get install -y libgl-dev libx11-xcb-dev libfontenc-dev \
     libxkbfile-dev libxcb-cursor-dev libxcb-icccm4-dev libxcb-keysyms1-dev \
     libxcb-shape0-dev libxcb-xkb-dev libxrender-dev
  ```
- On macOS, Xcode command-line tools provide the required OpenGL frameworks.
- The first build of Qt from source takes a long time (20–60+ minutes). Subsequent
  builds use the Conan binary cache.
