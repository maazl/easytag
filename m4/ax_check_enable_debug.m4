# ===========================================================================
#   http://www.gnu.org/software/autoconf-archive/ax_check_enable_debug.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_CHECK_ENABLE_DEBUG([enable by default=yes/info/profile/no], [ENABLE DEBUG VARIABLES ...], [DISABLE DEBUG VARIABLES NDEBUG ...])
#
# DESCRIPTION
#
#   Check for the presence of an --enable-debug option to configure, with
#   the specified default value used when the option is not present.  Return
#   the value in the variable $ax_enable_debug.
#
#   Specifying 'yes' adds '-g -O0' to the compilation flags for all
#   languages. Specifying 'info' adds '-g' to the compilation flags.
#   Specifying 'profile' adds '-g -pg' to the compilation flags and '-pg' to
#   the linking flags. Otherwise, nothing is added.
#
#   Define the variables listed in the second argument if debug is enabled,
#   defaulting to no variables.  Defines the variables listed in the third
#   argument if debug is disabled, defaulting to NDEBUG.  All lists of
#   variables should be space-separated.
#
#   If debug is not enabled, ensure AC_PROG_* will not add debugging flags.
#   Should be invoked prior to any AC_PROG_* compiler checks.
#
# LICENSE
#
#   Copyright (c) 2011 Rhys Ulerich <rhys.ulerich@gmail.com>
#   Copyright (c) 2014 Philip Withnall <philip@tecnocode.co.uk>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved.

#serial 3

AC_DEFUN([AX_CHECK_ENABLE_DEBUG],[
    AC_BEFORE([$0],[AC_PROG_CC])dnl
    AC_BEFORE([$0],[AC_PROG_CXX])dnl
    AC_BEFORE([$0],[AC_PROG_F77])dnl
    AC_BEFORE([$0],[AC_PROG_FC])dnl

    AC_MSG_CHECKING(whether to enable debugging)

    m4_define(ax_enable_debug_default,[m4_tolower(m4_normalize(ifelse([$1],,[no],[$1])))])
    m4_define(ax_enable_debug_vars,[m4_normalize(ifelse([$2],,,[$2]))])
    m4_define(ax_disable_debug_vars,[m4_normalize(ifelse([$3],,[NDEBUG],[$3]))])

    AC_ARG_ENABLE(debug,
	[AS_HELP_STRING([--enable-debug]@<:@=ax_enable_debug_default@:>@,[compile with debugging; one of yes/info/profile/no])],
	[],AS_CASE([$ax_is_release], [yes],enable_debug=[no], [no],enable_debug=[yes], enable_debug=ax_enable_debug_default))

    # empty mean debug yes
    AS_IF([test "x$enable_debug" = "x"],
      [enable_debug="yes"])

    # case of debug
    AS_CASE([$enable_debug],
      [yes],[
	AC_MSG_RESULT(yes)
	CFLAGS="${CFLAGS} -g -O0"
	CXXFLAGS="${CXXFLAGS} -g -O0"
	FFLAGS="${FFLAGS} -g -O0"
	FCFLAGS="${FCFLAGS} -g -O0"
	OBJCFLAGS="${OBJCFLAGS} -g -O0"
      ],
      [info],[
	AC_MSG_RESULT(info)
	CFLAGS="${CFLAGS} -g"
	CXXFLAGS="${CXXFLAGS} -g"
	FFLAGS="${FFLAGS} -g"
	FCFLAGS="${FCFLAGS} -g"
	OBJCFLAGS="${OBJCFLAGS} -g"
      ],
      [profile],[
	AC_MSG_RESULT(profile)
	CFLAGS="${CFLAGS} -g -pg"
	CXXFLAGS="${CXXFLAGS} -g -pg"
	FFLAGS="${FFLAGS} -g -pg"
	FCFLAGS="${FCFLAGS} -g -pg"
	OBJCFLAGS="${OBJCFLAGS} -g -pg"
	LDFLAGS="${LDFLAGS} -pg"
      ],
      [
	AC_MSG_RESULT(no)
	CFLAGS="${CFLAGS} -O2"
	CXXFLAGS="${CXXFLAGS} -O2"
	FFLAGS="${FFLAGS} -O2"
	FCFLAGS="${FCFLAGS} -O2"
	OBJCFLAGS="${OBJCFLAGS} -O2"
      ])

    dnl Define various variables if debugging is disabled.
    dnl assert.h is a NOP if NDEBUG is defined, so define it by default.
    AS_IF([test "x$enable_debug" = "xyes"],
      [m4_map_args_w(ax_enable_debug_vars, [AC_DEFINE(], [,,[Define if debugging is enabled])])],
      [m4_map_args_w(ax_disable_debug_vars, [AC_DEFINE(], [,,[Define if debugging is disabled])])])
    ax_enable_debug=$enable_debug
])
