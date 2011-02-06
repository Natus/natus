enginesdir=$(libdir)/$(PACKAGE_NAME)/$(PACKAGE_VERSION)/engines
AM_LDFLAGS = -module -avoid-version -no-undefined -shared
AM_LDADD = ../natus/libnatus.la

engines_LTLIBRARIES=
if WITH_SPIDERMONKEY
engines_LTLIBRARIES+=SpiderMonkey.la
endif
if WITH_V8
engines_LTLIBRARIES+=V8.la
endif
if WITH_JAVASCRIPTCORE
engines_LTLIBRARIES+=JavaScriptCore.la
endif

# SpiderMonkey (Mozilla) engine
SpiderMonkey_la_SOURCES=SpiderMonkey.cc
SpiderMonkey_la_CXXFLAGS = $(AM_CXXFLAGS) @SpiderMonkey_CFLAGS@ -Wall -I../../
SpiderMonkey_la_LDFLAGS  = $(AM_LDFLAGS)  @SpiderMonkey_LIBS@

# V8 engine
V8_la_SOURCES  = V8.cc
V8_la_CXXFLAGS = $(AM_CXXFLAGS) -Wall -I../../
V8_la_LDFLAGS  = $(AM_LDFLAGS) -lv8 -lpthread

# WebKit engine
JavaScriptCore_la_SOURCES  = JavaScriptCore.cc
JavaScriptCore_la_CXXFLAGS = $(AM_CXXFLAGS) @JavaScriptCore_CFLAGS@ -Wall -I../../
JavaScriptCore_la_LDFLAGS  = $(AM_LDFLAGS)  @JavaScriptCore_LIBS@
