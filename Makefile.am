bin_PROGRAMS = fdupes

if NO_NCURSES
fdupes_SOURCES = fdupes.c errormsg.c dir.c log.c fmatch.c sigint.c flags.c mbstowcs_escape_invalid.c positive_wcwidth.c md5/md5.c fdupes.h errormsg.h dir.h log.h fmatch.h sigint.h flags.h mbstowcs_escape_invalid.h positive_wcwidth.h md5/md5.h
notrans_dist_man1_MANS = fdupes.1

else
fdupes_SOURCES = fdupes.c ncurses-commands.c ncurses-getcommand.c ncurses-interface.c ncurses-print.c ncurses-prompt.c ncurses-status.c commandidentifier.c errormsg.c wcs.c dir.c log.c fmatch.c sigint.c flags.c mbstowcs_escape_invalid.c positive_wcwidth.c md5/md5.c fdupes.h ncurses-commands.h ncurses-getcommand.h ncurses-interface.h ncurses-print.h ncurses-prompt.h ncurses-status.h commandidentifier.h errormsg.h wcs.h filegroup.h dir.h log.h fmatch.h sigint.h flags.h mbstowcs_escape_invalid.h positive_wcwidth.h md5/md5.h
dist_man1_MANS = fdupes.1 fdupes-help.1

endif

EXTRA_DIST = testdir CHANGES CONTRIBUTORS

dist-hook:
	if [ -f $(top_srcdir)/INSTALL.enduser ]; then chmod u+w $(distdir)/INSTALL; \cp -f $(top_srcdir)/INSTALL.enduser $(distdir)/INSTALL; fi
