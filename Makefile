#
# fdupes Makefile
#

#####################################################################
# Standand User Configuration Section                               #
#####################################################################

#
# PREFIX indicates the base directory used as the basis for the 
# determination of the actual installation directories.
# Suggested values are "/usr/local", "/usr", "/pkgs/fdupes-$(VERSION)"
#
PREFIX = /usr/local

#
# When compiling for 32-bit systems, FILEOFFSET_64BIT must be enabled
# for fdupes to handle files greater than (2<<31)-1 bytes.
#
FILEOFFSET_64BIT = -D_FILE_OFFSET_BITS=64

#
# Certain platforms do not support long options (command line options).
# To disable long options, uncomment the following line.
#
#OMIT_GETOPT_LONG = -DOMIT_GETOPT_LONG

#
# To use the md5sum program for calculating signatures (instead of the
# built in MD5 message digest routines) uncomment the following
# line (try this if you're having trouble with built in code).
#
#EXTERNAL_MD5 = -DEXTERNAL_MD5=\"md5sum\"

#
# Uncomment the following line on systems lacking ncurses support, or
# for installations where the screen-mode interface is not desired.
#
#NO_NCURSES = -DNO_NCURSES

#####################################################################
# Developer Configuration Section                                   #
#####################################################################

#
# VERSION determines the program's version number.
#
include Makefile.inc/VERSION

#
# PROGRAM_NAME determines the installation name and manual page name
#
PROGRAM_NAME=fdupes

#
# BIN_DIR indicates directory where program is to be installed. 
# Suggested value is "$(PREFIX)/bin"
#
BIN_DIR = $(PREFIX)/bin

#
# MAN_DIR indicates directory where the fdupes man page is to be 
# installed. Suggested value is "$(PREFIX)/man/man1"
#
MAN_BASE_DIR = $(PREFIX)/man
MAN_DIR = $(MAN_BASE_DIR)/man1
MAN_EXT = 1

#
# Required External Tools
#

INSTALL = install	# install : UCB/GNU Install compatiable
#INSTALL = ginstall

RM      = rm -f

MKDIR   = mkdir -p
#MKDIR   = mkdirhier 
#MKDIR   = mkinstalldirs

#
# Use 8-bit code units for regex patterns and data.
#
PCRE2_CODE_UNIT_WIDTH = -DPCRE2_CODE_UNIT_WIDTH=8

#
#
#
HELP_COMMAND_STRING = -DHELP_COMMAND_STRING="\"man 1 fdupes-help\""

#
# Make Configuration
#
CC = gcc
COMPILER_OPTIONS = -Wall -O -g

CFLAGS= $(COMPILER_OPTIONS) -I. -DPROGRAM_NAME=\"$(PROGRAM_NAME)\" -DVERSION=\"$(VERSION)\" $(EXTERNAL_MD5) $(OMIT_GETOPT_LONG) $(NO_NCURSES) $(FILEOFFSET_64BIT) $(PCRE2_CODE_UNIT_WIDTH) $(HELP_COMMAND_STRING)

DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td
DEPDIR := .d
$(shell mkdir -p $(DEPDIR) >/dev/null)

INSTALL_PROGRAM = $(INSTALL) -c -m 0755
INSTALL_DATA    = $(INSTALL) -c -m 0644

POSTCOMPILE = @mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@

#
# EXTERNAL LIBRARIES
#
ifndef NO_NCURSES
EXTERNAL_LIBRARIES = -lncursesw -lpcre2-8
endif

#
# ADDITIONAL_OBJECTS - some platforms will need additional object files
# to support features not supplied by their vendor. Eg: GNU getopt()
#
#ADDITIONAL_OBJECTS = getopt.o

ifndef NO_NCURSES
NCURSES_OBJECT_FILES = ncurses-interface.o\
	ncurses-commands.o\
	ncurses-getcommand.o\
	ncurses-prompt.o\
	ncurses-status.o\
	ncurses-print.o\
	commandidentifier.o\
	wcs.o
endif

OBJECT_FILES = fdupes.o\
	errormsg.o\
	md5/md5.o\
	$(NCURSES_OBJECT_FILES)\
	$(ADDITIONAL_OBJECTS)

ifndef NO_NCURSES
NCURSES_SRCS = ncurses-interface.c\
	ncurses-commands.c\
	ncurses-getcommand.c\
	ncurses-prompt.c\
	ncurses-status.c\
	ncurses-print.c\
	commandidentifier.c\
	wcs.c
endif

SRCS = fdupes.c\
	errormsg.c\
	md5/md5.c\
	$(NCURSES_SRCS)

#####################################################################
# no need to modify anything beyond this point                      #
#####################################################################

all: fdupes

%.o : %.c
%.o : %.c $(DEPDIR)/%.d
	$(CC) -c $(CFLAGS) $(DEPFLAGS) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

$(DEPDIR)/%.d: ;
.PRECIOUS: $(DEPDIR)/%.d

md5/md5.o: md5/md5.c md5/md5.h
	$(CC) -c $(CFLAGS) $(OUTPUT_OPTION) $<

fdupes: $(OBJECT_FILES)
	$(CC) $(CFLAGS) -o fdupes $(OBJECT_FILES) $(EXTERNAL_LIBRARIES)

installdirs:
	test -d $(BIN_DIR) || $(MKDIR) $(BIN_DIR)
	test -d $(MAN_DIR) || $(MKDIR) $(MAN_DIR)

install: fdupes installdirs
	$(INSTALL_PROGRAM)	fdupes   $(BIN_DIR)/$(PROGRAM_NAME)
	$(INSTALL_DATA)		fdupes.1 $(MAN_DIR)/$(PROGRAM_NAME).$(MAN_EXT)
ifndef NO_NCURSES
	$(INSTALL_DATA)		fdupes-help.1 $(MAN_DIR)/fdupes-help.$(MAN_EXT)
endif

.PHONY: clean
clean:
	$(RM) $(OBJECT_FILES)
	$(RM) fdupes
	$(RM) *~ md5/*~

distclean: clean
	rm -f .d/*.d .d/*.Td
	rmdir .d

love:
	@echo You\'re not my type. Go find a human partner.

include $(wildcard $(patsubst %,$(DEPDIR)/%.d,$(basename $(SRCS))))