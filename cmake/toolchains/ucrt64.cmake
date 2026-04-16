# UCRT64 MinGW-w64 cross-compile toolchain (cmake/toolchains/ucrt64.cmake)
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Force SDL vendored flags when using this toolchain (only affects UCRT cross-compiles)
set(SDLTTF_VENDORED   ON  CACHE BOOL "" FORCE)
set(SDLIMAGE_VENDORED ON  CACHE BOOL "" FORCE)
set(SDLIMAGE_AVIF     OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_JXL      OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TIF      OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_WEBP     OFF CACHE BOOL "" FORCE)

# Try to locate llvm-mingw wrapper compilers. Prefer ucrt-named wrappers, fall back to mingw32 names and package clang
find_program(UCRT_CC x86_64-w64-ucrt-clang x86_64-w64-mingw32-clang x86_64-w64-windows-gnu-clang clang HINTS /opt/llvm-mingw-ucrt/bin /opt/llvm-mingw/bin /opt/llvm-mingw/llvm-mingw-ucrt/bin /opt/llvm-mingw/llvm-mingw-ucrt/bin /usr/bin /usr/local/bin)
find_program(UCRT_CXX x86_64-w64-ucrt-clang++ x86_64-w64-mingw32-clang++ x86_64-w64-windows-gnu-clang++ clang++ HINTS /opt/llvm-mingw-ucrt/bin /opt/llvm-mingw/bin /opt/llvm-mingw/llvm-mingw-ucrt/bin /opt/llvm-mingw/llvm-mingw-ucrt/bin /usr/bin /usr/local/bin)
find_program(UCRT_WINDRES x86_64-w64-ucrt-windres x86_64-w64-mingw32-windres llvm-windres windres HINTS /opt/llvm-mingw-ucrt/bin /opt/llvm-mingw/bin /opt/llvm-mingw/llvm-mingw-ucrt/bin /opt/llvm-mingw/llvm-mingw-ucrt/bin /usr/bin /usr/local/bin)

if(UCRT_CC)
  set(CMAKE_C_COMPILER "${UCRT_CC}")
  message(STATUS "Using C compiler: ${UCRT_CC}")
else()
  message(FATAL_ERROR "No suitable x86_64-w64-* clang wrapper found. Install llvm-mingw ucrt package or set CMAKE_C_COMPILER")
endif()
if(UCRT_CXX)
  set(CMAKE_CXX_COMPILER "${UCRT_CXX}")
  message(STATUS "Using CXX compiler: ${UCRT_CXX}")
else()
  message(FATAL_ERROR "No suitable x86_64-w64-* clang++ wrapper found. Install llvm-mingw ucrt package or set CMAKE_CXX_COMPILER")
endif()
if(UCRT_WINDRES)
  set(CMAKE_RC_COMPILER "${UCRT_WINDRES}")
  message(STATUS "Using windres: ${UCRT_WINDRES}")
endif()

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Try to detect sysroot from the wrapper compiler and set CMAKE_SYSROOT so host headers aren't used
execute_process(COMMAND ${CMAKE_C_COMPILER} --print-sysroot
  OUTPUT_VARIABLE _sroot OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(_sroot)
  message(STATUS "Detected sysroot: ${_sroot}")
  set(CMAKE_SYSROOT "${_sroot}")
  list(APPEND CMAKE_FIND_ROOT_PATH "${_sroot}")
else()
  message(STATUS "No sysroot detected from ${CMAKE_C_COMPILER}; ensure wrapper is in PATH or set CMAKE_SYSROOT manually")
endif()

# If llvm-mingw is installed under /opt/llvm-mingw/llvm-mingw-ucrt, prefer it as the sysroot and root path
if(EXISTS "/opt/llvm-mingw/llvm-mingw-ucrt")
  set(_LM_SYSROOT "/opt/llvm-mingw/llvm-mingw-ucrt")
  message(STATUS "Forcing CMAKE_SYSROOT=${_LM_SYSROOT}")
  set(CMAKE_SYSROOT "${_LM_SYSROOT}")
  list(APPEND CMAKE_FIND_ROOT_PATH "${_LM_SYSROOT}")
elseif(EXISTS "/opt/llvm-mingw-ucrt")
  list(APPEND CMAKE_FIND_ROOT_PATH "/opt/llvm-mingw-ucrt")
elseif(EXISTS "/opt/llvm-mingw")
  list(APPEND CMAKE_FIND_ROOT_PATH "/opt/llvm-mingw")
endif()

# Force-disable POSIX mmap/mprotect detection for harfbuzz when cross-compiling to Windows
# HarfBuzz's CMake uses check_include_file(sys/mman.h) and check_function_exists(mmap,mprotect).
# On this cross-target sysroot those tests can be misleading; explicitly set them off.
set(HAVE_SYS_MMAN_H 0 CACHE BOOL "Force disable sys/mman.h" FORCE)
set(HAVE_MMAP 0 CACHE BOOL "Force disable mmap" FORCE)
set(HAVE_MPROTECT 0 CACHE BOOL "Force disable mprotect" FORCE)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Uncomment to force an explicit sysroot if needed
# execute_process(COMMAND ${CMAKE_C_COMPILER} --print-sysroot OUTPUT_VARIABLE _sroot OUTPUT_STRIP_TRAILING_WHITESPACE)
# if(_sroot)
#   set(CMAKE_SYSROOT "${_sroot}")
# endif()
