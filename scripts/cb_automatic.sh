#!/bin/bash

#(c) 2008 by Malte Marwedel. Terms of use: GPL version 2
#ideas based on: http://www.kingcomputerservices.com/unix_101/tips_on_good_shell_programming_practices.htm

usage () {
  echo "You should give the program you want to copy as parameter"
  echo "as example, to copy the command 'xclock -digital' run:"
  echo "cb_autmatic.sh xclock -digital"
  exit 0
}


checkerr () {
  #idea: http://www.faqs.org/docs/Linux-HOWTO/Bash-Prog-Intro-HOWTO.html
  #otherwise $? will be overwritten after the if
  RETCODE=$?
  if [ $RETCODE -ne 0 ] ; then
  echo "cb_automatic.sh: The previous command returned with status $RETCODE, indicating an error, exiting"
  exit 1
  fi
}


echo "cb_automatic.sh: This is the interactive meta script for archiving an application"


if [ $# -lt 1 ] || [ "$1" == "--help" ] || [ "$1" == "-help" ] || [ "$1" == "-h" ] ; then
usage
fi


#determine a usable editor
if [ "$EDITOR" == "" ] ; then

#if not set
echo "cb_automatic.sh: Note: Set you EDITOR environment variable to get the text editor you prefer."

if [ `which kate` != "" ] ; then
EDITOR='kate'
elif [ `which gedit` != "" ] ; then
EDITOR='gedit'
elif [ `which kwrite` != "" ] ; then
EDITOR='kwrite'
elif [ `which emacs` != "" ] ; then
EDITOR='emacs'
elif [ `which nano` != "" ] ; then
EDITOR='nano'
else
echo "cb_automatic.sh: Error: Could not find a useble text editor, exiting"
exit 1
fi

fi

#make a temporary directory
ORIGDIR=`pwd`

echo "cb_automatic.sh: Where do you want to store a directory for the captured archive?"
echo "cb_automatic.sh: h) Within your home directory"
echo "cb_automatic.sh: t) Within /temp (please note that other user may read here)"
echo "cb_automatic.sh: otherwise the current directory is used (recommend)"
echo "cb_automatic.sh: Please choose and press enter:"

read CHOICE

if [ "$CHOICE" == "h" ] ; then

export COPY_TRACETMPDIR=`mktemp -d ~/tracetemp.XXXXXX`

elif [ "$CHOICE" == "t" ] ; then

export COPY_TRACETMPDIR=`mktemp -d /tmp/tracetemp.XXXXXX`

else

export COPY_TRACETMPDIR=`mktemp -d tracetemp.XXXXXX`

fi

echo "cb_automatic.sh: You will find all files within '$COPY_TRACETMPDIR'"

#now trace the command

cb_trace_program.sh "$@"
checkerr

cd "$COPY_TRACETMPDIR"

#ask the user if the used packages should be filtered
#based on: http://www.linuxconfig.org/Bash_scripting_Tutorial

echo "cb_automatic.sh: Do you want put all files which are provided by Debian"
echo "cb_automatic.sh: packages into a blacklist and add the name of the"
echo "cb_automatic.sh: packages, they are found in, as dependencies? y/N:"

read  SELECTED
if [ "$SELECTED" == "y" ] || [ "$SELECTED" == "Y" ] || [ "$SELECTED" == "yes" ] || [ "$SELECTED" == "Yes" ] ; then

cb_analyze_dep *.xml
checkerr

else

echo "cb_automatic.sh: No dependency analyzing wished"

fi

#ask the user if he wants to edit additional blacklist and wildcards
echo "cb_automatic.sh: Do you want to edit some blacklist or wildcards manually? y/N:"

read SELECTED
if [ "$SELECTED" == "y" ] || [ "$SELECTED" == "Y" ] || [ "$SELECTED" == "yes" ] || [ "$SELECTED" == "Yes" ] ; then

echo "__EMPTYDEMO__" > userfiles.xml

$EDITOR userfiles.xml

else

echo "cb_automatic.sh: No manual edit wished"

fi

#ask about a description

echo "cb_automatic.sh: It is recommend to enter a short description about the application"
echo "cb_automatic.sh: you are just archiving. This will be displayed on extraction already"
echo "cb_automatic.sh: before you entered the decryption password."
echo "cb_automatic.sh: It is the best to limit your text to printable ASCII characters."
echo "cb_automatic.sh: Umlauts may work only if you use utf-8 encoding."
echo "cb_automatic.sh: Press enter to launch the editor. After writing your description, safe the file and close the editor."

read

$EDITOR description_temp.txt

if [ -f description_temp.txt ] ; then

#the command replace is part of the mysql server...
#USERDESCRIPTION=$(cat description_temp.txt | replace "\"" "&quot;" "&" "&amp;" "<" "&lt;" ">" "&gt;")

USERDESCRIPTION=$(cat description_temp.txt)
USERDESCRIPTION=${USERDESCRIPTION//\"/"&quot;"} #"}
USERDESCRIPTION=${USERDESCRIPTION/"&"/"&amp;"}
USERDESCRIPTION=${USERDESCRIPTION/"<"/"&lt;"}
USERDESCRIPTION=${USERDESCRIPTION/">"/"&gt;"}

echo "<?xml version=\"1.0\"?><content><metainfo>\n<usercomment>$USERDESCRIPTION</usercomment>\n</metainfo></content>" > description.xml

rm description_temp.txt

fi

#ask about an archive name

BAPPNAME='application_'`basename $1`

echo "cb_automatic.sh: Please enter the file name of your archive."
echo "cb_automatic.sh: The '.sh' extension will be added automatically."
echo "cb_automatic.sh: If you just press enter, the name '$BAPPNAME.sh' will be used"

read OUTPUTNAME

if [ "$OUTPUTNAME" == "" ] ; then
OUTPUTNAME="$BAPPNAME"
fi

export OUTPUTNAME

#pack the whole thing
cb_make_selfextracting.sh *.xml

# go back to the original one

cd "$ORIGDIR"
