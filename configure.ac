dnl #
dnl # configure.ac
dnl #
dnl # PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
dnl #
dnl # Copyright (C) 2007-2013 William Weston <whw@linuxmail.org>
dnl #
dnl # PHASEX is free software: you can redistribute it and/or modify
dnl # it under the terms of the GNU General Public License as published by
dnl # the Free Software Foundation, either version 3 of the License, or
dnl # (at your option) any later version.
dnl #
dnl # PHASEX is distributed in the hope that it will be useful,
dnl # but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl # GNU General Public License for more details.
dnl #
dnl # You should have received a copy of the GNU General Public License
dnl # along with PHASEX.  If not, see <http://www.gnu.org/licenses/>.

AC_INIT([PHASEX], m4_esyscmd_s([sh scripts/version-sh]))
AC_CONFIG_AUX_DIR(scripts)
AM_INIT_AUTOMAKE(0.10.0)

###AM_MAINTAINER_MODE
###AM_ACLOCAL_INCLUDE(m4)


AC_PREFIX_DEFAULT(/usr/local)
ac_prefix=$prefix
if test "x$ac_prefix" = "xNONE" || test "x$ac_prefix" = "x"; then
    ac_prefix=$ac_default_prefix
fi
AC_SUBST(ac_prefix)

ac_datadir=$DATAROOTDIR
if test "x$ac_datadir" = "xNONE" || test "x$ac_datadir" = "x"; then
	ac_datadir="${ac_prefix}/share"
fi
AC_SUBST(ac_datadir)

ac_localedir=$LOCALEDIR
if test "x$ac_localedir" = "xNONE" || test "x$ac_localedir" = "x"; then
	ac_localedir="${ac_datadir}/locale"
fi
AC_SUBST(ac_localedir)


AC_DEFINE_UNQUOTED(CONFIG_PREFIX,        ["${ac_prefix}"],
                                         [Default installation prefix])
AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR,     ["${ac_datadir}"],
                                         [Default installation data prefix])
AC_DEFINE_UNQUOTED(PACKAGE_LOCALE_DIR,   ["${ac_localedir}"],
                                         [Default installation locale prefix])
AC_DEFINE_UNQUOTED(PHASEX_LICENSE_FILE,  ["${ac_datadir}/doc/phasex/LICENSE"],
                                         [PHASEX License Text])
AC_DEFINE_UNQUOTED(ALT_LICENSE_FILE,     ["${ac_datadir}/doc/phasex-${PACKAGE_VERSION}/LICENSE"],
                                         [PHASEX License Text (alternative location)])
AC_DEFINE_UNQUOTED(GPLv3_LICENSE_FILE,   ["${ac_datadir}/doc/phasex/GPL-3.0.txt"],
                                         [GPLv3 License Text])
AC_DEFINE_UNQUOTED(ALT_GPLv3_FILE,       ["${ac_datadir}/doc/phasex-${PACKAGE_VERSION}/GPL-3.0.txt"],
                                         [GPLv3 License Text (alternative location)])
AC_DEFINE_UNQUOTED(PHASEX_DIR,           ["${ac_datadir}/phasex"],
                                         [PHASEX Data Directory])
AC_DEFINE_UNQUOTED(PIXMAP_DIR,           ["${ac_datadir}/phasex/pixmaps"],
                                         [Pixmap Directory])
AC_DEFINE_UNQUOTED(PATCH_DIR,            ["${ac_datadir}/phasex/sys-patches"],
                                         [System Patch Directory])
AC_DEFINE_UNQUOTED(MIDIMAP_DIR,          ["${ac_datadir}/phasex/sys-midimaps"],
                                         [System MidiMap Directory])
AC_DEFINE_UNQUOTED(SAMPLE_DIR,           ["${ac_datadir}/phasex/sys-samples"],
                                         [System Sample Directory])
AC_DEFINE_UNQUOTED(PARAM_HELPFILE,       ["${ac_datadir}/phasex/help/parameters.help"],
                                         [Parameter Help File])
AC_DEFINE_UNQUOTED(PHASEX_GTK_ENGINE_RC, ["${ac_datadir}/phasex/gtkenginerc"],
                                         [GTKRC stub for theme engine])
AC_DEFINE_UNQUOTED(GTKRC_DARK,           ["${ac_datadir}/themes/Phasex-Dark/gtk-2.0/gtkrc"],
                                         [PHASEX Dark GTK Theme])
AC_DEFINE_UNQUOTED(GTKRC_LIGHT,          ["${ac_datadir}/themes/Phasex-Light/gtk-2.0/gtkrc"],
                                         [PHASEX Light GTK Theme])
AC_DEFINE_UNQUOTED(PACKAGE_BUGREPORT,    ["whw@linuxmail.org"],
                                         [Address where bug reports should be sent])


# save original vars while autoconf looks for programs (and changes user vars...)
ORIG_CFLAGS="$CFLAGS"
ORIG_LIBS="$LIBS"

#if test "x$CC" = "x"; then
#	CC="gcc -std=gnu99"
#fi

# Check for programs
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL


# Check for system headers, library functions, and compiler characteristics.
AC_HEADER_STDC
AC_C_CONST
AC_FUNC_MALLOC

AC_CHECK_FUNCS([sleep usleep poll fopen fgets fread fclose fprintf snprintf \
		strdup strerror strerror_r strstr strtok_r strcmp strncmp \
		strcasecmp atoi atof isdigit isupper memset memcpy mlockall])

# Check for math functions (should be built-in or available with -lm)
AC_CHECK_FUNC(atan2f,[], AC_CHECK_LIB(m, atan2f))
AC_CHECK_FUNC(cos,   [], AC_CHECK_LIB(m, cos))
AC_CHECK_FUNC(cosf,  [], AC_CHECK_LIB(m, cosf))
AC_CHECK_FUNC(exp,   [], AC_CHECK_LIB(m, exp))
AC_CHECK_FUNC(expf,  [], AC_CHECK_LIB(m, expf))
AC_CHECK_FUNC(fabs,  [], AC_CHECK_LIB(m, fabs))
AC_CHECK_FUNC(fabsf, [], AC_CHECK_LIB(m, fabsf))
AC_CHECK_FUNC(floor, [], AC_CHECK_LIB(m, floor))
AC_CHECK_FUNC(floorf,[], AC_CHECK_LIB(m, floorf))
AC_CHECK_FUNC(log,   [], AC_CHECK_LIB(m, log))
AC_CHECK_FUNC(logf,  [], AC_CHECK_LIB(m, logf))
AC_CHECK_FUNC(pow,   [], AC_CHECK_LIB(m, pow))
AC_CHECK_FUNC(powf,  [], AC_CHECK_LIB(m, powf))
AC_CHECK_FUNC(sin,   [], AC_CHECK_LIB(m, sin))
AC_CHECK_FUNC(sinf,  [], AC_CHECK_LIB(m, sinf))
AC_CHECK_FUNC(sqrt,  [], AC_CHECK_LIB(m, sqrt))
AC_CHECK_FUNC(sqrtf, [], AC_CHECK_LIB(m, sqrtf))

# Check for optionally implemented pthread functions
AC_CHECK_LIB(pthread, pthread_mutexattr_setprotocol,
	[AC_DEFINE_UNQUOTED(HAVE_PTHREAD_MUTEXATTR_SETPROTOCOL, [1],
	[libpthread has pthread_mutexattr_setprotocol])])

# Check for libraries

# X
if test "$X_DISPLAY_MISSING" != ""; then
  AC_MSG_ERROR([X required])
fi

# GLIB
# Glib version is checked due to dependence on glib/gatomic.h.
# GTK includes these flags already.  Use GTK_CFLAGS.
# Just keeping track for completeness.
PKG_CHECK_MODULES(GLIB,
  glib-2.0 >= 2.12,
  true,
  AC_MSG_ERROR([need glib >= 2.12])
)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

# GTK
PKG_CHECK_MODULES(GTK,
  gtk+-2.0 >= 2.4,
  true,
  AC_MSG_ERROR([need gtk+ >= 2.4])
)
AC_SUBST(GTK_CFLAGS)

# GMODULE
# At some point, -lgmodule was dropped from GTK_LIBS
AC_MSG_CHECKING(for -lgmodule in GTK_LIBS)
if echo $GLIB_LIBS $GTK_LIBS | grep '\-lgmodule' > /dev/null; then
  AC_MSG_RESULT([yes])
else
  AC_MSG_RESULT([no])
  PKG_CHECK_MODULES(GMODULE,
    gmodule-2.0 >= 2.12,
    true,
    AC_MSG_ERROR([need gmodule >= 2.12])
  )
  AC_SUBST(GMODULE_CFLAGS)
fi

# ALSA
PKG_CHECK_MODULES(ALSA,
  alsa >= 0.9.0,
  true,
  AC_MSG_ERROR([need ALSA >= 0.9.x]))
AC_SUBST(ALSA_CFLAGS)

# JACK
PKG_CHECK_MODULES(JACK,
  jack >= 0.99.0,
  true,
  AC_MSG_ERROR([need JACK >= 0.99.0])
)
AC_SUBST(JACK_CFLAGS)
AC_CHECK_HEADERS([jack/session.h])
AC_CHECK_HEADERS([jack/weakjack.h])
AC_CHECK_LIB(jack, jack_set_port_rename_callback,
  [AC_DEFINE_UNQUOTED(HAVE_JACK_SET_PORT_RENAME_CALLBACK, [1],
  [libjack has jack_set_port_rename_callback().])])
AC_CHECK_LIB(jack, jack_set_session_callback,
  [AC_DEFINE_UNQUOTED(HAVE_JACK_SET_SESSION_CALLBACK, [1],
  [libjack has jack_set_session_callback().])])
AC_CHECK_LIB(jack, jack_set_latency_callback,
  [AC_DEFINE_UNQUOTED(HAVE_JACK_SET_LATENCY_CALLBACK, [1],
  [libjack has jack_set_latency_callback().])])

# libsamplerate
PKG_CHECK_MODULES(SAMPLERATE,
  samplerate >= 0.1.2,
  true,
  AC_MSG_ERROR([need libsamplerate >= 0.1.2])
)
AC_SUBST(SAMPLERATE_CFLAGS)

# LASH
AC_ARG_WITH(lash,
  [AS_HELP_STRING([--without-lash],
    [disable LASH support])],
  [without_lash=1
  AC_DEFINE([WITHOUT_LASH], [], [Build without LASH support])],
  true
)

if test x$without_lash != x1; then
  PKG_CHECK_MODULES(LASH,
    lash-1.0 >= 0.5.4,
    true,
    [AC_MSG_WARN([lash >= 0.5.4 not found. Building without LASH support.])
    without_lash=1
    AC_DEFINE([WITHOUT_LASH], [], [Build without LASH support])]
  )
  AC_SUBST(LASH_CFLAGS)
fi

# UUID (required only by LASH)
if test x$without_lash != x1; then
  PKG_CHECK_MODULES(UUID,
    uuid >= 2.16,
    true,
    [AC_MSG_WARN([libuuid >= 2.16 not found. Building without LASH support.])
    without_lash=1
    AC_DEFINE([WITHOUT_LASH], [], [Build without LASH support])]
  )
  AC_SUBST(UUID_CFLAGS)
fi

AM_CONDITIONAL(WITH_LASH, test x$without_lash != x1)

# librt (for clock_gettime and clock_nanosleep)
have_librt="no"
found_librt="no"
AC_ARG_WITH(librt,
  AS_HELP_STRING([--with-librt], [Use librt for monotonic clock (default = yes)]),
  [ have_librt="$withval" ], [ have_librt="yes" ])
if test "$found_librt" = "no"; then
  AC_MSG_CHECKING(for -lrt in GTK_LIBS)
  if echo $GTK_LIBS | grep -q '\-lgmodule'; then
    AC_MSG_RESULT([yes])
    found_librt="yes"
  else
    AC_MSG_RESULT([no])
    found_librt="no"
  fi
fi
if test "$found_librt" = "no"; then
  AC_MSG_CHECKING(for -lrt in GLIB_LIBS)
  if echo $GLIB_LIBS $GTK_LIBS | grep -q '\-lgmodule'; then
    AC_MSG_RESULT([yes])
    found_librt="yes"
  else
    AC_MSG_RESULT([no])
    found_librt="no"
  fi
fi
if test "$found_librt" = "no"; then
  AC_MSG_CHECKING(for -lrt in GMODULE_LIBS)
  if echo $GMODULE_LIBS | grep -q '\-lgmodule'; then
    AC_MSG_RESULT([yes])
    found_librt="yes"
  else
    AC_MSG_RESULT([no])
    found_librt="no"
  fi
fi
if test "$have_librt" = "yes" || test "$found_librt" = "yes"; then
  AC_CHECK_LIB([rt], [clock_gettime], [HAVE_LIBRT="yes"])
  AC_CHECK_LIB([rt], [clock_nanosleep], [HAVE_LIBRT="yes"])
  if test "$HAVE_LIBRT" = "yes" ; then
    if test "$found_librt" = "no"; then
      RT_LIBS="$RT_LIBS -lrt"
    fi
    AC_DEFINE([HAVE_LIBRT], 1, [Have librt])
    AC_DEFINE([HAVE_CLOCK_GETTIME], 1, [Have clock gettime])
    AC_DEFINE([HAVE_CLOCK_NANOSLEEP], 1, [Have clock gettime])
  fi
fi
AC_SUBST(RT_LIBS)


# restore original vars
CFLAGS="$ORIG_CFLAGS"
CONF_LIBS="$LIBS"
LIBS="$ORIG_LIBS"


# piece together gtk engine directory
gtk_lib_dir=`$PKG_CONFIG --variable libdir gtk+-2.0`
gtk_binary_version=`$PKG_CONFIG --variable gtk_binary_version gtk+-2.0`
gtk_engine_dir="$gtk_lib_dir/gtk-2.0/$gtk_binary_version/engines"
AC_DEFINE_UNQUOTED(GTK_ENGINE_DIR, ["$gtk_engine_dir"], [GTK engine directory])


# --enable-debug=FOO option:  pass debug flags to compiler
DEBUG_CFLAGS="none"
AC_ARG_ENABLE(debug,
	AC_HELP_STRING([--enable-debug=FLAGS], [pass FLAGS to compiler for debugging.]),
	[DEBUG_CFLAGS="$enableval"])

# --enable-arch=FOO option:  select arch specific compilation
OPT_ARCH="none"
AC_ARG_ENABLE(arch,
	AC_HELP_STRING([--enable-arch=ARCH], [set the -march=ARCH and -mtune=ARCH flags for gcc.
					      (see the gcc man page for supported cpu types.)]),
	[OPT_ARCH="$enableval"])

# --enable-parts=FOO option:  build for FOO active synth parts
NUM_PARTS="1"
AC_ARG_ENABLE(parts,
	AC_HELP_STRING([--enable-parts=N], [build with N active synth parts.]),
	[NUM_PARTS="$enableval"])

if test $NUM_PARTS -lt 1; then
	NUM_PARTS="1"
fi
if test $NUM_PARTS -gt 12; then
	NUM_PARTS="12"
fi


# determine gcc version
gccver=`$CC -v 2>&1 | grep '^gcc version' | cut -f 3 -d ' '`
gccmajor=`echo "$gccver" | cut -f 1 -d '.'`
gccminor=`echo "$gccver" | cut -f 2 -d '.'`

AC_DEFINE(PHASEX_GCC_VERSION, ["$gccver"], [Built with gcc-$gccver])
AC_DEFINE_UNQUOTED(PHASEX_GCC_MAJOR, [$gccmajor], [Built with gcc major version])
AC_DEFINE_UNQUOTED(PHASEX_GCC_MINOR, [$gccminor], [Built with gcc minor version])


# compiler flags
CPU_POWER_LEVEL=2
ARCH_OPT_CFLAGS=""
ARCH_MATH_CFLAGS=""
ARCH_TRUE=`which true`
ARCH_BITS=`( file $ARCH_TRUE | grep 'ELF 64-bit' > /dev/null && echo 64 ) || echo 32`

WARNING=`echo "*"`
case "$OPT_ARCH" in
	"none"|"generic")
		CPU_POWER_LEVEL=2
		if test "x$gccver" \< "x4.2.0"; then
			ARCH_OPT_CFLAGS="-mtune=i686 -march=i386"
			WARNING=`echo "*" && \
				 echo "* WARNING:  No arch specific optimization has been specified." && \
				 echo "*           Defaulting to i686 tuning for i386 CPU !!!" && \
				 echo "*"`
		else
			ARCH_OPT_CFLAGS="-mtune=generic"
			WARNING=`echo "*" && \
				 echo "* WARNING:  No arch specific optimization has been specified." && \
				 echo "*           Try --enable-arch=native !!!" && \
				 echo "*"`
		fi
		ARCH_MATH_CFLAGS=""
		;;
	"native")
		CPU_POWER_LEVEL=2
		if test "x$gccver" \< "x4.2.0"; then
			ARCH_OPT_CFLAGS=""
				WARNING=`echo "*" && \
				 echo "* WARNING:  --enable-arch=native can only be used with gcc-4.2.x and above!" && \
				 echo "*           No arch specific optimizations will be used!" && \
				 echo "*"`
			OPT_ARCH="none"
		else
			ARCH_OPT_CFLAGS="-mtune=native -march=native"
		fi
		ARCH_MATH_CFLAGS=""
		;;
	"i386"|"i486"|"i586"|"pentium")
		CPU_POWER_LEVEL=1
		ARCH_OPT_CFLAGS="-mtune=i586 -march=$OPT_ARCH"
		ARCH_MATH_CFLAGS=""
		ARCH_BITS=32
		;;
	"i686"|"pentiumpro")
		CPU_POWER_LEVEL=1
		ARCH_OPT_CFLAGS="-mtune=$OPT_ARCH -march=$OPT_ARCH"
		ARCH_MATH_CFLAGS=""
		ARCH_BITS=32
		;;
	"pentium-mmx"|"pentium2"|"k6"|"winchip-c6")
		CPU_POWER_LEVEL=1
		ARCH_OPT_CFLAGS="-mtune=$OPT_ARCH -march=$OPT_ARCH"
		ARCH_MATH_CFLAGS="-mmmx"
		ARCH_BITS=32
		;;
	"pentium3"|"pentium3m"|"c3-2")
		CPU_POWER_LEVEL=1
		ARCH_OPT_CFLAGS="-mtune=$OPT_ARCH -march=$OPT_ARCH"
		ARCH_MATH_CFLAGS="-msse -mfpmath=sse,387"
		ARCH_BITS=32
		;;
	"pentium-m"|"pentium4"|"pentium4m")
		CPU_POWER_LEVEL=2
		ARCH_OPT_CFLAGS="-mtune=$OPT_ARCH -march=$OPT_ARCH"
		ARCH_MATH_CFLAGS="-msse2 -mfpmath=sse,387"
		ARCH_BITS=32
		;;
	"k6-2"|"k6-3"|"winchip2"|"c3"|"geode")
		CPU_POWER_LEVEL=1
		ARCH_OPT_CFLAGS="-mtune=$OPT_ARCH -march=$OPT_ARCH"
		ARCH_MATH_CFLAGS="-mmmx -m3dnow"
		ARCH_BITS=32
		;;
	"athlon")
		CPU_POWER_LEVEL=2
		ARCH_OPT_CFLAGS="-mtune=athlon-xp -march=$OPT_ARCH"
		ARCH_MATH_CFLAGS="-mmmx -m3dnow"
		ARCH_BITS=32
		;;
	"athlon-tbird")
		CPU_POWER_LEVEL=2
		ARCH_OPT_CFLAGS="-mtune=$OPT_ARCH -march=$OPT_ARCH"
		ARCH_MATH_CFLAGS="-mmmx -m3dnow"
		ARCH_BITS=32
		;;
	"athlon-4"|"athlon-mp"|"athlon-xp")
		CPU_POWER_LEVEL=2
		ARCH_OPT_CFLAGS="-mtune=$OPT_ARCH -march=$OPT_ARCH"
		ARCH_MATH_CFLAGS="-msse -mfpmath=sse,387"
		ARCH_BITS=32
		;;
	"k8"|"opteron"|"athlon64"|"athlon-fx")
		CPU_POWER_LEVEL=2
		ARCH_OPT_CFLAGS="-mtune=$OPT_ARCH -march=$OPT_ARCH"
		ARCH_MATH_CFLAGS="-msse2 -mfpmath=sse"
		ARCH_BITS=64
		;;
	"k8-sse3"|"opteron-sse3"|"athlon64-sse3")
		CPU_POWER_LEVEL=2
		if test "x$gccver" \< "x4.4.0"; then
			ARCH_OPT_CFLAGS="-mtune=athlon64 -march=athlon64"
			ARCH_MATH_CFLAGS="-msse2"
		else
			ARCH_OPT_CFLAGS="-mtune=$OPT_ARCH -march=$OPT_ARCH"
			ARCH_MATH_CFLAGS="-msse3"
		fi
		ARCH_BITS=64
		;;
	"amdfam10")
		CPU_POWER_LEVEL=3
		ARCH_OPT_CFLAGS="-mtune=$OPT_ARCH -march=$OPT_ARCH"
		ARCH_MATH_CFLAGS="-msse4a -mfpmath=sse"
		ARCH_BITS=64
		;;
	"barcelona")
		CPU_POWER_LEVEL=3
		if test "x$gccver" \< "x4.4.0"; then
			ARCH_OPT_CFLAGS="-mtune=amdfam10 -march=amdfam10"
		else
			ARCH_OPT_CFLAGS="-mtune=barcelona -march=barcelona"
		fi
		ARCH_MATH_CFLAGS="-msse4a -mfpmath=sse"
		ARCH_BITS=64
		;;
	"bdver1"|"bdver2")
		CPU_POWER_LEVEL=3
		if test "x$gccver" \< "x4.6.0"; then
			ARCH_OPT_CFLAGS="-mtune=amdfam10 -march=amdfam10"
		else
			ARCH_OPT_CFLAGS="-mtune=$OPT_ARCH -march=$OPT_ARCH"
		fi
		ARCH_MATH_CFLAGS="-msse2 -msse3 -mssse3 -msse4a -msse4.1 -msse4.2 -mfpmath=sse"
		ARCH_BITS=64
		;;
    "btver1")
		CPU_POWER_LEVEL=3
		if test "x$gccver" \< "x4.6.0"; then
			ARCH_OPT_CFLAGS="-mtune=amdfam10 -march=amdfam10"
		else
			ARCH_OPT_CFLAGS="-mtune=$OPT_ARCH -march=$OPT_ARCH"
		fi
		ARCH_MATH_CFLAGS="-msse2 -msse3 -mssse3 -msse4a -mfpmath=sse"
		ARCH_BITS=64
		;;
	"x86_64")
		CPU_POWER_LEVEL=3
		ARCH_OPT_CFLAGS="-mtune=generic"
		ARCH_MATH_CFLAGS="-msse2 -mfpmath=sse"
		ARCH_BITS=64
		;;
	"prescott")
		CPU_POWER_LEVEL=2
		ARCH_OPT_CFLAGS="-mtune=$OPT_ARCH -march=$OPT_ARCH"
		ARCH_MATH_CFLAGS="-msse3 -mfpmath=sse"
		ARCH_BITS=32
		;;
	"nocona")
		CPU_POWER_LEVEL=2
		ARCH_OPT_CFLAGS="-mtune=$OPT_ARCH -march=$OPT_ARCH"
		ARCH_MATH_CFLAGS="-msse3 -mfpmath=sse"
		ARCH_BITS=32
		;;
	"core2")
		CPU_POWER_LEVEL=3
		if test "x$gccver" \< "x4.2.0"; then
			ARCH_OPT_CFLAGS="-mtune=nocona -march=nocona"
		else
			ARCH_OPT_CFLAGS="-mtune=core2 -march=core2"
		fi
		ARCH_MATH_CFLAGS="-msse -msse2 -mssse3 -mfpmath=sse"
		ARCH_BITS=64
		;;
	"corei7")
		CPU_POWER_LEVEL=3
		if test "x$gccver" \< "x4.6.0"; then
			ARCH_OPT_CFLAGS="-mtune=core2 -march=core2"
		else
			ARCH_OPT_CFLAGS="-mtune=corei7 -march=corei7"
		fi
		ARCH_MATH_CFLAGS="-msse -msse2 -mssse3 -mssse3 -msse4.1 -msse4.2 -mfpmath=sse"
		ARCH_BITS=64
		;;
	"corei7-avx")
		CPU_POWER_LEVEL=3
		if test "x$gccver" \< "x4.6.0"; then
			ARCH_OPT_CFLAGS="-mtune=core2 -march=core2"
		else
			ARCH_OPT_CFLAGS="-mtune=corei7 -march=corei7"
		fi
		ARCH_MATH_CFLAGS="-msse -msse2 -mssse3 -mssse3 -msse4.1 -msse4.2 -mfpmath=sse"
		ARCH_BITS=64
		;;
	"corei7-avx-i")
		CPU_POWER_LEVEL=3
		if test "x$gccver" \< "x4.6.0"; then
			ARCH_OPT_CFLAGS="-mtune=core2 -march=core2"
		else
			ARCH_OPT_CFLAGS="-mtune=corei7 -march=corei7"
		fi
		ARCH_MATH_CFLAGS="-msse -msse2 -mssse3 -mssse3 -msse4.1 -msse4.2 -mfpmath=sse"
		ARCH_BITS=64
		;;
	"amd64")
		CPU_POWER_LEVEL=2
		ARCH_OPT_CFLAGS="-mtune=athlon64 -march=athlon64"
		ARCH_MATH_CFLAGS=""
		ARCH_BITS=64
		;;
	"itanium"|"itanium1"|"merced"|"itanium2"|"mckinley")
		CPU_POWER_LEVEL=2
		ARCH_OPT_CFLAGS="-mtune=$OPT_ARCH"
		ARCH_MATH_CFLAGS=""
		ARCH_BITS=64
		;;
	"ia64")
		CPU_POWER_LEVEL=2
		ARCH_OPT_CFLAGS="-mtune=itanium"
		ARCH_MATH_CFLAGS=""
		ARCH_BITS=64
		;;
	"ia32e")
		CPU_POWER_LEVEL=2
		ARCH_OPT_CFLAGS="-mtune=generic"
		ARCH_MATH_CFLAGS=""
		ARCH_BITS=32
		;;
	"atom"|"atom32")
		CPU_POWER_LEVEL=2
		if test "x$gccver" \< "x4.2.0"; then
			ARCH_OPT_CFLAGS="-mtune=prescott"
		elif test "x$gccver" \< "x4.3.0"; then
			ARCH_OPT_CFLAGS="-mtune=generic -march=prescott"
		elif test "x$gccver" \< "x4.5.0"; then
			ARCH_OPT_CFLAGS="-mtune=core2 -march=core2"
		else
			ARCH_OPT_CFLAGS="-mtune=atom -march=atom"
		fi
		ARCH_MATH_CFLAGS="-msse3 -mfpmath=sse"
		ARCH_BITS=32
		;;
	"atom64")
		CPU_POWER_LEVEL=2
		if test "x$gccver" \< "x4.2.0"; then
			ARCH_OPT_CFLAGS="-mtune=prescott"
		elif test "x$gccver" \< "x4.3.0"; then
			ARCH_OPT_CFLAGS="-mtune=generic -march=prescott"
		elif test "x$gccver" \< "x4.5.0"; then
			ARCH_OPT_CFLAGS="-mtune=core2 -march=core2"
		else
			ARCH_OPT_CFLAGS="-mtune=atom -march=atom"
		fi
		ARCH_MATH_CFLAGS="-msse3 -mfpmath=sse"
		ARCH_BITS=64
		;;
	"geode")
		CPU_POWER_LEVEL=2
		if test "x$gccver" \< "x4.3.0"; then
			ARCH_OPT_CFLAGS="-mtune=athlon -march=athlon"
		else
			ARCH_OPT_CFLAGS="-mtune=geode -march=geode"
		fi
		ARCH_MATH_CFLAGS="-msse3 -mfpmath=sse"
		ARCH_BITS=64
		;;

	*)
		CPU_POWER_LEVEL=2
		ARCH_OPT_CFLAGS="-mtune=$OPT_ARCH -march=$OPT_ARCH"
		ARCH_MATH_CFLAGS=""
		;;
esac

if test "x$gccver" \< "x4.2.0"; then
	ARCH_OPT_CFLAGS=`echo $ARCH_OPT_CFLAGS | sed -e 's/\-mtune=generic//g'`
	ARCH_MATH_CFLAGS=`echo $ARCH_MATH_CFLAGS | sed -e 's/\-mssse3//g' -e 's/\-msse4\(\.[12]\)*//g'`
fi

# --enable-32bit option:  Force 32 bit builds (mainly for cross compiling...)
FORCE_32_BIT=0
AC_ARG_ENABLE(32bit,
	AC_HELP_STRING([--enable-32bit], [force 32-bit build (-m32) in 64-bit environment.]),
  	[FORCE_32_BIT=32])

if test "x$FORCE_32_BIT" = "x32"; then
	ARCH_BITS=32;
fi


# Apply user-overridden PHASEX_CPU_POWER flag
FORCE_CPU_POWER=0
AC_ARG_ENABLE(cpu-power,
	AC_HELP_STRING([--enable-cpu-power], [optimize sound quality for (1) low-, (2) medium-, (3) high- , or (4) over- powered CPU.]),
	[FORCE_CPU_POWER="$enableval"])

if test "x$FORCE_CPU_POWER" = "x1" || test "x$FORCE_CPU_POWER" = "x2" || test "x$FORCE_CPU_POWER" = "x3" || test "x$FORCE_CPU_POWER" = "x4"; then
	CPU_POWER_LEVEL="$FORCE_CPU_POWER"
fi


# set optimization flags, with fewer optimizations when --enable-debug is given
case "$DEBUG_CFLAGS" in
	"none")
		DEBUG_CFLAGS=""
		OPT_CFLAGS="-pipe -Wall $ARCH_OPT_CFLAGS -m$ARCH_BITS $ARCH_MATH_CFLAGS -O3 -ffast-math -fomit-frame-pointer -fsingle-precision-constant"
		;;
	"yes")
		DEBUG_CFLAGS="-g"
		OPT_CFLAGS="-pipe -Wall -Wcast-align -Wconversion -Wextra $ARCH_OPT_CFLAGS -m$ARCH_BITS $ARCH_MATH_CFLAGS -O2 -ffast-math -fsingle-precision-constant"
		;;
	*)
		DEBUG_CFLAGS="-g"
		OPT_CFLAGS="-pipe -Wall -Wcast-align -Wconversion -Wextra $ARCH_OPT_CFLAGS -m$ARCH_BITS $ARCH_MATH_CFLAGS -fsingle-precision-constant"
		;;
esac

# test user supplied CFLAGS for options already set
if echo "$CFLAGS" | grep '\-O' > /dev/null; then
	NEW_CFLAGS=`echo $OPT_CFLAGS | /bin/sed -e 's/\-O[[0-9]]*//g'`
	OPT_CFLAGS="$NEW_CFLAGS"
fi
if echo "$CFLAGS" | grep '\-march=' > /dev/null; then
	NEW_CFLAGS=`echo $OPT_CFLAGS | /bin/sed -e 's/\-march=[[a-zA-Z0-9]]\+//g'`
	OPT_CFLAGS="$NEW_CFLAGS"
fi
if echo "$CFLAGS" | grep '\-mtune=' > /dev/null; then
	NEW_CFLAGS=`echo $OPT_CFLAGS | /bin/sed -e 's/\-mtune=[[a-zA-Z0-9]]\+//g'`
	OPT_CFLAGS="$NEW_CFLAGS"
fi

# Put all compile flags together
PHASEX_CFLAGS="-std=gnu99 $OPT_CFLAGS $DEBUG_CFLAGS"
AC_SUBST(PHASEX_CFLAGS)

PHASEX_CPPFLAGS="$ALSA_CFLAGS $JACK_CFLAGS $GTK_CFLAGS $GMODULE_CFLAGS $SAMPLERATE_CFLAGS $LASH_CFLAGS $UUID_CFLAGS $RT_CFLAGS -D_GNU_SOURCE -D_XOPEN_SOURCE=600 -D_REENTRANT -DARCH_BITS=$ARCH_BITS -DPHASEX_CPU_POWER=$CPU_POWER_LEVEL -DNUM_PARTS=$NUM_PARTS"
AC_SUBST(PHASEX_CPPFLAGS)

PHASEX_LIBS="$ALSA_LIBS $JACK_LIBS $GTK_LIBS $GMODULE_LIBS $SAMPLERATE_LIBS $LASH_LIBS $UUID_LIBS $RT_LIBS $CONF_LIBS"
if ! echo "$PHASEX_LIBS $LIBS" | grep '\-lpthread' > /dev/null; then
	PHASEX_LIBS="$PHASEX_LIBS -lpthread"
fi
if ! echo "$PHASEX_LIBS $LIBS" | grep '\-lX11' > /dev/null; then
	PHASEX_LIBS="$PHASEX_LIBS -lX11"
fi
AC_SUBST(PHASEX_LIBS)


# Output files
AC_CONFIG_FILES([
	Makefile
	src/Makefile
	help/Makefile
	pixmaps/Makefile
	themes/Makefile
	patches/Makefile
	samples/Makefile
	midimaps/Makefile
	misc/Makefile
	scripts/Makefile
	phasex.spec
	misc/phasex.desktop
])
AC_CONFIG_HEADERS([config.h])

AC_OUTPUT


# Display build info
AC_MSG_NOTICE([
******************************************************************************
*
* Build configuration for phasex-$PACKAGE_VERSION:
*
* Optimized for arch: ... $OPT_ARCH (${ARCH_BITS}-bit)
* CPU power level: ...... $CPU_POWER_LEVEL / 4
*
* Active synth parts: ... $NUM_PARTS
*
* CC: ................... $CC
* GCC version: .......... $gccver
*
* GTK binary version: ... $gtk_binary_version
* GTK engine dir: ....... $gtk_engine_dir
*
* CFLAGS: ............... '$CFLAGS'
* PHASEX_CFLAGS: ........ '$PHASEX_CFLAGS'
*
* CPPFLAGS: ............. '$CPPFLAGS'
* PHASEX_CPPFLAGS: ...... '$PHASEX_CPPFLAGS'
*
* LDFLAGS: .............. '$LDFLAGS'
* PHASEX_LDFLAGS: ....... '$PHASEX_LDFLAGS'
*
* LIBS: ................. '$LIBS'
* PHASEX_LIBS: .......... '$PHASEX_LIBS'
*
******************************************************************************
*
* PHASEX is now configured!  Now run:
*
*    make clean
*    make
*    make install
$WARNING
******************************************************************************])
