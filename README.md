# RadioWizard

A C++20 application for Software Defined Radio (SDR) control, spectrum and I/Q data observation, signal isolation, and signal demodulation. RadioWizard interfaces with SDR hardware to provide real-time spectrum visualization, waterfall displays, constellation diagrams, and signal processing capabilities.

## Features

- **SDR Control**: Interface with Software Defined Radio hardware for tuning, gain control, and sample acquisition
- **Spectrum Observation**: Real-time spectrum (frequency-domain) display of RF signals
- **I/Q Data Handling**: Capture, stream, and analyze raw I/Q samples from SDR devices
- **Signal Isolation**: Identify and isolate individual signals within a wideband capture
- **Signal Demodulation**: Demodulate isolated signals using configurable demodulation chains
- **VITA 49.2 Support**: Encode and decode VITA 49.2 signal data and context packets for interoperability
- **Real-Time Visualization**: Qt 6 widgets for spectrum, waterfall/spectrogram, and IQ constellation displays
- **Distributed Architecture**: Publish/subscribe messaging via Zyre (peer-to-peer) and UDP multicast for high-bandwidth data streams
- **Protocol Buffers** for structured command/control and telemetry serialization
- **Modern C++20** with CMake 3.25+, Conan 2.0, Google Test, spdlog, sanitizers, and CI/CD

## Quick Start

### Prerequisites

- **Compiler**: GCC 13+ or Clang 15+ (Linux/macOS)
- **CMake**: 3.25+
- **Conan**: 2.0+
- **Build Tool**: Ninja (recommended) or Make
- **Python**: 3.8+ (for Conan)
- **Linux only**: OpenGL / X11 / XCB development headers (see [Build Guide](docs/BUILD.md#linux-ubuntudebian))

### Build Instructions

#### 1. Clone and Setup

```bash
# Clone the repository
git clone https://github.com/sfestenstein/RadioWizard
cd RadioWizard

# Create a Conan profile (one-time setup)
conan profile detect --force
```

#### 2. Install Dependencies (One-Time)

Install both Debug and Release configurations to a unified build folder:

```bash
conan install . --output-folder=build --build=missing -s build_type=Release -s compiler.cppstd=gnu20
```

> **Note:** The first build can take 20–60+ minutes as Conan builds Qt 6 and its
> transitive dependencies (OpenGL, Freetype, Harfbuzz, etc.) from source.
> Subsequent builds use the Conan binary cache.

> **Important:** Use `gnu20` (not `20`) for `compiler.cppstd` — Qt 6 requires GNU
> extensions such as `__int128` that are disabled in strict ISO C++20 mode.

If `conan install` fails while building `libsystemd/255` with an error about
"Unknown filesystems defined in kernel headers" (for example `BCACHEFS_SUPER_MAGIC`),
this is typically a mismatch between newer Linux kernel headers and the base `255` recipe.
This project works around it by overriding to a newer `libsystemd/255.x` patch release
in `conanfile.py`.

If `conan install` fails while building `openal-soft` with GCC 15 (typed-enum
parse errors in `alc/alu.h`), the project disables OpenAL in Qt via
`with_openal = False` in `conanfile.py` since RadioWizard does not use OpenAL.

This installs all dependencies for all presets (debug, release, coverage, ci-linux).

#### 3. Build and Test

**Debug Build** (with sanitizers and tests):
```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

**Release Build** (optimized):
```bash
cmake --preset release
cmake --build --preset release
```

**Coverage Build**:
```bash
cmake --preset coverage
cmake --build --preset coverage
ctest --preset coverage
cmake --build --preset coverage --target CommonUtilsCoverage
# View report in build/coverage/CommonUtilsCoverage/index.html
```

#### 4. Run Applications

```bash
# Main RadioWizard application (Qt GUI with SDR engine)
./build/debug/bin/RadioWizardMain

# Zyre-based pub/sub (peer-to-peer discovery)
./build/debug/bin/ZyreSubscriber  # In terminal 1
./build/debug/bin/ZyrePublisher   # In terminal 2

# (Optional) Bind to a specific network interface.
# You can pass either an interface name (recommended) or a local IP on that interface.
#   - Linux examples:  ./build/debug/bin/ZyreSubscriber wlp2s0
#                     ./build/debug/bin/ZyrePublisher  192.168.1.130
#   - macOS examples:  ./build/debug/bin/ZyreSubscriber en0
#                     ./build/debug/bin/ZyrePublisher  192.168.1.42
# Note: Zyre discovery uses LAN beacons; both machines must be on the same LAN/subnet
# and firewalls/VPNs/guest Wi-Fi isolation can block peer discovery.

# High-bandwidth UDP multicast pub/sub
./build/debug/bin/HighBandwidthSubscriber  # In terminal 1
./build/debug/bin/HighBandwidthPublisher   # In terminal 2

# RealTimeGraphs interactive test
./build/debug/bin/RealTimeGraphsTest
```

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
├── sanitizer_suppressions/   # ASan/LSan suppression files
└── .github/                  # CI/CD workflows and configuration
```

## Documentation

- [Project Design](docs/DESIGN.md) - Architecture and design decisions
- [Build Guide](docs/BUILD.md) - Detailed build instructions
- [Development Guide](docs/DEVELOPMENT.md) - Contributing and development workflow

## CMake Presets

The project uses CMake presets for consistent build configurations across different environments.

### Configure Presets

| Preset | Build Type | Tests | Coverage | Sanitizers | Clang-Tidy | Use Case |
|--------|------------|-------|----------|------------|------------|----------|
| `debug` | Debug | ✅ | ❌ | ✅ | ❌ | Local development |
| `debug-clang-tidy` | Debug | ✅ | ❌ | ✅ | ✅ | Development + static analysis |
| `release` | Release | ❌ | ❌ | ❌ | ❌ | Production builds |
| `relwithdebinfo` | RelWithDebInfo | ✅ | ❌ | ❌ | ❌ | Profiling / optimized debug |
| `coverage` | Debug | ✅ | ✅ | ❌ | ✅ | Code coverage reports |
| `ci-linux` | Debug | ✅ | ✅ | ❌ | ❌ | GitHub Actions CI (Linux) |
| `ci-macos` | Debug | ✅ | ❌ | ❌ | ❌ | GitHub Actions CI (macOS) |

### Using Presets

```bash
# List available presets
cmake --list-presets

# Configure with a preset
cmake --preset debug

# Build with a preset
cmake --build --preset debug

# Test with a preset
ctest --preset debug
```

### Custom Configuration (without presets)

```bash
# Manual configuration example (using unified Conan output)
cmake -B build/custom -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_TOOLCHAIN_FILE=build/build/Debug/generators/conan_toolchain.cmake \
    -DBUILD_TESTS=ON \
    -DENABLE_SANITIZERS=ON
```

## CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TESTS` | ON | Build unit tests |
| `ENABLE_COVERAGE` | OFF | Enable code coverage |
| `ENABLE_SANITIZERS` | OFF | Enable ASan/UBSan |
| `ENABLE_CLANG_TIDY` | OFF | Enable clang-tidy |

## Dependencies

Managed by Conan 2.0:

| Package | Version | Purpose |
|---------|---------|---------|
| spdlog | 1.15.0 | Logging |
| protobuf | 5.27.0 | Serialization |
| zeromq | 4.3.5 | Low-level messaging |
| cppzmq | 4.10.0 | C++ ZeroMQ bindings |
| czmq | 4.2.1 | High-level C ZeroMQ binding |
| zyre | 2.0.1 | Peer-to-peer discovery |
| fftw | 3.3.10 | Fast Fourier Transform (SdrEngine) |
| liquid-dsp | 1.6.0 | DSP primitives — filters, NCO, resampling (SdrEngine) |
| qt | 6.10.1 | GUI widgets (RealTimeGraphs, RadioWizardMain) |
| gtest | 1.14.0 | Unit testing |

System dependencies (not managed by Conan):

| Package | Purpose |
|---------|---------|
| librtlsdr | RTL-SDR hardware driver library (SdrEngine) |

## License

TBD
