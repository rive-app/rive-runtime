#!/bin/sh

set -e

mkdir -p dependencies
cd dependencies

echo
if [ ! -d dawn ]; then
	echo "Cloning Dawn..."
    git clone https://dawn.googlesource.com/dawn
else
    echo "Already have Dawn; updating it..."
    git -C dawn fetch origin
fi

cd dawn

echo
echo Checking out code...
git reset --hard
git checkout 211333b2e3e429c3508f25c81c547f602adf448c
cp scripts/standalone.gclient .gclient
gclient sync -f -D

echo
echo Manually applying patches to suppress compiler warnings...
pushd build
git apply - << 'EOF'
diff --git a/config/compiler/BUILD.gn b/config/compiler/BUILD.gn
index 6a96f9b6f..4c1387628 100644
--- a/config/compiler/BUILD.gn
+++ b/config/compiler/BUILD.gn
@@ -336,6 +336,7 @@ config("compiler") {
   # categories here, add it to the associated file to keep this shared config
   # smaller.
   if (is_win) {
+    defines += [ "_SILENCE_CXX20_OLD_SHARED_PTR_ATOMIC_SUPPORT_DEPRECATION_WARNING" ]
     configs += [ "//build/config/win:compiler" ]
   } else if (is_android) {
     configs += [ "//build/config/android:compiler" ]
EOF
popd

echo
echo Generating out/release/args.gn...
gn gen --args='is_debug=false dawn_complete_static_libs=true use_custom_libcxx=false dawn_use_swiftshader=false angle_enable_swiftshader=false' out/release

echo
echo Compiling...
ninja -C out/release -j20 webgpu_dawn_static cpp proc_static
