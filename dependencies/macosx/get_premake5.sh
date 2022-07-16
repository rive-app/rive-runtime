#!/bin/sh
set -e
if [[ -z "${DEPENDENCIES}" ]]; then
    echo "DEPENDENCIES env variable must be set. This script is usually called by other scripts."
    exit 1
fi

mkdir -p $DEPENDENCIES/bin
echo Downloading Premake5
curl https://github.com/premake/premake-core/releases/download/v5.0.0-beta1/premake-5.0.0-beta1-macosx.tar.gz -L -o $DEPENDENCIES//bin/premake_macosx.tar.gz
cd $DEPENDENCIES/bin
# Export premake5 into bin
tar -xvf premake_macosx.tar.gz 2>/dev/null
# Delete downloaded archive
rm premake_macosx.tar.gz
