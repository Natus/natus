moduledir = @MODULEDIR@
enginedir = $(libdir)/$(PACKAGE_NAME)/engines
natincdir = $(includedir)/$(PACKAGE_NAME)

noinst_PROGRAMS = tests
bin_PROGRAMS    = natus
lib_LTLIBRARIES = libnatus.la
natinc_HEADERS  = natus.h natusc.h engine.h

libnatus_la_SOURCES = natus.cc natusc.cc natus.h natusc.h engine.h 
libnatus_la_CXXFLAGS = $(AM_CXXFLAGS) -Wall -DMODULEDIR=$(moduledir) -DENGINEDIR=$(enginedir)
libnatus_la_LDFLAGS = $(AM_LDFLAGS) -ldl -lpthread

natus_SOURCES  = main.cc natus.h
natus_CXXFLAGS = $(AM_CXXFLAGS) -Wall -DMODULEDIR=$(moduledir)
natus_LDFLAGS  = $(AM_LDFLAGS) -lreadline
natus_LDADD = libnatus.la

tests_SOURCES  = tests.cc natus.h
tests_CXXFLAGS = $(AM_CXXFLAGS) -Wall -DMODULEDIR=$(moduledir)
tests_LDFLAGS  = $(AM_LDFLAGS)
tests_LDADD = libnatus.la

SUBDIRS = . engines

install-data-hook:
	rm -f $(DESTDIR)$(libdir)/libnatus*.a $(DESTDIR)$(libdir)/libnatus*.la
