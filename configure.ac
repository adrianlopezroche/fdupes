AC_INIT([fdupes], [2.3.2])

AM_INIT_AUTOMAKE([foreign subdir-objects])

AC_CONFIG_HEADERS([config.h])

AC_ARG_PROGRAM

PKG_PROG_PKG_CONFIG

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
	#define _XOPEN_SOURCE 700
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <unistd.h>
]], [[
	struct stat st;
	st.st_ctim.tv_nsec = 0;
	st.st_mtim.tv_nsec = 0;
]])],
	[AC_DEFINE([HAVE_NSEC_TIMES], [1], [stat supports nanosecond precision])]
	[AC_DEFINE([_XOPEN_SOURCE], [700], [enable certain X/Open and POSIX features])]
)

#
# NCURSES library
#
AC_ARG_WITH([ncurses], AS_HELP_STRING([--without-ncurses], [Do not use ncurses interface]))

AC_CHECK_HEADERS([getopt.h ncursesw/curses.h])
AS_IF([test x"$with_ncurses" != x"no"],
	[PKG_CHECK_MODULES([NCURSES], [ncursesw],
		[LIBS="$LIBS $NCURSES_LIBS"],
		[AC_SEARCH_LIBS([wget_wch], [ncursesw ncurses curses], [], [AC_ERROR([ncurses library not found (or lacks wide character support)])])]
		[AC_SEARCH_LIBS([keypad], [ncursesw tinfow ncurses tinfo curses], [], [AC_ERROR([ncurses library not found (lacks keypad support)])])]
	)]
	[AC_DEFINE([_XOPEN_SOURCE], [700], [enable certain X/Open and POSIX features])]
	[AC_DEFINE([_XOPEN_SOURCE_EXTENDED], [], [enable certain functions in curses.h])]
	[AC_DEFINE([_ISOC99_SOURCE], [], [enable strtoll])]
	[AC_SEARCH_LIBS([pcre2_match_32], [pcre2-32], [], [AC_ERROR([pcre2 library not found])])]
	[AC_DEFINE([PCRE2_CODE_UNIT_WIDTH], [32], [PCRE2 Code Unit Width])],

	[AC_DEFINE([NO_NCURSES], [], [Do not compile against ncurses])]
	)

AM_CONDITIONAL([WITH_NCURSES], [test x"$with_ncurses" != x"no"])

#
# SQLITE library
#
AC_ARG_WITH([sqlite], AS_HELP_STRING([--without-sqlite], [Do not use sqlite database]))

AS_IF([test x"$with_sqlite" != x"no"],
	[AC_SEARCH_LIBS([sqlite3_prepare_v2], [sqlite3], [], [AC_ERROR([sqlite3 library not found])])],

	[AC_DEFINE([NO_SQLITE], [], [Do not compile against sqlite])]
	)

AM_CONDITIONAL([WITH_SQLITE], [test x"$with_sqlite" != x"no"])

unescaped_program_transform_name=`echo "${program_transform_name}"|sed -e "s&\\\\$\\\\$&\\\\$&g"`
transformed_program_name=`echo "${PACKAGE_NAME}"|sed -e "${unescaped_program_transform_name}"|sed -e "s&\\\\\\\\&\\\\\\\\\\\\\\\\&g"`
transformed_manpage_name=`echo "${PACKAGE_NAME}-help"|sed -e "${unescaped_program_transform_name}"`

AC_DEFINE_UNQUOTED([HELP_COMMAND_STRING], "man 7 ${transformed_manpage_name}", [fdupes help file])

AC_DEFINE([_FILE_OFFSET_BITS], [64], [allow fdupes to handle files greater than (2<<31)-1 bytes])

AC_DEFINE([CHUNK_SIZE], [8192], [number of bytes to read per read call])
AC_DEFINE([PARTIAL_MD5_SIZE], [4096], [maximum number of bytes to use when calculating partial hashes])
AC_DEFINE([INPUT_SIZE], [256], [size of command buffer (plain interactive mode only)])

AC_DEFINE([FDUPES_CACHE_DIRECTORY], ["fdupes"], [default subdirectory for fdupes config files])
AC_DEFINE([FDUPES_CACHE_DIRECTORY_PERMISSIONS], [0700], [directory permissions for fdupes config directory])
AC_DEFINE([FDUPES_HASH_DATABASE_NAME], ["hash.db"], [filename for fdupes hash database])

AC_CONFIG_FILES([Makefile])
AC_PROG_CC
AC_OUTPUT
