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
VERSION = "1.20"

#
# To use the md5sum program for calculating signatures (instead of the
# built in MD5 message digest routines) uncomment the following
# line (try this if you're having trouble with built in code).
#
#EXTERNAL_MD5 = -DEXTERNAL_MD5=\"md5sum\"



#####################################################################
# no need to modify anything beyond this point                      #
#####################################################################

fdupes: fdupes.c md5/md5.c	
	gcc fdupes.c md5/md5.c -o fdupes -DVERSION=\"$(VERSION)\" $(EXTERNAL_MD5)

install: fdupes
	cp fdupes $(INSTALLDIR)
	cp fdupes.1 $(MANPAGEDIR)/man1

tarball: clean
	tar --directory=.. -c -z -v -f ../fdupes-$(VERSION).tar.gz fdupes

clean:
	rm -f *.o
	rm -f fdupes
	rm -f *~

love:
	@echo You\'re not my type. Go find a human partner.
