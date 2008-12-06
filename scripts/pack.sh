#!/bin/bash

#--------makes a self extracting script with a PGP encrypted tar archive--------
#(c) 2008 by Malte Marwedel. Terms of Use: GPL version 2

checkerr () {
#idea: http://www.faqs.org/docs/Linux-HOWTO/Bash-Prog-Intro-HOWTO.html
#otherwise $? will be overwritten after the if
RETCODE=$?
if [ $RETCODE -ne 0 ] ; then
echo "cb_automatic.sh: The previous command returned with status $RETCODE, indicating an error, exiting"
exit 1
fi
}

#how to write functions: http://tldp.org/LDP/abs/html/complexfunct.html
encapsulate () {
  RETVAL=${1//\"/\\\"} #"} #uses the first argument as input
  RETVAL=${RETVAL//\`/\\\`}
  RETVAL=${RETVAL//\$/\\\$}
}

#set up some parameters

if [ "$OUTPUTNAME" == "" ] ; then
OUTPUTNAME='application'
fi

#manually change both names in unpack.sh too if changing here
METANAME='metainfomerged'
TEMPTARGET='./target12345'


if [ $# -lt 1 ] || [ "$1" == "--help" ] || [ "$1" == "-help" ] || [ "$1" == "-h" ] ; then
echo "This script will create an encrypted self extracting archive."
echo "The script is based on http://www.linuxjournal.com/node/1005818"
echo "All xml files which should be used, must be given as parameter."
echo "Normally '*.xml' is the parameter you want to use."
exit 0
fi

#copy all files together

cb_files -xdevices.xml "$@" "$TEMPTARGET"

checkerr

#merge metadata and copy to target
cb_meta_merge -o"$METANAME".xml *.xml

checkerr

mkdir "$TEMPTARGET"/tools
cp "$METANAME".xml "$TEMPTARGET"/tools/"$METANAME".xml

#some ideas based on: http://www.ibm.com/developerworks/library/l-bash.html

#DEPPACKAGES=$(cb_meta_check -q -printpackages "$METANAME".xml | replace \" \\\" \\ \\\\ )
DEPPACKAGES=$(cb_meta_check -q -printpackages "$METANAME".xml)
encapsulate "$DEPPACKAGES"
DEPPACKAGES="$RETVAL"


#DESCRIPTION=$(cb_meta_check -q -printcomment "$METANAME".xml | replace \" \\\" \\ \\\\ \` \\\` )
DESCRIPTION=$(cb_meta_check -q -printcomment "$METANAME".xml)
encapsulate "$DESCRIPTION"
DESCRIPTION="$RETVAL"


#add copy and check program

cp `which cb_remove_from_source` "$TEMPTARGET"/tools/cb_remove_from_source
checkerr
cp `which cb_meta_check` "$TEMPTARGET"/tools/cb_meta_check
checkerr

#based on: http://nl.ijs.si/gnusl/tex/tunix/tips/node130.html
echo "cb_make_selfextracting.sh: Creating tar archive..."

tar  -cvf "$OUTPUTNAME".tar "$TEMPTARGET" >/dev/null

checkerr

rm -rf "$TEMPTARGET"

#encrypt with PGP
#based on: http://www.cyberciti.biz/tips/linux-how-to-encrypt-and-decrypt-files-with-a-password.html
echo "cb_make_selfextracting.sh: Encrypting with GPG..."

gpg -c "$OUTPUTNAME".tar

#checkerr might result in false positives on gpg
if [ ! -f "$OUTPUTNAME.tar.gpg" ] ; then

echo "cb_make_selfextracting.sh: Error: Encryption failed. Different passwords?"
exit 1

fi

rm "$OUTPUTNAME".tar

#make selfextracting script
#based on: http://www.linuxjournal.com/node/1005818
#and: http://info.eps.surrey.ac.uk/FAQ/unpack.html

echo "cb_make_selfextracting.sh: Creating self extracting archive"

#echo ... | replace __DEPPACKAGES__ "$DEPPACKAGES" __DESCRIPTION__ "$DESCRIPTION" > decompress12345.sh
UNPACK="__MAGICMARK__"
UNPACK=${UNPACK//__DEPPACKAGES__/"$DEPPACKAGES"}
UNPACK=${UNPACK/__DESCRIPTION__/"$DESCRIPTION"}
echo "$UNPACK" > decompress12345.sh

cat decompress12345.sh "$OUTPUTNAME".tar.gpg > "$OUTPUTNAME".sh

chmod u+x "$OUTPUTNAME".sh

#clean up

rm decompress12345.sh

rm "$OUTPUTNAME".tar.gpg

echo "cb_make_selfextracting.sh: Done. '$OUTPUTNAME.sh' is ready to use."
