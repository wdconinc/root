# Copyright (C) 1995-2025, Rene Brun and Fons Rademakers.
# All rights reserved.
#
# For the licensing terms see $ROOTSYS/LICENSE.
# For the list of contributors see $ROOTSYS/README/CREDITS.

############################################################################
# CMakeLists.txt file for building ROOT core/wasm package
############################################################################

# This sub-directory is only compiled when targeting WebAssembly with
# Emscripten.  The guard is placed here as a safety net; the parent
# CMakeLists.txt is responsible for only calling add_subdirectory(wasm)
# when EMSCRIPTEN is defined.
if(NOT EMSCRIPTEN)
  return()
endif()

set_property(TARGET Core APPEND PROPERTY DICT_HEADERS TEmscriptenSystem.h)
target_sources(Core PRIVATE src/TEmscriptenSystem.cxx)
target_include_directories(Core PRIVATE inc)

ROOT_INSTALL_HEADERS()
