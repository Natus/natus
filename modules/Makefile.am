moduledir = @MODULEDIR@
AM_LDFLAGS = -module -avoid-version -no-undefined -shared -L../natus -lnatus

module_LTLIBRARIES = system.la posix.la socket.la

system_la_SOURCES  = system.cc
system_la_CXXFLAGS = -Wall -I../
system_la_LDFLAGS  = $(AM_LDFLAGS)

posix_la_SOURCES  = posix.cc
posix_la_CXXFLAGS = -Wall -I../
posix_la_LDFLAGS  = $(AM_LDFLAGS) -lutil

socket_la_SOURCES  = socket.cc
socket_la_CXXFLAGS = -Wall -I../
socket_la_LDFLAGS  = $(AM_LDFLAGS)

install-data-hook:
	rm -f $(DESTDIR)$(moduledir)/*.a $(DESTDIR)$(moduledir)/*.la