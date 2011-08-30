# Define our directories
moduledir = @MODULEDIR@
enginedir = $(libdir)/$(PACKAGE_NAME)/$(PACKAGE_VERSION)/engines
natincdir = $(includedir)/$(PACKAGE_NAME)

# Defaults
AM_CFLAGS   = -Wall -I$(top_srcdir) -DMODSUFFIX='".so"'
AM_CXXFLAGS = -Wall -I$(top_srcdir) -DMODSUFFIX='".so"'
AM_LDFLAGS  = -export-symbols-regex '^(_ZNK?5natus|natus_)' -ldl -lpthread

# Variables
CHEADERS   = natus.h  natus-engine.h 
CXXHEADERS = natus.hh
CSOURCES   = misc.c   value.c  private.c
CXXSOURCES = misc.cc  value.cc

# Handle the headers
natinc_HEADERS = $(CHEADERS) $(CXXHEADERS)
noinst_HEADERS = natus-internal.h

# Build our internal C libraries
noinst_LTLIBRARIES   = libnatusc.la libnatusc-require.la

libnatusc_la_SOURCES = $(CSOURCES) $(CHEADERS)
libnatusc_la_CFLAGS  = $(AM_CFLAGS) -DMODULEDIR=$(moduledir) -DENGINEDIR=$(enginedir)

libnatusc_require_la_SOURCES = require.c natus-require.h
libnatusc_require_la_CFLAGS  = $(AM_CFLAGS)

# Build/install the shared C/C++ libraries
lib_LTLIBRARIES              = libnatus.la libnatus-require.la

libnatus_la_SOURCES          = $(CXXSOURCES) $(CXXHEADERS)
libnatus_la_CXXFLAGS         = $(AM_CXXFLAGS)
libnatus_la_LIBADD           = libnatusc.la

libnatus_require_la_SOURCES  = require.cc natus-require.hh
libnatus_require_la_CXXFLAGS = $(AM_CXXFLAGS)
libnatus_require_la_LIBADD   = libnatus.la libnatusc-require.la

# Build/install the shell
bin_PROGRAMS         = natus
natus_SOURCES        = main.cc
natus_CXXFLAGS       = $(AM_CXXFLAGS) -Wall -DMODULEDIR=$(moduledir) -I../
natus_LDFLAGS        = $(AM_LDFLAGS) -lreadline
natus_LDADD          = libnatus-require.la

SUBDIRS = . engines
