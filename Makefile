#
# INSTALLDIR indicates directory where program is to be installed. 
# Suggested values are "/usr/local/bin" or "/usr/bin".
#
INSTALLDIR = /usr/local/bin

#
# MANPAGEDIR indicates directory where the fdupes man page is to be 
# installed. Suggested values are "/usr/local/man" or "/usr/man".
#
MANPAGEDIR = /usr/local/man

#
# VERSION determines the program's version number.
#
VERSION = "1.40"

#
# To use the md5sum program for calculating signatures (instead of the
# built in MD5 message digest routines) uncomment the following
# line (try this if you're having trouble with built in code).
#
#EXTERNAL_MD5 = -DEXTERNAL_MD5=\"md5sum\"

#
# This version of fdupes can use a red-black tree structure to
# store file information. This is disabled by default, as it
# hasn't been optimized or verified correct. If you wish to
# enable this untested option, uncomment the following line.
#
#EXPERIMENTAL_RBTREE = -DEXPERIMENTAL_RBTREE

#####################################################################
# no need to modify anything beyond this point                      #
#####################################################################

fdupes: fdupes.c md5/md5.c	
	gcc fdupes.c md5/md5.c -Wall -o fdupes -DVERSION=\"$(VERSION)\" $(EXTERNAL_MD5) $(EXPERIMENTAL_RBTREE)

install: fdupes
	cp fdupes $(INSTALLDIR)
	cp fdupes.1 $(MANPAGEDIR)/man1

tarball: clean
	tar --directory=.. -c -z -v -f ../fdupes-$(VERSION).tar.gz fdupes-$(VERSION)

clean:
	rm -f *.o
	rm -f fdupes
	rm -f *~

love:
	@echo You\'re not my type. Go find a human partner.
