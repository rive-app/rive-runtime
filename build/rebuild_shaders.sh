if [ -z "$RIVE_RENDERER_DIR" ]; then
    echo "RIVE_RENDERER_DIR environment variable needs to be set for rebuild_shaders to run"
    exit 1
fi

echo $RIVE_RENDERER_DIR
pushd $RIVE_RENDERER_DIR > /dev/null
echo Entered dir: $RIVE_RENDERER_DIR

# Clean old shader files 
dst_shader_dir=$1
echo Cleaning dir: $dst_shader_dir
rm -r $dst_shader_dir

make_flags="--human-readable"

# Find ply
curr_dir=`pwd`
found_ply_dir=`find ${curr_dir} -type d -wholename "*/dependencies/*/ply-*/src" -print`
if [ -n "$found_ply_dir" ]; then
    echo found ply: $found_ply_dir
    make_flags="-p ${found_ply_dir} ${make_flags}"
fi

# Invoke make
make -C ./src/shaders -j32 OUT=$dst_shader_dir FLAGS="${make_flags}" spirv-binary 
echo Exiting dir: $RIVE_RENDERER_DIR

# Done 
popd > /dev/null
echo Done compiling shaders.
