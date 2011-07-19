AUTOMAKE_OPTIONS  = parallel-tests color-tests
TESTS_ENVIRONMENT = ./runjstest 
TESTS             = stub.js
SUBDIRS           = native
EXTRA_DIST        = $(TESTS)

# Build runjstest
check_PROGRAMS    = runjstest
runjstest_SOURCES = test.h runjstest.c
runjstest_CFLAGS  = -Wall -DMODSUFFIX='".so"' \
                    -DNATUSBIN='"$(abs_top_builddir)/natus/natus"' \
                    -DENGINEDIR='"$(abs_top_builddir)/natus/engines/.libs"'
