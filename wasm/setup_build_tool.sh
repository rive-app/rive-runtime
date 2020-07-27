# We need to compile without RTTI. In order to do that we need to use a fork
# with a fix to emscripten by @dnfield.
# https://github.com/emscripten-core/emscripten/pull/10914
mkdir -p custom_emcc
cd custom_emcc
if [ ! -d emsdk ]
then
    command -v cmake > /dev/null || brew install cmake
    git clone https://github.com/emscripten-core/emsdk.git
    cd emsdk
    ./emsdk install latest
    ./emsdk activate latest
    ./emsdk install sdk-upstream-master-64bit
    ./emsdk activate sdk-upstream-master-64bit
    source ./emsdk_env.sh
    cd emscripten/master
    git remote add luigis https://github.com/luigi-rosso/emscripten.git
    git fetch luigis
    git checkout -b luigismaster --track luigis/master
    npm install
    # add dan's remote
    # git remote add dans https://github.com/dnfield/emscripten.git
    # git fetch dans
    # git checkout -b dansmaster --track dans/master
    # get closure compiler
    npm install
    cd ../../
else
    cd emsdk
    source ./emsdk_env.sh
fi

# Note that you'll need to cd into custom_emcc/emsdk and run source
# ./emsdk_env.sh manually from the shell where you'll call build_wasm.sh.