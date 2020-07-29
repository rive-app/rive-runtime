#!/bin/bash
rm -fR .clone_defs 2> /dev/null
mkdir .clone_defs
cd .clone_defs
git init
git remote add origin -f git@github.com:rive-app/rive.git
git config core.sparseCheckout true
echo '/dev/defs/*' > .git/info/sparse-checkout
git pull origin master

rm -fR ../defs
mv dev/defs ../
cd ..
rm -fR .clone_defs