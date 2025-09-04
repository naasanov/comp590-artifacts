#!/usr/bin/env bash
###
#
#  @file build.sh
#  @copyright 2023-2024 Bordeaux INP, CNRS (LaBRI UMR 5800), Inria,
#                       Univ. Bordeaux. All rights reserved.
#
#  @version 1.2.4
#  @author Mathieu Faverge
#  @author Florent Pruvost
#  @date 2024-05-29
#
###
set -e
set -x


# specific compilation flags on linux for linters
if [[ "$SYSTEM" == "linux" ]]; then
    FLAGS="-O0 -g -Wall -fPIC -fno-inline --coverage"
#    SCAN_BUILD="scan-build  -v -plist --intercept-first --analyze-headers -o analyzer_reports "
    SCAN_BUILD="scan-build  -v -plist -analyze-headers -o analyzer_reports "
fi

case $SYSTEM in

  linux)
    conda remove -n openvibe --all
    conda env update --file conda/env_linux.yaml --prune
    ;;

  macosx)
    conda remove -n openvibe --all
    conda env update --file conda/env_osx.yaml --prune
   ;;

  *)
    echo "The SYSTEM environment variable is $SYSTEM. It is not one of: linux, macosx, windows -> exit 1."
    exit 1
    ;;
esac

conda activate openvibe
rm -rf build
mkdir build
cd build

# Configure with CMake
case $SYSTEM in

  linux)
    # configure the cmake project
    cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
    ;;

  macosx)
   # clang is used on macosx and it is not compatible with MORSE_ENABLE_COVERAGE=ON
    cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
   ;;

  *)
    echo "The SYSTEM environment variable is $SYSTEM. It is not one of: linux, macosx, windows -> exit 1."
    exit 1
    ;;
esac

# Compile openvibe
# $TEST_PIPELINE_ONLY defined in common.yml
# $BUILD_QT6_BRANCH defined in common.yml

TARGET=""

if [[ "$TEST_PIPELINE_ONLY" == "true" ]]; then
    TARGET=" openvibe"
fi
if [[ "$BUILD_QT6_BRANCH" == "true" ]]; then
    TARGET=""
fi

${SCAN_BUILD}make -j3 ${TARGET} 2>&1 | tee build.log
# sed '/^scan-build/d' build.log > test; mv test build.log 

# Install
if [[ "$TEST_PIPELINE_ONLY" == "false" ]]; then
    make install
fi
