moduledir = @MODULEDIR@
AM_LDFLAGS = -module -avoid-version -no-undefined -shared

module_LTLIBRARIES = posix.la socket.la system.la

posix_la_SOURCES  = posix.cc
posix_la_CXXFLAGS = -Wall -I../
posix_la_LDFLAGS  = $(AM_LDFLAGS) -lutil
posix_la_LIBADD   = ../natus/libnatus.la

socket_la_SOURCES  = socket.cc
socket_la_CXXFLAGS = -Wall -I../
socket_la_LDFLAGS  = $(AM_LDFLAGS)
socket_la_LIBADD   = ../natus/libnatus.la

system_la_SOURCES  = system.cc
system_la_CXXFLAGS = -Wall -I../
system_la_LDFLAGS  = $(AM_LDFLAGS)
system_la_LIBADD   = ../natus/libnatus.la
