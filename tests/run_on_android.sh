set -e

BINARY_PATH="$1"
BINARY_NAME="$(basename $BINARY_PATH)"
shift

./push_console_app_to_android.sh $BINARY_PATH
adb shell "cd /data/local/tmp; ./$BINARY_NAME $@"
