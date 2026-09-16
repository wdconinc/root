# Copyright (C) 1995-2019, Rene Brun and Fons Rademakers.
# All rights reserved.
#
# For the licensing terms see $ROOTSYS/LICENSE.
# For the list of contributors see $ROOTSYS/README/CREDITS.

############################################################################
# CMakeLists.txt file for building ROOT rootx package
# @author Pere Mato, CERN
############################################################################

ROOT_EXECUTABLE(root src/rootx.cxx)

if (CMAKE_SYSTEM_NAME MATCHES FreeBSD)
  target_link_libraries(root PRIVATE util procstat)
endif()

generateHeader(root
  ${PROJECT_SOURCE_DIR}/core/base/src/root-argparse.py
  ${PROJECT_BINARY_DIR}/ginclude/rootCommandLineOptionsHelp.h
)

target_include_directories(root PRIVATE
  ${PROJECT_SOURCE_DIR}/core/foundation/inc
  ${PROJECT_SOURCE_DIR}/core/base/inc
  ${PROJECT_SOURCE_DIR}/core/clib/inc    # for snprintf.h
  ${PROJECT_SOURCE_DIR}/core/meta/inc    # for TGenericClassInfo.h
  ${PROJECT_BINARY_DIR}/ginclude         # for RConfigure.h and rootCommandLineOptionsHelp.h
)
