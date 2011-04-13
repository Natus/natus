AUTOMAKE_OPTIONS  = parallel-tests color-tests
TESTS_ENVIRONMENT = ./runjstest 
TESTS             = stub.js
SUBDIRS           = native

noinst_PROGRAMS  = runjstest
runjstest_CFLAGS = -Wall -DMODSUFFIX='".so"' -DENGINEDIR='"$(srcdir)/../natus/engines/.libs"' -DMODULEDIR='"$(srcdir)/../modules/.libs"' 
