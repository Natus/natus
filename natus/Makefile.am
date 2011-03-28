# Define our directories
moduledir = @MODULEDIR@
enginedir = $(libdir)/$(PACKAGE_NAME)/$(PACKAGE_VERSION)/engines
natincdir = $(includedir)/$(PACKAGE_NAME)

# Variables
CHEADERS   = engine.h   misc.h   value.h   private.h natus.h   types.h   backend.h 
CSOURCES   = engine.c   misc.c   value.c   private.c
CXXHEADERS = engine.hpp misc.hpp value.hpp           natus.hpp types.hpp
CXXSOURCES = engine.cc  misc.cc  value.cc

# Install the headers
natinc_HEADERS = $(CHEADERS) $(CXXHEADERS)

# Build our internal C library
noinst_LTLIBRARIES   = libnatusc.la
libnatusc_la_SOURCES = $(CSOURCES) $(CHEADERS)
libnatusc_la_CFLAGS  = $(AM_CFLAGS) -Wall -DMODULEDIR=$(moduledir) -DENGINEDIR=$(enginedir) -DMODSUFFIX='".so"'

# Build/install the shared C/C++ library
lib_LTLIBRARIES      = libnatus.la
libnatus_la_SOURCES  = $(CXXSOURCES) $(CXXHEADERS)
libnatus_la_CXXFLAGS = $(AM_CFLAGS) -Wall 
libnatus_la_LIBADD   = libnatusc.la
libnatus_la_LDFLAGS  = $(AM_LDFLAGS) -ldl -lpthread

# Build/install the shell
#bin_PROGRAMS         = natus
#natus_SOURCES        = main.cc
#natus_CXXFLAGS       = $(AM_CXXFLAGS) -Wall -DMODULEDIR=$(moduledir) -I../
#natus_LDFLAGS        = $(AM_LDFLAGS) -lreadline
#natus_LDADD          = libnatus.la

SUBDIRS = . engines tests
