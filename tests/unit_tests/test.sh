#!/bin/bash
set -e

unameOut="$(uname -s)"
case "${unameOut}" in
Linux*) machine=linux ;;
Darwin*) machine=macosx ;;
MINGW*) machine=windows ;;
*) machine="unhandled:${unameOut}" ;;
esac

CONFIG=debug
MATCH=
COVERAGE=
EXTRA_CONFIG=
UTILITY=
TOOLSET_ARG=
while [[ $# -gt 0 ]]; do
  case $1 in
  -m | --match)
    MATCH="$2"
    shift # past argument
    shift # past value
    ;;
  lldb)
    if [[ $machine = "windows" ]]; then
      echo "Warning: Ignoring 'lldb' command-line argument, which does not work on Windows."
    else
      echo Starting debugger...
      if [[ ! -z $UTILITY ]]; then
        echo "Warning: 'lldb' parameter overrides '$UTILITY' parameter."
      fi
      UTILITY='lldb'
    fi
    shift # past argument
    ;;
  memory)
    if [[ $machine = "macosx" ]]; then
      echo Will perform memory checks...
      if [[ ! -z $UTILITY ]]; then
        echo "Warning: 'memory' parameter overrides '$UTILITY' parameter."
      fi
      UTILITY='leaks --atExit --'
    else
      echo "Warning: Ignoring 'memory' command-line argument, which only works on Mac."
    fi
    shift # past argument
    ;;
  asan)
    echo Will perform address sanitization...
    EXTRA_CONFIG=$EXTRA_CONFIG'--with-asan '
    shift # past argument
    ;;
  release)
    CONFIG=release
    shift # past argument
    ;;
  coverage)
    if [[ $machine = "macosx" ]]; then
      COVERAGE=true
    else
      echo "Warning: Ignoring 'coverage' command-line argument, which only works on Mac."
    fi
    shift # past argument
    ;;
  rebaseline)
    export REBASELINE_SILVERS=true
    shift # past argument
    ;;
  clean)
    rm -fR out
    shift # past argument
    ;;
  --toolset=*)
    # Windows build specifies a default toolset but allow it to be overridden.
    echo "Toolset has been specified."
    TOOLSET_ARG=$1
    shift
    ;;
  --with_vulkan)
    echo "Vulkan is added"
    EXTRA_CONFIG=$EXTRA_CONFIG'--with_vulkan '
    shift
    ;;
  *)
    # We could pass any unrecognized arguments through instead of just eating them
    echo "Warning: unrecognized argument '$1'"
    shift # past argument
    ;;
  esac
done

if [[ $machine = "windows" ]] && [[ -z $TOOLSET_ARG ]]; then
  echo "Using default Windows toolset of clang"
  TOOLSET_ARG="--toolset=clang"
fi

pushd ../../
RUNTIME=$PWD
popd

BUILD_RIVE_COMMANDS="$CONFIG --with_rive_tools --with_rive_audio=external --with_rive_scripting --no_ffp_contract $TOOLSET_ARG $EXTRA_CONFIG"
$RUNTIME/build/build_rive.sh $BUILD_RIVE_COMMANDS

rm -fR silvers/tarnished
mkdir -p silvers/tarnished

OUT_DIR="out/$CONFIG"

# Actually run the unit tests
$UTILITY $OUT_DIR/unit_tests "$MATCH"

if [[ $COVERAGE = "true" ]]; then
  if [[ $machine = "macosx" ]]; then
    xcrun llvm-profdata merge -sparse default.profraw -o default.profdata
    xcrun llvm-cov report $OUT_DIR/unit_tests -instr-profile=default.profdata
    # xcrun llvm-cov export out/debug/unit_tests -instr-profile=default.profdata -format=text >coverage.json
    xcrun llvm-cov export out/debug/unit_tests -instr-profile=default.profdata -format=lcov >coverage.txt
    sed -i '' -e 's?'$RUNTIME'?packages/runtime?g' coverage.txt
  else
    echo "'coverage' command line argument was specified but it only works on Mac so it was ignored"
  fi
fi  
