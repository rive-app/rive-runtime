set -x

for f in `find ../../../zzzgold/ -name emptyclear.png`; do
    cp $1 `dirname $f`/
    git add `dirname $f`/`basename $1`
done
