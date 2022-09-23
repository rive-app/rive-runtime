@echo off
pushd %DEPENDENCIES%
@echo off
if not exist ".\harfbuzz" (
    echo "Cloning Harfbuzz."
    git clone https://github.com/harfbuzz/harfbuzz
    pushd harfbuzz
    git checkout 858570b1d9912a1b746ab39fbe62a646c4f7a5b1 .
    popd
)
popd