set -e

BINARY_PATH="$1"
BINARY_NAME="$(basename $BINARY_PATH)"

adb shell rm -f /sdcard/$BINARY_NAME
adb shell rm -f /data/local/tmp/$BINARY_NAME

(adb push "$BINARY_PATH" "//sdcard" && adb shell "
set -e
mv /sdcard/$BINARY_NAME /data/local/tmp/
") || adb push "$BINARY_PATH" "//data/local/tmp"

adb shell "
set -e
cd /data/local/tmp
chmod +x $BINARY_NAME
"
