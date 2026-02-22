"""
RadioWizard Conan 2.0 Configuration

Manages external dependencies for the RadioWizard SDR application —
SDR control, spectrum/IQ observation, signal isolation, and demodulation.
"""

from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout


class RadioWizardConan(ConanFile):
   name = "radiowizard"
   version = "1.0.0"
   description = "SDR control, spectrum/IQ observation, signal isolation and demodulation"

   # Build settings
   settings = "os", "compiler", "build_type", "arch"

   # Build options
   options = {
      "shared": [True, False],
      "fPIC": [True, False],
      "build_tests": [True, False],
      "enable_coverage": [True, False],
      "enable_sanitizers": [True, False],
   }

   default_options = {
      "shared": False,
      "fPIC": True,
      "build_tests": True,
      "enable_coverage": False,
      "enable_sanitizers": False,
      "spdlog/*:use_std_fmt": True,  # Use C++20 std::format instead of external fmt
   }

   # Dependencies
   def requirements(self):
      # Logging
      self.requires("spdlog/1.15.0")

      # Protocol Buffers
      self.requires("protobuf/5.27.0")

      # ZeroMQ messaging
      self.requires("zeromq/4.3.5")
      self.requires("cppzmq/4.10.0")

      # Zyre (ZeroMQ Realtime Exchange) and dependencies
      self.requires("czmq/4.2.1")
      self.requires("zyre/2.0.1")

      # zyre/2.0.1 on Linux/FreeBSD unconditionally depends on libsystemd/255.
      # libsystemd/255 (base) can fail to build against newer kernel headers.
      # Override to a newer 255.x patch release that includes the updated magic list.
      if self.settings.os in ["Linux", "FreeBSD"]:
         self.requires("libsystemd/255.10", override=True)



      # FFT
      self.requires("fftw/3.3.10")

      # DSP
      self.requires("liquid-dsp/1.6.0")

      # Qt 6 — for RealTimeGraphs widget library
      self.requires("qt/6.10.1")

   def build_requirements(self):
      # Unit testing (only needed during build)
      if self.options.build_tests:
         self.test_requires("gtest/1.14.0")

   def configure(self):
      # Handle fPIC for shared libraries
      if self.options.shared:
         self.options.rm_safe("fPIC")

      # Disable systemd integration in czmq (option name in ConanCenter recipe is `with_systemd`).
      # Note: zyre/2.0.1 still depends on libsystemd on Linux/FreeBSD regardless; see override above.
      self.options["czmq/*"].with_systemd = False

      # Qt 6 options — enable only the modules we need
      self.options["qt/*"].shared = True     # Qt must be shared
      self.options["qt/*"].qtshadertools = True
      self.options["qt/*"].qtmultimedia = True
      self.options["qt/*"].gui = True
      self.options["qt/*"].widgets = True
      self.options["qt/*"].opengl = "desktop"
      self.options["qt/*"].with_pq = False       # No PostgreSQL SQL driver
      self.options["qt/*"].with_odbc = False      # No ODBC SQL driver
      self.options["qt/*"].with_sqlite3 = False   # No SQLite SQL driver

      # openal-soft (all available versions through 1.23.1) fails to build with
      # GCC 15 due to a typed-enum parse error in alc/alu.h.  RadioWizard does
      # not use OpenAL directly, so disable it in Qt to avoid the broken dep.
      self.options["qt/*"].with_openal = False

   def layout(self):
      # Use standard CMake layout
      cmake_layout(self)

   def generate(self):
      # Generate CMake dependency files
      deps = CMakeDeps(self)
      deps.generate()

      # Generate CMake toolchain file
      tc = CMakeToolchain(self)

      # Pass options to CMake
      tc.variables["BUILD_TESTS"] = self.options.build_tests
      tc.variables["ENABLE_COVERAGE"] = self.options.enable_coverage
      tc.variables["ENABLE_SANITIZERS"] = self.options.enable_sanitizers

      tc.generate()
