cmake_minimum_required(VERSION 3.15)
project(poker_odds CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Equity enumeration is CPU-bound, so default to an optimized build. The build
# type supplies the optimization flags (/O2 on MSVC); don't set them by hand or
# they collide with Debug's /RTC1.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra)
endif()

# Main calculator (CLI).
add_executable(poker main.cpp equity.cpp hand_evaluator.cpp serialize.cpp)

# Local web server for the browser UI.
add_executable(poker_server server.cpp equity.cpp hand_evaluator.cpp serialize.cpp)
if(WIN32)
  target_link_libraries(poker_server ws2_32)
endif()
# Copy the web assets next to the server executable so its default "web" dir resolves.
add_custom_command(TARGET poker_server POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/web" "$<TARGET_FILE_DIR:poker_server>/web"
  COMMENT "Copying web/ next to poker_server")

# Tests / verification tools.
add_executable(test_evaluator test_evaluator.cpp hand_evaluator.cpp)
add_executable(oracle_verify oracle_verify.cpp)

enable_testing()
add_test(NAME evaluator_selftests COMMAND test_evaluator)
