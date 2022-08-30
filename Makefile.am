bin_PROGRAMS = fdupes

fdupes_SOURCES = fdupes.c\
 fdupes.h\
 errormsg.c\
 errormsg.h\
 dir.c\
 dir.h\
 log.c\
 log.h\
 fmatch.c\
 fmatch.h\
 sigint.c\
 sigint.h\
 flags.c\
 flags.h\
 confirmmatch.c\
 confirmmatch.h\
 removeifnotchanged.c\
 removeifnotchanged.h\
 mbstowcs_escape_invalid.c\
 mbstowcs_escape_invalid.h\
 md5/md5.c\
 md5/md5.h
dist_man1_MANS = fdupes.1

if WITH_NCURSES
fdupes_SOURCES += filegroup.h\
 fileaction.h\
 fileaction.c\
 ncurses-commands.c\
 ncurses-commands.h\
 ncurses-getcommand.c\
 ncurses-getcommand.h\
 ncurses-interface.c\
 ncurses-interface.h\
 ncurses-print.c\
 ncurses-print.h\
 ncurses-prompt.c\
 ncurses-prompt.h\
 ncurses-status.c\
 ncurses-status.h\
 commandidentifier.c\
 commandidentifier.h\
 wcs.c\
 wcs.h\
 positive_wcwidth.c\
 positive_wcwidth.h
dist_man7_MANS = fdupes-help.7

endif

EXTRA_DIST = testdir CHANGES CONTRIBUTORS

dist-hook:
	if [ -f $(top_srcdir)/INSTALL.enduser ]; then chmod u+w $(distdir)/INSTALL; \cp -f $(top_srcdir)/INSTALL.enduser $(distdir)/INSTALL; fi
