INSTALLDIR = /usr/local/bin
MANPAGEDIR = /usr/man
VERSION = 1.11

fdupes: fdupes.c
	gcc fdupes.c -o fdupes -DVERSION=\"$(VERSION)\"

test: fdupes
	./runtest

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
