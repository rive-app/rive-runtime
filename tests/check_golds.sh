set -e

TARGET="host"
if [[ "$OSTYPE" == "darwin"* ]]; then
    DEFAULT_BACKEND=metal
elif [[ "$OSTYPE" == "msys" ]]; then
    DEFAULT_BACKEND=d3d
else
    DEFAULT_BACKEND=gl
fi
ARGS=

while :; do
    case $1 in
        -i)
            TARGET="ios"
            DEFAULT_BACKEND=metal
            UDID="$(idevice_id -l)" # brew install ideviceinstaller
            shift
            ;;
        -s)
            TARGET="iossim"
            DEFAULT_BACKEND=metal
            UDID="$(xcrun simctl list devices | grep '(Booted)' | sed 's/^[^(]*(\([A-Z0-9\-]*\)) (Booted).*$/\1/')"
            shift
            ;;
        -a)
            TARGET="android"
            DEFAULT_BACKEND=gl
            SERIAL="$(adb get-serialno)"
            shift
            ;;
        -R)
            REBASELINE=true
            shift
            ;;
        -r)
            ARGS="$ARGS --remote"
            shift
            ;;
        -v)
            ARGS="$ARGS --verbose"
            shift
            ;;
        -n)
            ARGS="$ARGS --no-rebuild"
            shift
            ;;
        *)
            break
            ;;
    esac
done

open_file() {
    if which start >/dev/null; then # windows
        start $1
    elif which open >/dev/null; then # mac
        open $1
    else
        gnome-open $1
    fi
}

# Updated to "--no-rebuild" after the first backend (so we only rebuild once).
NO_REBUILD=

for BACKEND in "${@:-$DEFAULT_BACKEND}"
do
    ID=$BACKEND
    if ! [ -z "$RIVE_GPU" ]; then
        ID="$RIVE_GPU/$BACKEND"
    fi
    if [[ "$TARGET" == "ios" ]]; then
        ID="ios_$UDID/$BACKEND"
    elif [[ "$TARGET" == "iossim" ]]; then
        ID="iossim_$UDID/$BACKEND"
    elif [[ "$TARGET" == "android" ]]; then
        ID="android_$SERIAL/$BACKEND"
    fi

    NUMBER_OF_PROCESSORS="${NUMBER_OF_PROCESSORS:-$(nproc 2>/dev/null || sysctl -n hw.physicalcpu)}"
    if [[ $NUMBER_OF_PROCESSORS > 20 ]]; then
        GOLDEN_JOBS=6
    else
        GOLDEN_JOBS=4
    fi

    if [ "$REBASELINE" == true ]; then
        echo
        echo "Rebaselining $ID..."
        rm -fr .gold/$ID
        python3 deploy_tools.py gms goldens -j$GOLDEN_JOBS $ARGS --target=$TARGET --outdir=.gold/$ID --backend=$BACKEND $NO_REBUILD
    else
        echo
        echo "Checking $ID..."
        rm -fr .gold/candidates/$ID
        python3 deploy_tools.py gms goldens -j$GOLDEN_JOBS $ARGS --target=$TARGET --outdir=.gold/candidates/$ID --backend=$BACKEND $NO_REBUILD

        echo
        echo "Checking $ID..."
        rm -fr .gold/diffs/$ID && mkdir -p .gold/diffs/$ID
        python3 diff.py -g .gold/$ID -c .gold/candidates/$ID -j$NUMBER_OF_PROCESSORS -o .gold/diffs/$ID \
            || open_file .gold/diffs/$ID/index.html
    fi

    NO_REBUILD=--no-rebuild
done
