set -e
for DEVICE in $(adb devices | grep -v '^List' | cut -f1)
do
    ANDROID_SERIAL=$DEVICE "$@" &
done
wait
