AUTOMAKE_OPTIONS  = parallel-tests color-tests
TESTS_ENVIRONMENT = ./runjstest 
TESTS             = stub.js
SUBDIRS           = native

noinst_PROGRAMS  = runjstest
runjstest_CFLAGS = -Wall -DMODSUFFIX='".so"' \
                   -DNATUSBIN='"$(abs_top_builddir)/natus/natus"' \
                   -DENGINEDIR='"$(abs_top_builddir)/natus/engines/.libs"'
