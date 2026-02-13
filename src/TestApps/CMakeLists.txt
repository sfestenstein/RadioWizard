# =============================================================================
# Applications - CMakeLists.txt
# =============================================================================

# Zyre Publisher executable
add_executable(ZyrePublisher  ZyrePublisherTest.cpp)

target_link_libraries(ZyrePublisher
   PRIVATE PubSubLib ProtoLib
)

# Zyre Subscriber executable
add_executable(ZyreSubscriber ZyreSubscriberTest.cpp)

target_link_libraries(ZyreSubscriber
   PRIVATE PubSubLib ProtoLib )

# High-Bandwidth Subscriber executable
add_executable(HighBandwidthSubscriber HighBandwidthSubscriberTester.cpp)

target_link_libraries(HighBandwidthSubscriber
   PRIVATE PubSubLib ProtoLib )

# High-Bandwidth Publisher executable
add_executable(HighBandwidthPublisher HighBandwidthPublisherTester.cpp)

target_link_libraries(HighBandwidthPublisher
   PRIVATE PubSubLib ProtoLib )

# VITA 49.2 Round-Trip Test
add_executable(Vita49RoundTripTest Vita49RoundTripTest.cpp)

target_link_libraries(Vita49RoundTripTest
   PRIVATE Vita49_2 CommonUtils )

# VITA 49.2 Performance Benchmark
add_executable(Vita49PerfBenchmark Vita49PerfBenchmark.cpp)

target_link_libraries(Vita49PerfBenchmark
   PRIVATE Vita49_2 CommonUtils )

# VITA 49.2 File Codec (generate / inspect / roundtrip)
add_executable(Vita49FileCodec Vita49FileCodec.cpp)

target_link_libraries(Vita49FileCodec
   PRIVATE Vita49_2 CommonUtils )

# RealTimeGraphs interactive test application
add_executable(RealTimeGraphsTest RealTimeGraphsTest.cpp)

target_link_libraries(RealTimeGraphsTest
   PRIVATE RealTimeGraphs CommonUtils
           Qt6::Core Qt6::Gui Qt6::Widgets )

set_target_properties(RealTimeGraphsTest PROPERTIES
   AUTOMOC ON
)

# Set properties
set(APP_TARGETS
   ZyrePublisher ZyreSubscriber HighBandwidthSubscriber HighBandwidthPublisher
   Vita49RoundTripTest Vita49PerfBenchmark Vita49FileCodec RealTimeGraphsTest
)

set_target_properties(${APP_TARGETS}
   PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
