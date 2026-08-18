set -e

# Self-hosted boxes run several runner slots with separate workspaces; a
# shared RIVE_GOLD_DIR keeps one baseline per machine.
GOLD="${RIVE_GOLD_DIR:-.gold}"

TESTS="gms goldens"

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
        --gms)
            TESTS="gms"
            shift
        ;;
        -u)
            TARGET="unreal"
            if [[ "$OSTYPE" == "darwin"* ]]; then
                DEFAULT_BACKEND=metalatomic
            elif [[ "$OSTYPE" == "msys" ]]; then
                DEFAULT_BACKEND=d3d12
            else
                DEFAULT_BACKEND=vkatomic
            fi
            if [ -z "$RIVE_UNREAL_ENGINE" ]; then
                ARGS="$ARGS --no-rebuild --no-install"   # expect a prebuilt package
            fi
            shift
        ;;
        -ua)
            TARGET="unreal_android"
            # msaa is what we target on android. atomics needs pixel shader
            # UAVs, which mobile handles badly, and it hangs on adreno.
            DEFAULT_BACKEND=vkmsaa
            ARGS="$ARGS --no-rebuild"
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
            SERIAL="$(adb get-serialno | sed 's/[:.]/_/g')"
            if [[ "$1" == "-a32" ]]; then
                ARGS="--android-arch arm"
            fi
            shift
        ;;
        -w)
            TARGET="webbrowser"
            DEFAULT_BACKEND=gl
            shift
        ;;
        -ws)
            TARGET="webserver"
            DEFAULT_BACKEND=gl
            shift
        ;;
        -wa)
            TARGET="webbrowserandroid"
            DEFAULT_BACKEND=gl
            shift
        ;;
        -wsa)
            TARGET="webserverandroid"
            DEFAULT_BACKEND=gl
            shift
        ;;
        -R)
            REBASELINE=true
            shift
        ;;
        -r)
            ARGS="$ARGS --release"
            shift
        ;;
        --remote)
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
        -m)
            ARGS="$ARGS --match $2"
            shift 2
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
    # Headless CI has no browser; opening the diff report there hangs the runner.
    if [ -n "$CI" ]; then
        echo "CI: skipping report open ($1)"
        return 0
    fi
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
FAILED=()

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
    elif [[ "$TARGET" != "host" ]]; then
        ID="$TARGET/$BACKEND"
    fi
    
    DEPLOYED=true
    if [ "$REBASELINE" == true ]; then
        echo
        echo "Rebaselining $ID..."
        rm -fr $GOLD/$ID
        python3 deploy_tests.py $TESTS $ARGS --target=$TARGET --outdir=$GOLD/$ID --backend=$BACKEND $NO_REBUILD \
            || DEPLOYED=false
    else
        echo
        echo "Deploying $ID..."
        rm -fr $GOLD/candidates/$ID
        python3 deploy_tests.py $TESTS $ARGS --target=$TARGET --outdir=$GOLD/candidates/$ID --backend=$BACKEND $NO_REBUILD \
            || DEPLOYED=false

        if [ "$DEPLOYED" == true ]; then
            echo
            echo "Diffing $ID..."
            rm -fr $GOLD/diffs/$ID && mkdir -p $GOLD/diffs/$ID
            if ! python3 diff.py $DIFF_ARGS -g $GOLD/$ID -c $GOLD/candidates/$ID -j$NUMBER_OF_PROCESSORS -o $GOLD/diffs/$ID; then
                open_file $GOLD/diffs/$ID/index.html
                FAILED+=("$ID (diff)")
            fi
        fi
    fi

    NO_REBUILD="--no-rebuild --no-install"

    if [ "$DEPLOYED" != true ]; then
        echo
        echo "FAILED to deploy $ID."
        FAILED+=("$ID (deploy)")
    fi
done

if [ ${#FAILED[@]} -gt 0 ]; then
    echo
    echo "${#FAILED[@]} backend(s) failed:"
    for ID in "${FAILED[@]}"; do
        echo "    $ID"
    done
    exit 1
fi
