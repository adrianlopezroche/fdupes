INSTALLDIR = /usr/local/bin
VERSION = 1.0

fdupes: fdupes.c
	gcc fdupes.c -o fdupes -DVERSION=\"$(VERSION)\"

test: fdupes
	./runtest

install: fdupes
	cp fdupes $(INSTALLDIR)

tarball: clean
	tar --directory=.. -c -z -v -f ../fdupes-$(VERSION).tar.gz fdupes

clean:
	rm -f *.o
	rm -f fdupes
	rm -f *~

