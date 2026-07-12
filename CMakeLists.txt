cmake_minimum_required(VERSION 3.28)
project(sysmon VERSION 1.0.0 LANGUAGES CXX
  DESCRIPTION "Terminal system monitor with FTXUI — CPU, memory, processes, disk I/O"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies (via FetchContent) ──
include(FetchContent)

# spdlog (header-only logging)
FetchContent_Declare(spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.15.1
  SYSTEM
)
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "Use bundled fmt")
FetchContent_MakeAvailable(spdlog)

# FTXUI (terminal UI library)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG        v5.0.0
  SYSTEM
)
FetchContent_MakeAvailable(ftxui)

# Catch2 (test framework)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.8.0
  SYSTEM
)
FetchContent_MakeAvailable(Catch2)

# ── Library target for system monitoring ──
file(GLOB_RECURSE LIB_SOURCES src/*.cpp)
file(GLOB_RECURSE LIB_HEADERS include/*.hpp include/*.h)

add_library(sysmon_lib STATIC ${LIB_SOURCES} ${LIB_HEADERS})
target_include_directories(sysmon_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(sysmon_lib PUBLIC
  spdlog::spdlog
  ftxui::screen
  ftxui::dom
  ftxui::component
)

# ── Main executable ──
add_executable(sysmon src/main.cpp)
target_link_libraries(sysmon PRIVATE sysmon_lib)

# ── Tests ──
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ── Install ──
install(TARGETS sysmon DESTINATION bin)
install(TARGETS sysmon_lib DESTINATION lib)
install(DIRECTORY include/sysmon DESTINATION include)
