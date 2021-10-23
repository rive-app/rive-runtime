
OPTION=$1
unameSystem="$(uname -s)"
case "${unameSystem}" in
    Linux*)     system=linux;;
    Darwin*)    system=macosx;;
    *)          system="unknown:${unameSystem}"
esac

if [ "$OPTION" = 'help' ]; then
    echo build.sh - build debug library
    echo build.sh clean - clean the build
    echo build.sh release - build release library
elif [ "$OPTION" = "release" ]; then
    ./build/$system/bin/release/rive_viewer
else
    ./build/$system/bin/debug/rive_viewer
fi
