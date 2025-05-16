set -e

TARGET="host"
if [[ "$OSTYPE" == "darwin"* ]]; then
    DEFAULT_BACKEND=metal
elif [[ "$OSTYPE" == "msys" ]]; then
    DEFAULT_BACKEND=d3d
else
    DEFAULT_BACKEND=gl
fi

NUMBER_OF_PROCESSORS="${NUMBER_OF_PROCESSORS:-$(nproc 2>/dev/null || sysctl -n hw.physicalcpu)}"
if [[ $NUMBER_OF_PROCESSORS > 20 ]]; then
    ARGS=-j6
else
    ARGS=-j4
fi

DIFF_ARGS=

while :; do
    case $1 in
        -u)
            TARGET="unreal"
            DEFAULT_BACKEND=rhi
            ARGS="$ARGS --no-rebuild --no-install"
            shift
        ;;
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
        -a|-a32)
            TARGET="android"
            DEFAULT_BACKEND=gl
            SERIAL="$(adb get-serialno)"
            if [[ "$1" == "-a32" ]]; then
                ARGS="--android-arch arm"
            fi
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
            ARGS="$ARGS --no-rebuild --no-install"
            shift
        ;;
        -H)
            DIFF_ARGS="$DIFF_ARGS -H"
            shift
        ;;
        -t[0-9] | -t[0-9][0-9] )
            DIFF_ARGS="$DIFF_ARGS --threshold=${1: 2}"
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

# Updated to "--no-rebuild --no-install" after the first backend (so we only
# rebuild once).
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
    
    if [ "$REBASELINE" == true ]; then
        echo
        echo "Rebaselining $ID..."
        rm -fr .gold/$ID
        python3 deploy_tests.py gms goldens $ARGS --target=$TARGET --outdir=.gold/$ID --backend=$BACKEND $NO_REBUILD
    else
        echo
        echo "Checking $ID..."
        rm -fr .gold/candidates/$ID
        python3 deploy_tests.py gms goldens $ARGS --target=$TARGET --outdir=.gold/candidates/$ID --backend=$BACKEND $NO_REBUILD
        
        echo
        echo "Checking $ID..."
        rm -fr .gold/diffs/$ID && mkdir -p .gold/diffs/$ID
        python3 diff.py $DIFF_ARGS -g .gold/$ID -c .gold/candidates/$ID -j$NUMBER_OF_PROCESSORS -o .gold/diffs/$ID \
            || open_file .gold/diffs/$ID/index.html
    fi
    
    NO_REBUILD="--no-rebuild --no-install"
done
