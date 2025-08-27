SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
export DEPENDENCIES_SCRIPTS=$SCRIPT_DIR
export DEPENDENCIES=$SCRIPT_DIR/cache
mkdir -p $DEPENDENCIES
