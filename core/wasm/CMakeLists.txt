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

# ── Auto-generated bindings ───────────────────────────────────────────────────
# RootWasmBindings_generated.cxx is produced by:
#   python3 core/wasm/tools/gen_wasm_bindings.py \
#       --manifest core/wasm/bindings.yml      \
#       --output   core/wasm/src/RootWasmBindings_generated.cxx
#
# The generator uses libclang to parse ROOT headers and emit EMSCRIPTEN_BINDINGS
# blocks without per-class boilerplate.  To add a new class, add one entry to
# bindings.yml and re-run the generator, then commit the updated .cxx file.
# (libclang is a developer tool — it is NOT required at WASM build time.)

if(ROOT_WASM_BINDINGS)
  # Collect the ROOT libraries that are built in the minimal configuration.
  # Order matters for the linker; more-derived before more-base.
  set(ROOT_WASM_LINK_LIBS
    Tree       # TTree, TBranch (must come before Hist/RIO)
    Hist       # TH1F, TH2F, TGraph, TF1
    Physics    # TVector3, TLorentzVector (math/physics)
    MathCore   # TMath, TRandom3
    RIO        # TFile, RRawFile
    Core       # TROOT, TNamed, TAxis
  )

  add_executable(root_wasm_bindings
    src/RootWasmBindings.cxx
    src/RootWasmBindings_generated.cxx
  )

  set_target_properties(root_wasm_bindings PROPERTIES
    OUTPUT_NAME  "root"
    SUFFIX       ".js"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/js"
  )

  target_include_directories(root_wasm_bindings PRIVATE
    ${CMAKE_SOURCE_DIR}/core/wasm/inc
    ${CMAKE_SOURCE_DIR}/math/physics/inc    # TVector3, TLorentzVector
  )

  target_link_libraries(root_wasm_bindings PRIVATE
    ${ROOT_WASM_LINK_LIBS}
  )

  # Embind requires --bind linker flag
  target_link_options(root_wasm_bindings PRIVATE
    "--bind"
    # Wrap the module in a factory function (createROOT()) so it can be
    # loaded asynchronously and used from ES modules / Node.js without
    # polluting the global scope.
    "SHELL:-sMODULARIZE=1"
    "SHELL:-sEXPORT_NAME=createROOT"
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

