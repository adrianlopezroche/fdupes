AC_INIT([fdupes], [2.1.2])

AM_INIT_AUTOMAKE([foreign subdir-objects])

AC_CONFIG_HEADERS([config.h])

AC_ARG_PROGRAM

PKG_PROG_PKG_CONFIG

AC_ARG_WITH([ncurses], AS_HELP_STRING([--without-ncurses], [Do not use ncurses interface]))

AC_CHECK_HEADERS([getopt.h ncursesw/curses.h])

AS_IF([test x"$with_ncurses" != x"no"],
	[PKG_CHECK_MODULES([NCURSES], [ncursesw],
		[LIBS="$LIBS $NCURSES_LIBS"],
		[AC_SEARCH_LIBS([wget_wch], [ncursesw ncurses curses], [], [AC_ERROR([ncurses library not found (or lacks wide character support)])])]
		[AC_SEARCH_LIBS([keypad], [ncursesw tinfow ncurses tinfo curses], [], [AC_ERROR([ncurses library not found (lacks keypad support)])])]
	)]
	[AC_DEFINE([_XOPEN_SOURCE], [], [enable certain functions in wchar.h])]
	[AC_DEFINE([_XOPEN_SOURCE_EXTENDED], [], [enable certain functions in curses.h])]
	[AC_DEFINE([_ISOC99_SOURCE], [], [enable strtoll])]
	[AC_SEARCH_LIBS([pcre2_match_32], [pcre2-32], [], [AC_ERROR([pcre2 library not found])])]
	[AC_DEFINE([PCRE2_CODE_UNIT_WIDTH], [32], [PCRE2 Code Unit Width])],

	[AC_DEFINE([NO_NCURSES], [], [Do not compile against ncurses])]
	)

AM_CONDITIONAL([NO_NCURSES], [test x"$with_ncurses" = x"no"])

unescaped_program_transform_name=`echo "${program_transform_name}"|sed -e "s&\\\\$\\\\$&\\\\$&g"`
transformed_program_name=`echo "${PACKAGE_NAME}"|sed -e "${unescaped_program_transform_name}"|sed -e "s&\\\\\\\\&\\\\\\\\\\\\\\\\&g"`
transformed_manpage_name=`echo "${PACKAGE_NAME}-help"|sed -e "${unescaped_program_transform_name}"`

AC_DEFINE_UNQUOTED([HELP_COMMAND_STRING], "man 7 ${transformed_manpage_name}", [fdupes help file])

AC_DEFINE([_FILE_OFFSET_BITS], [64], [allow fdupes to handle files greater than (2<<31)-1 bytes])

AC_CONFIG_FILES([Makefile])
AC_PROG_CC
AC_OUTPUT
