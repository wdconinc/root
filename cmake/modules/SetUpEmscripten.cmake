# Copyright (C) 1995-2025, Rene Brun and Fons Rademakers.
# All rights reserved.
#
# For the licensing terms see $ROOTSYS/LICENSE.
# For the list of contributors see $ROOTSYS/README/CREDITS.

# Platform setup for Emscripten / WebAssembly builds.
# Included by CheckCompiler.cmake when EMSCRIPTEN is defined.

set(ROOT_PLATFORM wasm32)
set(ROOT_ARCHITECTURE wasm32)

# Emscripten compiles to a WASM binary + a JS loader; there are no shared libs
# in the traditional sense.  Force static linking for all ROOT libraries.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Forced OFF for WASM builds" FORCE)

# Emscripten does not support -rdynamic.  The JIT / ACLiC path is unavailable
# in WASM anyway, so there is no need for run-time symbol export.
string(REPLACE "-rdynamic" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
string(REPLACE "-rdynamic" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")

# Enable warnings consistent with the Linux/macOS builds.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -W -Woverloaded-virtual -fsigned-char")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wall -W")

# Emscripten does not support RPATH; silence CMake warnings about it.
set(CMAKE_SKIP_RPATH TRUE CACHE BOOL "No RPATH on WASM" FORCE)
set(CMAKE_SKIP_BUILD_RPATH TRUE CACHE BOOL "No build RPATH on WASM" FORCE)
set(CMAKE_SKIP_INSTALL_RPATH TRUE CACHE BOOL "No install RPATH on WASM" FORCE)

# Enable Emscripten fetch API so that TWebFile can use HTTP range requests.
set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS}    -s FETCH=1")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -s FETCH=1")

# Allow memory to grow so ROOT heaps do not hit the 256 MB default cap.
set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS}    -s ALLOW_MEMORY_GROWTH=1")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -s ALLOW_MEMORY_GROWTH=1")

# Use WASM BigInt so that 64-bit integers are faithfully represented in JS.
set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS}    -s WASM_BIGINT=1")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -s WASM_BIGINT=1")

message(STATUS "Configuring ROOT for WebAssembly (Emscripten ${EMSCRIPTEN_VERSION})")
