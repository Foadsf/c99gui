cmake_minimum_required(VERSION 3.10)
project(c99gui VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(C99GUI_STRICT "Build in strict ISO C99 mode (no platform headers, line input)" OFF)
option(C99GUI_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

add_executable(c99gui src/c99gui.c)

if(MSVC)
  target_compile_options(c99gui PRIVATE /W4 /utf-8)
  if(C99GUI_WARNINGS_AS_ERRORS)
    target_compile_options(c99gui PRIVATE /WX)
  endif()
  # Static CRT: one self-contained .exe.
  set_property(TARGET c99gui PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
else()
  target_compile_options(c99gui PRIVATE -Wall -Wextra -Wpedantic)
  if(C99GUI_WARNINGS_AS_ERRORS)
    target_compile_options(c99gui PRIVATE -Werror)
  endif()
endif()

if(C99GUI_STRICT)
  target_compile_definitions(c99gui PRIVATE STRICT_C99)
endif()

include(CTest)
find_program(C99GUI_SH sh)
if(BUILD_TESTING AND C99GUI_SH)
  add_test(NAME smoke
           COMMAND ${C99GUI_SH} ${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke.sh $<TARGET_FILE:c99gui>)
endif()

install(TARGETS c99gui RUNTIME DESTINATION bin)
