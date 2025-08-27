echo $RIVE_RENDERER_DIR
pushd $RIVE_RENDERER_DIR > /dev/null
echo Entered dir: $RIVE_RENDERER_DIR

# Clean old shader files 
dstShaderDir=$1
echo Cleaning dir: $dstShaderDir
rm -r $dstShaderDir

# Find ply
curr_dir=`pwd`
found_ply_dir=`find ${curr_dir} -type d -wholename "*/dependencies/*/ply-*/src" -print`
echo found ply: $found_ply_dir

# Invoke make
make -C ./src/shaders -j32 OUT=$dstShaderDir FLAGS="-p ${found_ply_dir} --human-readable" spirv-binary 
echo Exiting dir: $RIVE_RENDERER_DIR

# Done 
popd > /dev/null
echo Done compiling shaders.
