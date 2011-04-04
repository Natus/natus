dnl Process this file with autoconf to produce a configure script.

m4_define_default([PKG_CHECK_MODULES], [:])

AC_PREREQ(2.59)
AC_INIT(natus, 0.2.0)
AC_SUBST(PACKAGE_NAME)

AC_CANONICAL_SYSTEM
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE()

LT_INIT([disable-static])

AC_PROG_CPP
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL
AM_PROG_LIBTOOL
AM_PROG_CC_C_O


m4_ifdef([AM_SILENT_RULES], [
  AM_SILENT_RULES([yes])
], [
  AM_DEFAULT_VERBOSITY=1
  AC_SUBST(AM_DEFAULT_VERBOSITY)
])

PKG_CHECK_MODULES(SpiderMonkey, libjs >= 1.8, have_spidermonkey=yes,
	[PKG_CHECK_MODULES(SpiderMonkey, xulrunner-js >= 2, have_spidermonkey=yes, 
	[PKG_CHECK_MODULES(SpiderMonkey, firefox-js   >= 2, have_spidermonkey=yes,
	[PKG_CHECK_MODULES(SpiderMonkey, mozilla-js   >= 2, have_spidermonkey=yes, 
	[PKG_CHECK_MODULES(SpiderMonkey, seamonkey-js >= 2, have_spidermonkey=yes,
	[PKG_CHECK_MODULES(SpiderMonkey, mozilla2-js  >= 2, have_spidermonkey=yes, have_spidermonkey=no)])])])])])
AC_ARG_WITH([SpiderMonkey],
            [AS_HELP_STRING([--without-spidermonkey],
              [disable the SpiderMonkey (Mozilla) engine @<:@automatic@:>@])],
            [with_spidermonkey=no],
            [test x$have_spidermonkey = xyes && with_spidermonkey=yes])
if test x$with_spidermonkey = xyes; then
	AC_SUBST(spidermonkey_CFLAGS)
	AC_SUBST(spidermonkey_LIBS)
	if test x$have_spidermonkey = xno; then
		echo "SpiderMonkey (Mozilla) engine requires: libjs >= 1.8 or xulrunner >= 2.0!"
		exit 1
	fi
fi
AM_CONDITIONAL([WITH_SPIDERMONKEY], [test x$with_spidermonkey = xyes])

PKG_CHECK_MODULES(JavaScriptCore, webkitgtk-3.0, have_javascriptcore=yes,
	[PKG_CHECK_MODULES(JavaScriptCore, webkit-1.0, have_javascriptcore=yes, have_javascriptcore=no)])
AC_ARG_WITH([javascriptcore],
            [AS_HELP_STRING([--without-javascriptcore],
              [build the JavaScriptCore (WebKit) engine @<:@automatic@:>@])],
            [with_javascriptcore=no],
            [test x$have_javascriptcore = xyes && with_javascriptcore=yes])
if test x$with_javascriptcore = xyes; then
	AC_SUBST(javascriptcore_CFLAGS)
	AC_SUBST(javascriptcore_LIBS)
	if test x$have_javascriptcore = xno; then
		echo "JavaScriptCore (WebKit) engine requires: webkit!"
		exit 1
	fi
fi
AM_CONDITIONAL([WITH_JAVASCRIPTCORE], [test x$with_javascriptcore = xyes])

AC_LANG_PUSH([C++])
AC_CHECK_HEADER(v8.h, have_v8=yes, have_v8=no)
AC_LANG_POP
AC_ARG_WITH([v8],
            [AS_HELP_STRING([--without-v8],
              [disable the V8 (Chrome/Chromium) engine @<:@automatic@:>@])],
            [with_v8=no],
            [test x$have_v8 = xyes && with_v8=yes])
if test x$with_v8 = xyes; then
    if test x$have_v8 = xno; then
        echo "V8 (Chrome/Chromium) engine requires: v8!"
        exit 1
    fi
fi
AM_CONDITIONAL([WITH_V8], [test x$with_v8 = xyes])

AM_PATH_PYTHON()
PKG_CHECK_MODULES(python, python-${PYTHON_VERSION}, have_python=yes,
        [PKG_CHECK_MODULES(python, python, have_python=yes, have_python=no)])
# Try to work around missing pkgconfig files...
if test x$have_python != xyes && test x$PYTHON != x:; then
    CFLAGSBAK=${CFLAGS}
    CPPFLAGSBAK=${CPPFLAGS}
    CFLAGS=-I`$PYTHON -c 'import sys; print sys.prefix'`/include/python${PYTHON_VERSION}
    CPPFLAGS=${CFLAGS}
    AC_CHECK_HEADER(Python.h, have_python=yes, have_python=no)
    if test x$have_python = xyes; then
        python_CFLAGS=${CFLAGS}
        python_LIBS=-lpython${PYTHON_VERSION}
    fi
    CFLAGS=${CFLAGSBAK}
    CPPFLAGS=${CPPFLAGSBAK}
fi
AC_ARG_WITH([python],
            [AS_HELP_STRING([--without-python],
              [disable the Python module @<:@automatic@:>@])],
            [with_python=no],
            [test x$have_python = xyes && with_python=yes])
if test x$with_python = xyes; then
    AC_SUBST(python_CFLAGS)
    AC_SUBST(python_LIBS)
    if test x$have_python = xno; then
        echo "Python module requires: python!"
        exit 1
    fi
fi
AM_CONDITIONAL([WITH_PYTHON], [test x$with_python = xyes])

echo
echo
echo "Building Engines:"
printf "\tJavaScriptCore\t\t${with_javascriptcore:-no}\n"
printf "\tSpiderMonkey\t\t${with_spidermonkey:-no}\n"
printf "\tV8\t\t\t${with_v8:-no}\n"

echo 
echo "Building Modules:"
printf "\tsystem\t\t\tyes\n"
printf "\tposix\t\t\tyes\n"

echo
echo "Building Bindings:"
printf "\tpython\t\t\t${with_python:-no}\n"
echo
echo

MODULEDIR=${libdir}/${PACKAGE_NAME}/${PACKAGE_VERSION}/modules
AC_SUBST(MODULEDIR)
AC_CONFIG_FILES(Makefile natus.pc natus/Makefile natus/engines/Makefile natus/tests/Makefile modules/Makefile bindings/Makefile bindings/python/Makefile)
AC_OUTPUT

