AM_DEFAULT_SOURCE_EXT = .cc
AUTOMAKE_OPTIONS = parallel-tests color-tests
LDADD       = ../libnatus.la
AM_CXXFLAGS = -Wall -DMODSUFFIX='".so"' -I$(srcdir)/../../

TESTS            = cxx_globals \
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
                   cxx_require
noinst_PROGRAMS  = $(TESTS)
