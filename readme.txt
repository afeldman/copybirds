------------ This version is from the SVN trunk, please use an official release instead-----

(c) 2008 by Malte Marwedel m.marwedel AT onlinehome DOT de
COPYBIRDS, Copy binaries runtime data surrounding
http://copybirds.sourceforge.net
Terms of Use: GPL Version 2 or later

============ Howto compile ===============================
On a fresh Kubuntu 7.04 Live-CD the following three packages and its dependencies have to be installed before compiling works:
libc6-dev, libxml2-dev, freeglut3-dev

run
  make
in this directory.
If you encounter problems, you might need to run
  ./configure
in the directory lib/c-algorithms-1.1.0

After compiling, make the content of bin available in your $PATH variable
This may work with:
  PATH=$PATH:PATHTOTHEBINDIRECTORY
  export PATH

============ Quickstart ==================================
Go into a writeable directory. Here you may run (no root rights needed)
  cb_automatic.sh <your program to capture with parameters>
And follow the instructions.

Then, on the target system, you may run: (You will be asked about your sudo password, which means you have to be in the sudoers file.)
  ./WHATEVERYOUNAMEIT.sh

You need more help?
Every program and script supports
  -h
as parameter to print instructions. (And -help, --help too if you prefer this)

=========== What are all those different programs for? ===
cb_analyze_dep: .deb package dependency analysis
cb_files: Copies files together in one directory
cb_automatic.sh: Interactive script, calls cb_trace_program.sh and cb_make_selfextracting.sh
cb_make_selfextracting.sh: Generates the self extracting archive with all files, calls cb_files
cb_meta_check: Gathers and compares meta informations
cb_meta_merge: Unifies meta information from multiple xml files into one file
cb_remove_from_source: Deletes files on the target system if a self-extracting archive is unpacked (be very careful if you run this on productive systems)
cb_strace_to_xml: Converts the output of strace to a suitable xml representation
cb_trace_program.sh: Calls strace, cb_strace_to_xml and cb_meta_check with proper parameters

=========== Troubleshooting on special machines ==========
The programs use escape sequences to generate color output in the console.
If you get problems with them, go to the file share/tools.h and define the preprocessor variable NOCOLOR_H. After doing so, please re-compile.
http://linuxgazette.net/issue65/padala.html

=========== Other things =================================
If you found a bug (especially crashes), please report.
You can trace crashes down by running the individual program with the gdb debugger:
1. Start the debugger:
  gdb cb_THE_PROGRAM_WHICH_CRASHED
2. Start the program within the debugger
  run THE_PARAMETERS_YOU_GAVE_THE_PROGRAM_AT_THE_CRASH
3. Wait for the crash and look where it happened:
  backtrace full
then mail me the whole output of gdb or report as a bug on sourceforge.
4. Exit the debugger
  q

Have fun.