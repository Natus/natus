AUTOMAKE_OPTIONS  = parallel-tests color-tests
TESTS_ENVIRONMENT = NATUS_PATH=$(abs_top_builddir)/modules/.libs:$(abs_top_srcdir)/modules ./runjstest 
TESTS             = stub.js mod_socket.js
SUBDIRS           = native

noinst_PROGRAMS  = runjstest
runjstest_CFLAGS = -Wall -DMODSUFFIX='".so"' -DNATUSBIN='"$(abs_top_srcdir)/natus/natus"' -DENGINEDIR='"$(abs_top_srcdir)/natus/engines/.libs"' 
