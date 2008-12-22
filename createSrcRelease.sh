#/bin/bash

echo "CreateSrcRelease.sh: This script will generate a proper named .tar.bz2 file with all the sources. The file can be found in the ../temp directory"

# get version information

VERSTR=`cat share/tools.h | grep PGNVERSION | cut -d\  -f3`
VERSTR=${VERSTR//./-} #"} #replace . by -
VERSTR=${VERSTR//\"/_} #"} #replace " by _

echo "CreateSrcRelease.sh: Release version suffix is: $VERSTR "

CDIR=`pwd`

RNAME=copybirds"$VERSTR"src

./createdir.sh ../temp

mkdir "../temp/$RNAME"

cp -r . "../temp/$RNAME/"

cd "../temp/$RNAME"

CNEWDIR=`pwd`

if [ "$CDIR" == "$CNEWDIR" ] ; then

echo "CreateSrcRelease.sh: Error source and temporary directory are the same, exiting"

exit 1

fi

make clean > /dev/null

make removebackups > /dev/null

rm createSrcRelease.sh

cd ..

tar -cf "$RNAME".tar --exclude=*svn* "$RNAME"

bzip2 -z "$RNAME".tar

chmod u+w "$RNAME"/* -R

chmod u+w "$RNAME"/.svn -R

rm -r $RNAME

echo "CreateSrcRelease.sh: Done."
