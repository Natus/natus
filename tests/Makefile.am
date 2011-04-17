AUTOMAKE_OPTIONS  = parallel-tests color-tests
TESTS_ENVIRONMENT = ./runjstest 
TESTS             = stub.js mod_socket.js
SUBDIRS           = native

noinst_PROGRAMS  = runjstest
runjstest_CFLAGS = -Wall -DMODSUFFIX='".so"' \
                   -DNATUSBIN='"$(abs_top_builddir)/natus/natus"' \
                   -DENGINEDIR='"$(abs_top_builddir)/natus/engines/.libs"' \
                   -DMODULEDIR='"$(abs_top_builddir)/modules/.libs:$(abs_top_srcdir)/modules"'
