enginesdir=$(libdir)/$(PACKAGE_NAME)/$(PACKAGE_VERSION)/engines
AM_LDFLAGS = -module -avoid-version -no-undefined -shared
AM_CFLAGS = -g -Wall -I$(top_srcdir)/natus

engines_LTLIBRARIES=

# SpiderMonkey (Mozilla) engine
if WITH_SPIDERMONKEY
engines_LTLIBRARIES += SpiderMonkey.la
SpiderMonkey_la_SOURCES  = SpiderMonkey.c
SpiderMonkey_la_CFLAGS = $(AM_CFLAGS) @SpiderMonkey_CFLAGS@
SpiderMonkey_la_LDFLAGS  = $(AM_LDFLAGS)  @SpiderMonkey_LIBS@
SpiderMonkey_la_LIBADD   = ../libnatus.la
endif

# v8 engine
if WITH_V8
engines_LTLIBRARIES += v8.la
v8_la_SOURCES  = v8.cc
v8_la_CXXFLAGS = $(AM_CXXFLAGS) -fno-exceptions -fno-rtti
v8_la_LDFLAGS  = $(AM_LDFLAGS)  -fno-exceptions -fno-rtti -lv8 -lpthread
v8_la_LIBADD   = ../libnatus.la
endif

# WebKit engine
if WITH_JAVASCRIPTCORE
engines_LTLIBRARIES += JavaScriptCore.la
JavaScriptCore_la_SOURCES = JavaScriptCore.c
JavaScriptCore_la_CFLAGS  = $(AM_CFLAGS) @JavaScriptCore_CFLAGS@
JavaScriptCore_la_LDFLAGS = $(AM_LDFLAGS)  @JavaScriptCore_LIBS@
JavaScriptCore_la_LIBADD  = ../libnatus.la
endif



