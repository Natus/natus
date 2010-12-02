moduledir = @MODULEDIR@
AM_LDFLAGS = -module -avoid-version -no-undefined -shared -L../../natus -lnatus

pyexec_LTLIBRARIES = natus.la
module_LTLIBRARIES = python.la

natus_la_SOURCES  = pythonmod.cc
natus_la_CXXFLAGS = -Wall -I../../ @python_CFLAGS@
natus_la_LDFLAGS  = $(AM_LDFLAGS) @python_LIBS@

python_la_SOURCES  = natusmod.cc
python_la_CXXFLAGS = -Wall -I../../ @python_CFLAGS@
python_la_LDFLAGS  = $(AM_LDFLAGS) -lnatus @python_LIBS@

install-data-hook:
	rm -f $(DESTDIR)$(pyexecdir)/natus.a $(DESTDIR)$(pyexecdir)/natus.la
	rm -f $(DESTDIR)$(moduledir)/python.a $(DESTDIR)$(moduledir)/python.la