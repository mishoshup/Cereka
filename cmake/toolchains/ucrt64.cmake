# -----------------------------------------------
# UCRT64 MinGW-w64 cross-compile toolchain
# -----------------------------------------------

# Target system
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 1)

# Target architecture
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Compiler paths
set(UCRT64_PREFIX /opt/llvm-mingw/llvm-mingw-ucrt/bin)

set(CMAKE_C_COMPILER   ${UCRT64_PREFIX}/x86_64-w64-mingw32-clang)
set(CMAKE_CXX_COMPILER ${UCRT64_PREFIX}/x86_64-w64-mingw32-clang++)

# Tools
set(CMAKE_RC_COMPILER  ${UCRT64_PREFIX}/x86_64-w64-mingw32-windres)
set(CMAKE_AR           ${UCRT64_PREFIX}/x86_64-w64-mingw32-ar)
set(CMAKE_RANLIB       ${UCRT64_PREFIX}/x86_64-w64-mingw32-ranlib)
set(CMAKE_LINKER       ${UCRT64_PREFIX}/x86_64-w64-mingw32-ld)

# Optional flags
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
