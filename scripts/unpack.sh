#!/bin/bash

checkerr ()
{
#idea: http://www.faqs.org/docs/Linux-HOWTO/Bash-Prog-Intro-HOWTO.html
if [ $? -ne 0 ] ; then
echo "$ARCHIVENAME: The previous command returned with status 1, indicating an error, exiting"
exit 1
fi
}

ARCHIVENAME=`basename "$0"`
#manually change the two name in pack.sh too if changing here
METANAME='metainfomerged'
TEMPTARGET='./target12345'

echo "$ARCHIVENAME: Self Extracting Installer, try --help to see options"
echo "This genius idea came from: http://www.linuxjournal.com/node/1005818"
echo "__DESCRIPTION__";

#Or statement: http://www.issociate.de/board/post/300500/%22or%22_in_bash_if_statements.html
if [ "$1" != "" ] ; then

if [ "$1" == "--help" ] || [ "$1" == "-help" ] || [ "$1" == "-h" ]; then
echo "This will extract the archive and integrate it into your system."
echo "You may use ONE of the following options as FIRST parameter:"
echo "--show to display the depended packages."
echo "--installdepend to run the sudo apt-get install <packagenames>."
echo "--localtemp to do the extraction part in the directory where you are currently"
echo "  this may be useful to save RAM on a live-CD by using an USB stick."
echo "If no parameter is given, the application will be extracted"
echo "  and integrated creating the temporary directory in your home dir."
echo "The following parameters may be used as first or second parameter:"
echo "--keeptemp prevents the temporary directory from deletion."
echo "--extractonly only extracts the archive, without integration".
exit 0
elif [ "$1" == "--show" ] ; then
echo "$ARCHIVENAME: Depending packages are: '__DEPPACKAGES__'"
exit 0
elif [ "$1" == "--installdepend" ] ; then
echo "$ARCHIVENAME: I will install all given dependency packages now"
sudo apt-get install __DEPPACKAGES__
exit 0
elif [ "$1" != "--localtemp" ] && [ "$1" != "--keeptemp" ] ; then
#just check if the given parameter $1 contains some valid parameter
echo "$ARCHIVENAME: Error: Parameter '$1' is not known"
exit 1
fi

fi

if  [ "$2" != "" ] && [ "$2" != "--keeptemp" ] ; then
echo "$ARCHIVENAME: Error: Parameter '$2' is not known as second parameter"
exit 1
fi

#/tmp is too small on most current liveCDs

if [ "$1" == "--localtemp" ] ; then
  TMPDIR=`mktemp -d ./selfextract.XXXXXX`
else
  TMPDIR=`mktemp -d ~/selfextract.XXXXXX`
fi

ARCHIVE=`awk '/^__ARCHIVE_BELOW__/ {print NR + 1; exit 0; }' "$0"`

echo "$ARCHIVENAME: Extracting userdata from script..."

tail -n+$ARCHIVE "$0" > "$TMPDIR"/application.tar.gpg

CDIR=`pwd`

cd "$TMPDIR"

echo "$ARCHIVENAME: Decrypting with GPG..."

gpg application.tar.gpg

#testing if gpg returns non 0 results in false-positive aborts

#idea: http://www.linuxconfig.org/Bash_scripting_Tutorial
if [ ! -f application.tar ] ; then

echo "$ARCHIVENAME: Error: Decryption failed. Bad password?"
exit 1

fi


rm application.tar.gpg

echo "$ARCHIVENAME: Unpacking tar archive..."

tar xvf application.tar >/dev/null

checkerr

rm application.tar

if [ "$1" != "--extractonly" ] && [ "$2" != "--extractonly" ] ; then
echo "$ARCHIVENAME: Done."
exit 0

fi

#move tools outside of TEMPTARGET, to prevent copying into the system

mv "$TEMPTARGET"/tools ./tools

#tricky move old homedir to new homedir

export ORIGUSER=`./tools/cb_meta_check -q -printuserhome ./tools/"$METANAME".xml`

checkerr

if [ "$ORIGUSER" != "$HOME" ] ; then

#creating the whole path and deletes one level, then move highest level

mkdir -p "$TEMPTARGET"/"$HOME"

rmdir "$TEMPTARGET"/"$HOME"

mv "$TEMPTARGET"/"$ORIGUSER" "$TEMPTARGET"/"$HOME"

fi

if [ -f ./target12345/"$HOME"/.bashrc ] ; then


echo "$ARCHIVENAME: Should your .bashrc be overwritten with the archived one (the old one will be backuped)?"
echo "$ARCHIVENAME: This might be useful to set up some environment variables. N/y:"

read SELECTED
if [ "$SELECTED" == "y" ] || [ "$SELECTED" == "Y" ] || [ "$SELECTED" == "yes" ] || [ "$SELECTED" == "Yes" ] ; then

#explicitply copy the .bashrc
#howto use the date: http://www.cyberciti.biz/faq/linux-unix-formatting-dates-for-display/

mv ~/.bashrc ~/.bashrc-backup-`date +"%Y%m%d-%H%M%S"`

cp "$TEMPTARGET"/"$HOME"/.bashrc ~/.bashrc

echo "$ARCHIVENAME: You should source your new .bashrc after this script has finished."

BASHRCNEWNAME=""

else

BASHRCNEWNAME=`date +"%Y%m%d-%H%M%S"`
BASHRCNEWNAME=~/.bashrc-fromarchive-"$BASHRCNEWNAME"

cp "$TEMPTARGET"/"$HOME"/.bashrc "$BASHRCNEWNAME"

fi

fi

#remove files which exists on the target
./tools/cb_remove_from_source -delete "$PWD/$TEMPTARGET" /

checkerr

#now copy

echo "$ARCHIVENAME: You may enter your password for sudo to integrate the files into your system."
echo "$ARCHIVENAME: Note: Only enter if you really want this."

sudo rsync -aE --ignore-existing --keep-dirlinks "$TEMPTARGET"/ /

checkerr

echo "$ARCHIVENAME: Files integrated."

#check for compatibility
./tools/cb_meta_check ./tools/"$METANAME".xml
#print captured commands
./tools/cb_meta_check -printcommands ./tools/"$METANAME".xml


cd "$CDIR"

if [ "$1" != "--keeptemp" ] && [ "$2" != "--keeptemp" ] ; then
echo "$ARCHIVENAME: Removing temporary directory."
rm -rf "$TMPDIR"

else

echo "$ARCHIVENAME: The name of the temporary directory is '$TMPDIR'"

fi

if [ "$BASHRCNEWNAME" != "" ] ; then
  echo "$ARCHIVENAME: The .bashrc from the archive is named: '$BASHRCNEWNAME'"
fi

echo "$ARCHIVENAME: Done."

#The exit is VERY important in order to not execute the added binary data
exit 0

__ARCHIVE_BELOW__