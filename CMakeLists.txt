cmake_minimum_required(VERSION 3.22)
project(FlangModernizationAdvisor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find LLVM / Flang dependencies
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)
find_package(Flang CONFIG QUIET)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM include dirs: ${LLVM_INCLUDE_DIRS}")

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

set(SRC_FILES
  src/main.cpp
  src/FlangFrontend.cpp
  src/PatternDetector.cpp
  src/ImpactAnalyzer.cpp
  src/Prioritization.cpp
)

add_executable(flang-modernization-advisor ${SRC_FILES})

target_include_directories(flang-modernization-advisor PRIVATE include)

# Link official Flang library targets if found
if(Flang_FOUND OR TARGET flangFrontend)
  message(STATUS "Found Flang libraries. Enabling compiler AST parser pathway.")
  add_definitions(-DUSE_FLANG_PARSER)
  target_link_libraries(flang-modernization-advisor PRIVATE
    flangFrontend
    FortranParser
    FortranSemantics
    FortranEvaluate
    FortranDecimal
    FortranSupport
    FortranUtils
  )
endif()

if(TARGET Flang::FlangFrontend)
  target_link_libraries(flang-modernization-advisor PRIVATE Flang::FlangFrontend)
endif()

target_link_libraries(flang-modernization-advisor PRIVATE ${LLVM_LIBRARIES})

install(TARGETS flang-modernization-advisor RUNTIME DESTINATION bin)
