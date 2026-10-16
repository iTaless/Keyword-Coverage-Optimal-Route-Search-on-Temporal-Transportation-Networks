cmake_minimum_required(VERSION 3.16)
project(keyword_coverage_temporal_routes LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(Python3 COMPONENTS Interpreter)

add_executable(route_search src/main.cpp)
add_executable(kcrn_bl baseline_bl/fixed_query_runner.cpp)
add_executable(test_bl baseline_bl/test_bl.cpp)

foreach(target route_search kcrn_bl test_bl)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
  else()
    target_compile_options(${target} PRIVATE -O2 -Wall -Wextra -Wpedantic)
  endif()
endforeach()

if(BUILD_TESTING)
  add_test(NAME bl_time_expanded_tests COMMAND test_bl)
  if(Python3_Interpreter_FOUND)
    add_test(
      NAME small_exhaustive_oracle
      COMMAND ${Python3_EXECUTABLE}
              ${CMAKE_SOURCE_DIR}/tests/test_small_oracle.py
              $<TARGET_FILE:route_search>)
    add_test(
      NAME fixed_query_interface
      COMMAND ${Python3_EXECUTABLE}
              ${CMAKE_SOURCE_DIR}/tests/test_fixed_query_interface.py
              $<TARGET_FILE:route_search>
              $<TARGET_FILE:kcrn_bl>)
    add_test(
      NAME seconds_boundary
      COMMAND ${Python3_EXECUTABLE}
              ${CMAKE_SOURCE_DIR}/tests/test_seconds_boundary.py
              $<TARGET_FILE:route_search>
              $<TARGET_FILE:kcrn_bl>)
    add_test(
      NAME numeric_boundaries
      COMMAND ${Python3_EXECUTABLE}
              ${CMAKE_SOURCE_DIR}/tests/test_numeric_boundaries.py
              $<TARGET_FILE:route_search>
              $<TARGET_FILE:kcrn_bl>)
  endif()
endif()

