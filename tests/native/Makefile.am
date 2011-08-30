AM_DEFAULT_SOURCE_EXT = .cc
AUTOMAKE_OPTIONS = parallel-tests color-tests
LDADD       = $(abs_top_srcdir)/natus/libnatus.la $(abs_top_srcdir)/natus/libnatus-require.la
AM_LDFLAGS  = -rpath $(abs_top_builddir)/natus/.libs
AM_CXXFLAGS = -Wall -DMODSUFFIX='".so"' -DENGINEDIR='"$(abs_top_builddir)/natus/engines/.libs"' -I$(top_srcdir)/natus

TESTS = cxx_globals \
        cxx_array \
        cxx_boolean \
        cxx_function \
        cxx_null \
        cxx_number \
        cxx_object \
        cxx_private \
        cxx_string \
        cxx_recursive \
        cxx_require \
        cxx_exception \
        cxx_types \
        cxx_convargs
check_PROGRAMS = $(TESTS)
EXTRA_DIST     = test.hh scriptmod.js
