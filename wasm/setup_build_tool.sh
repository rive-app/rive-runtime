# We need to compile without RTTI. In order to do that we need to use a fork
# with a fix to emscripten by @dnfield.
# https://github.com/emscripten-core/emscripten/pull/10914
mkdir -p custom_emcc
cd custom_emcc
if [ ! -d emsdk ]
then
    git clone https://github.com/emscripten-core/emsdk.git
    cd emsdk
    ./emsdk install latest
    ./emsdk activate latest
    source ./emsdk_env.sh
    cd upstream/emscripten
    # add dan's remote
    git remote add dans https://github.com/dnfield/emscripten.git
    git fetch dans
    git checkout -b dansmaster --track dans/master
    # get closure compiler
    npm install
    cd ../../
else
    cd emsdk
fi

source ./emsdk_env.sh