#!/bin/bash

# make sure we stop if we encounter an error
set -e

# required envs
RIVE_RUNTIME_DIR="${RIVE_RUNTIME_DIR:=../..}"
SKIA_DIR_NAME="${SKIA_DIR_NAME:=skia}"
SKIA_REPO=${SKIA_REPO:-https://github.com/rive-app/skia}
SKIA_BRANCH=${SKIA_BRANCH:-rive}
SKIA_COMMIT=${SKIA_COMMIT}
COMPILE_TARGET="${COMPILE_TARGET:-$(uname -s)_$(uname -m)}"
CACHE_NAME="${CACHE_NAME:=skia}"
OUTPUT_CACHE="${OUTPUT_CACHE:=out}"
ARCHIVE_CONTENTS_NAME="${ARCHIVE_CONTENTS_NAME:=archive_contents}"

# lets just make sure this exists, or fail
if [[ ! -d $RIVE_RUNTIME_DIR ]]; then
    echo "Cannot find $RIVE_RUNTIME_DIR, bad setup"
    exit 1
fi

# computed environment variables
SKIA_DEPENDENCIES_DIR="$RIVE_RUNTIME_DIR/skia/dependencies"
SKIA_DIR="$SKIA_DEPENDENCIES_DIR/$SKIA_DIR_NAME"

# gotta switch into a non .git folder to check the remote repo's hash
# this avoid issues with corrupted git repos throwing irrelevant errors
pushd ~
SKIA_COMMIT_HASH="$(git ls-remote $SKIA_REPO $SKIA_BRANCH | awk '{print $1}')"
popd

ARCHIVE_CONTENTS_PATH="$SKIA_DIR/$ARCHIVE_CONTENTS_NAME"
echo $ARCHIVE_CONTENTS_PATH

ARCHIVE_CONTENTS="missing"
if test -f "$ARCHIVE_CONTENTS_PATH"; then
    ARCHIVE_CONTENTS="$(cat $ARCHIVE_CONTENTS_PATH)"
fi

# TODO: could add OS_RELEASE in if portability is a problem
# TODO: hmm how do we know the make skia script.. i guess its an arg? a back arg?
if [[ $OSTYPE == 'darwin'* ]]; then
    # md5 -r == md5sum
    CONFIGURE_VERSION=$(md5 -r cache_helper.sh | awk '{print $1}')
    MAKE_SKIA_HASH=$(md5 -r $SKIA_DEPENDENCIES_DIR/$MAKE_SKIA_FILE | awk '{print $1}')
    BUILD_HASH=$(md5 -r -s "$SKIA_COMMIT_HASH $MAKE_SKIA_HASH $CONFIGURE_VERSION" | awk '{print $1}')
else
    CONFIGURE_VERSION=$(md5sum cache_helper.sh | awk '{print $1}')
    MAKE_SKIA_HASH=$(md5sum $SKIA_DEPENDENCIES_DIR/$MAKE_SKIA_FILE | awk '{print $1}')
    BUILD_HASH=$(echo "$SKIA_COMMIT_HASH $MAKE_SKIA_HASH $CONFIGURE_VERSION" | md5sum | awk '{print $1}')
fi

echo "Created hash: $BUILD_HASH from skia_commit=$SKIA_COMMIT_HASH make_skia_script=$MAKE_SKIA_HASH configure_script=$CONFIGURE_VERSION"

EXPECTED_ARCHIVE_CONTENTS="$BUILD_HASH"_"$COMPILE_TARGET"

ARCHIVE_FILE_NAME="$CACHE_NAME"_"$EXPECTED_ARCHIVE_CONTENTS.tar.gz"
ARCHIVE_URL="https://cdn.rive.app/archives/$ARCHIVE_FILE_NAME"
ARCHIVE_PATH="$SKIA_DIR/$ARCHIVE_FILE_NAME"

pull_cache() {
    echo "Grabbing cached build from $ARCHIVE_URL"
    mkdir -p $SKIA_DIR
    curl --output $SKIA_DIR/$ARCHIVE_FILE_NAME $ARCHIVE_URL
    pushd $SKIA_DIR
    tar -xf $ARCHIVE_FILE_NAME out include $ARCHIVE_CONTENTS_NAME third_party modules
}

is_build_cached_remotely() {
    echo "Checking for cache build $ARCHIVE_URL"
    if curl --output /dev/null --head --silent --fail $ARCHIVE_URL; then
        return 0
    else
        return 1
    fi
}

upload_cache() {
    pushd $SKIA_DEPENDENCIES_DIR
    echo $EXPECTED_ARCHIVE_CONTENTS >$SKIA_DIR_NAME/$ARCHIVE_CONTENTS_NAME
    # not really sure about this third party biz
    # also we are caching on a per architecture path here, but out could contain more :thinking:
    tar -C $SKIA_DIR_NAME -cf $SKIA_DIR_NAME/$ARCHIVE_FILE_NAME $OUTPUT_CACHE $ARCHIVE_CONTENTS_NAME include third_party/libpng third_party/externals/libpng modules
    popd
    # # if we're configured to upload the archive back into our cache, lets do it!
    echo "Uploading to s3://2d-public/archives/$ARCHIVE_FILE_NAME"
    ls $ARCHIVE_PATH
    aws s3 cp $ARCHIVE_PATH s3://2d-public/archives/$ARCHIVE_FILE_NAME
}

is_build_cached_locally() {
    if [ "$EXPECTED_ARCHIVE_CONTENTS" == "$ARCHIVE_CONTENTS" ]; then
        return 0
    else
        return 1
    fi
}
