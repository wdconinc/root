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

target_sources(Core PRIVATE src/TEmscriptenSystem.cxx)
target_include_directories(Core PRIVATE inc)

ROOT_INSTALL_HEADERS()

# ── Embind JS bindings ────────────────────────────────────────────────────────
# Build a separate WASM+JS bundle 'root.js' that wraps the key ROOT classes
# for direct use from JavaScript / JSROOT.  This target links all the ROOT
# static libraries that have been built and adds the Embind glue layer.
#
# The bundle is placed in ${CMAKE_BINARY_DIR}/js/root.js (and root.wasm) so
# that the GitHub Pages docs/ can reference it.

option(ROOT_WASM_BINDINGS "Build Emscripten/Embind JS bindings bundle (root.js)" ON)

if(ROOT_WASM_BINDINGS)
  # Collect the ROOT libraries that are built in the minimal configuration.
  # Order matters for the linker; more-derived before more-base.
  set(ROOT_WASM_LINK_LIBS
    Hist       # TH1F, TH2F
    MathCore   # TMath, TRandom3
    RIO        # TFile, RRawFile
    Core       # TROOT, TNamed, TAxis, TBufferJSON
  )

  add_executable(root_wasm_bindings
    src/RootWasmBindings.cxx
  )

  set_target_properties(root_wasm_bindings PROPERTIES
    OUTPUT_NAME  "root"
    SUFFIX       ".js"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/js"
  )

  target_include_directories(root_wasm_bindings PRIVATE
    ${CMAKE_SOURCE_DIR}/core/wasm/inc
  )

  target_link_libraries(root_wasm_bindings PRIVATE
    ${ROOT_WASM_LINK_LIBS}
  )

  # Embind requires --bind linker flag
  target_link_options(root_wasm_bindings PRIVATE
    "--bind"
    "SHELL:-sEXIT_RUNTIME=0"
    # Export the Embind-generated factory plus the C runtime init
    "SHELL:-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString']"
    # Show all linker errors (default limit is 20)
    "SHELL:-Wl,--error-limit=0"
  )

  install(FILES
    "${CMAKE_BINARY_DIR}/js/root.js"
    "${CMAKE_BINARY_DIR}/js/root.wasm"
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    OPTIONAL
  )
endif()

