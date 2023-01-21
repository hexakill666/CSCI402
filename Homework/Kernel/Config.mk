# This file is meant to change some aspect of how weenix is built or run.

# Variables in this file should meet the following criteria:
# * They change some behavior in the building or running of weenix that someone
#   using weenix for educational purposes could reasonably want to change on a regular
#   basis. Note that variables like CFLAGS are not defined here because they should
#   generally not be changed.

#
# Setting any of these variables will control which parts of the source tree
# are built. To enable something set it to 1, otherwise set it to 0.
#
     DRIVERS=1
         VFS=1
        S5FS=1
          VM=1
     DYNAMIC=1
# When you finish S5FS, first enable "VM"; once this is working, then enable
# "DYNAMIC".

# Set whether or not to loop in kmain waiting for a gdb attach.  This is a hack
# to get around the qemu/gdb bug (https://bugs.launchpad.net/qemu/+bug/526653).
#
# By default, GDBWAIT=0.  But if you want to debug weenix with gdb and the
# "./weenix -n -d gdb" command is not working right for you, see the instruction
# about setting GDBWAIT=1 here.
#
# If GDBWAIT=1 is used, kmain in kernel/main/kmain.c loops.  The commands in
# init.gdb free qemu from that loop and the breakpoint at bootstrap is hit.
#
# Note that if GDBWAIT=1, weenix MUST BE invoked with the "./weenix -n -d gdb -w"
# command!  If you invoke weenix with simply "./weenix -n", weenix will hang.
# Therefore, to run without gdb, you must have GDBWAIT=0.
#
# If GDBWAIT=0 and you invoke weenix with gdb (i.e., with "./weenix -n -d gdb"),
# you should still be able to debug your kernel, but you will not be able to see
# all the printout from dbg() statements.
#
# If you change this value, you must do "make clean" and "make".
        GDBWAIT=0

# Set which CS402 tests are to run.  Valid from 0 (no tests except running init)
# to 10 (run all tests plus student tests).
        CS402TESTS=10

# Currently, CS402INITCHOICE is not used in the kernel.  You can use it for
# anything you want (mainly to select which way to go in initproc_run()).
        CS402INITCHOICE=0

# Debug message behavior.
#
# To disable a dbg mode add ',-name' to this variable. To enable one add
# ',name'. For example to have everything except 'mm' and 'pagealloc' you would
# set DBG to 'all,-mm,-pagealloc'. To have only 'test', 'testpass', 'testfail'
# you would set DBG to '-all,test,testpass,testfail'.
#
# We generally recommend that you leave this set to 'all' with some of the
# less useful message types disabled. To see all avaible message types, and to
# potentially add to them see 'kernel/include/util/debug.h'
#
# Note that due to the way this is interpreted either 'all' or '-all' should
# always be the first thing in this variable. Note that this setting can be
# changed at runtime by modifying the dbg_modes global variable.
#
# All debug statements
        DBG=error,test
# Change to this for no debug statements
#       DBG=-all
# Change to this to select all the "General" and "VFS" stuff in "kernel/include/util/debug.h"
#       DBG = error,temp,print,test,testpass,testfail,vfs,fref,vnref
# Grading: (the grader will use the next line for grading "SELF-checks" and section (A) of grading guidelines)
#       DBG = error,print,test
# Grading: (the grader will use the next line when grading in general, i.e., will not see things printed with DBG_PRINT)
#       DBG = error,test
# Other examples:
#       DBG = error,temp,test
#       DBG = error,temp,print,test
#       DBG = error,temp,test,vnref,fref,vfs

# Switches for non-required components. If you wish to try implementing
# some extra features in Weenix, there are some pre-designed features
# you can add. Turn on one of these flags and re-compile Weenix. Please
# see the Wiki for details on what is provided by changing these flags
# and what you will need to implement to complete them, of course you
# are always free to implement your own features as well. Remember, though
# these features are not "extra-credit" they are purely for academic
# interest. The most important thing is that you have a working core
# implementation, and that is what you will be graded on. If you decide
# to implement extra features please make sure your core Weenix is working
# first, and make sure to make a copy of your working Weenix before you
# go breaking it, which we promise you will happen.

         SHADOWD=0 # shadow page cleanup
        MOUNTING=0 # be able to mount multiple file systems
          GETCWD=0 # getcwd(3) syscall-like functionality
        UPREEMPT=0 # userland preemption
             MTP=0 # multiple kernel threads per process
           PIPES=0 # pipe(2) functionality

# Set the number of terminals that we should be launching.
        NTERMS=3

# Set the number of disks that we should be launching with
        NDISKS=1

# terminal binary to use when opening a second terminal for gdb
        GDB_TERM=xterm
        GDB_PORT=1234

# The amount of physical memory which will be available to Weenix (in megabytes)
# XXX MEMORY=256 is hardcoded in ./weenix right now -- this line here is
#     currently ignored
        MEMORY=256

# Parameters for the hard disk we build (must be compatible!)
# If the FS is too big for the disk, BAD things happen!
        DISK_BLOCKS=2048 # For fsmaker
        DISK_INODES=240  # For fsmaker

# Boolean options specified in this specified in this file that should be
# included as definitions at compile time
        COMPILE_CONFIG_BOOLS=" DRIVERS VFS S5FS VM FI DYNAMIC MOUNTING MTP SHADOWD GETCWD UPREEMPT PIPES "
# As above, but not booleans
        COMPILE_CONFIG_DEFS=" NTERMS NDISKS DBG DISK_SIZE "
