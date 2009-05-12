#!/bin/bash

usage ()
{
  echo "You should give the program you want to trace as parameter."
  echo "As example, to trace the command 'xclock -digital' run:"
  echo "cb_trace_program.sh xclock -digital"
  exit 0
}

checkerr ()
{
#idea: http://www.faqs.org/docs/Linux-HOWTO/Bash-Prog-Intro-HOWTO.html
#otherwise $? will be overwritten after the if
RETCODE=$?
if [ $RETCODE -ne 0 ] ; then
echo "cb_trace_program.sh: The previous command returned with status $RETCODE, indicating an error, exiting"
exit 1
fi
}

if [ $# -lt 1 ] || [ "$1" == "--help" ] || [ "$1" == "-help" ] || [ "$1" == "-h" ] ; then
usage
fi

ORIGDIR=`pwd`

#needed to make the cb_automatic.sh script usable
if [ "$COPY_TRACETMPDIR" == "" ] ; then
COPY_TRACETMPDIR='.'
fi

echo "$ORIGDIR" > "$COPY_TRACETMPDIR"/path_temp

echo "$@" > "$COPY_TRACETMPDIR"/command_temp

SYSCALLS="open,access,execve,clone,fork,vfork,chdir,stat64,lstat64,readlink,getcwd,connect"

#note: Unfortunately the return value of strace is not documented in the man-page.

strace -q -f -o "$COPY_TRACETMPDIR"/strace_bashrc_temp -e trace=$SYSCALLS bash ~/.bashrc

checkerr

strace -q -f -o "$COPY_TRACETMPDIR"/strace_temp -e trace=$SYSCALLS "$@"

checkerr

cd "$COPY_TRACETMPDIR"

cb_strace_to_xml -q -sstrace_bashrc_temp -orecord_bashrc.xml

checkerr

cb_strace_to_xml

checkerr

cb_meta_check -update -ccommand_temp

checkerr

rm -f strace_bashrc_temp
#rm -f strace_temp
#rm -f path_temp
rm -f strace_prepro_temp
rm -f command_temp

cd "$ORIGDIR"
