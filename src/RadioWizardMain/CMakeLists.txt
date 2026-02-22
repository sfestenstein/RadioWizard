cmake_minimum_required(VERSION 3.16)

project(RadioWizardMain VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

qt_add_executable(RadioWizardMain
   main.cpp
   MainWindow.cpp
   MainWindow.h
   MainWindow.ui
   AudioOutput.cpp
   AudioOutput.h
   FrequencyInputWidget.cpp
   FrequencyInputWidget.h
)

target_include_directories(RadioWizardMain PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET RadioWizardMain PROPERTY AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(RadioWizardMain PRIVATE
   Qt6::Widgets
   Qt6::Multimedia
   RealTimeGraphs
   CommonUtils
   SdrEngine
)

set_target_properties(RadioWizardMain PROPERTIES
   MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
   MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
   MACOSX_BUNDLE TRUE
   WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS RadioWizardMain
   BUNDLE DESTINATION .
   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_finalize_executable(RadioWizardMain)
