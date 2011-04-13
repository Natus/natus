AM_DEFAULT_SOURCE_EXT = .cc
AUTOMAKE_OPTIONS = parallel-tests color-tests
LDADD       = $(abs_top_srcdir)/natus/libnatus.la
AM_CXXFLAGS = -Wall -DMODSUFFIX='".so"' -DENGINEDIR='"$(abs_top_srcdir)/natus/engines/.libs"' -I$(abs_top_srcdir)

TESTS = cxx_globals \
        cxx_array \
        cxx_boolean \
        cxx_function \
        cxx_null \
        cxx_number \
        cxx_object \
        cxx_private \
        cxx_string \
        cxx_json \
        cxx_recursive \
        cxx_require \
        cxx_exception
noinst_PROGRAMS  = $(TESTS)
