set -e

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
            DEFAULT_BACKEND=rhi
            if [ -z "$RIVE_UNREAL_ENGINE" ]; then
                ARGS="$ARGS --no-rebuild --no-install"   # expect a prebuilt package
            else
                ARGS="$ARGS --no-rebuild"                # build & package on demand
            fi
            shift
        ;;
        -ua)
            TARGET="unreal_android"
            DEFAULT_BACKEND=rhi
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
    elif [[ "$TARGET" != "host" ]]; then
        ID="$TARGET/$BACKEND"
    fi
    
    # deploy_tests expands unreal's "rhi" meta-backend into the RHIs the target
    # supports, writing one sibling directory per RHI (e.g. .gold/unreal/d3d11,
    # .gold/unreal/vulkan) rather than the single .gold/$ID.
    RHI_FANOUT=false
    if [[ "$TARGET" == unreal* ]] && [[ "$BACKEND" == "rhi" ]]; then
        RHI_FANOUT=true
    fi

    if [ "$REBASELINE" == true ]; then
        echo
        echo "Rebaselining $ID..."
        if [ "$RHI_FANOUT" == true ]; then
            rm -fr .gold/$TARGET
        else
            rm -fr .gold/$ID
        fi
        python3 deploy_tests.py $TESTS $ARGS --target=$TARGET --outdir=.gold/$ID --backend=$BACKEND $NO_REBUILD
    else
        echo
        echo "Deploying $ID..."
        if [ "$RHI_FANOUT" == true ]; then
            rm -fr .gold/candidates/$TARGET
        else
            rm -fr .gold/candidates/$ID
        fi
        python3 deploy_tests.py $TESTS $ARGS --target=$TARGET --outdir=.gold/candidates/$ID --backend=$BACKEND $NO_REBUILD

        # Diff each directory deploy_tests produced (one per RHI when fanning out).
        if [ "$RHI_FANOUT" == true ]; then
            DIFF_IDS=()
            for RHI_DIR in .gold/candidates/$TARGET/*/; do
                DIFF_IDS+=("$TARGET/$(basename $RHI_DIR)")
            done
        else
            DIFF_IDS=("$ID")
        fi

        for DIFF_ID in "${DIFF_IDS[@]}"; do
            echo
            echo "Diffing $DIFF_ID..."
            rm -fr .gold/diffs/$DIFF_ID && mkdir -p .gold/diffs/$DIFF_ID
            python3 diff.py $DIFF_ARGS -g .gold/$DIFF_ID -c .gold/candidates/$DIFF_ID -j$NUMBER_OF_PROCESSORS -o .gold/diffs/$DIFF_ID \
                || open_file .gold/diffs/$DIFF_ID/index.html
        done
    fi

    NO_REBUILD="--no-rebuild --no-install"
done
