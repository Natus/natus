moduledir = @MODULEDIR@
AM_LDFLAGS = -module -avoid-version -no-undefined -shared -L../natus -lnatus

module_LTLIBRARIES = system.la

system_la_SOURCES  = system.cc
system_la_CXXFLAGS = -Wall -I../
system_la_LDFLAGS  = $(AM_LDFLAGS)

install-data-hook:
	rm -f $(DESTDIR)$(moduledir)/*.a $(DESTDIR)$(moduledir)/*.la