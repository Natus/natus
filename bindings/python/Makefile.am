moduledir = @MODULEDIR@
AM_LDFLAGS = -module -avoid-version -no-undefined -shared
AM_LDADD = ../../libnatus.la

if WITH_PYTHON
pyexec_LTLIBRARIES = natus.la
module_LTLIBRARIES = python.la
endif

natus_la_SOURCES  = pythonmod.cc common.cc
natus_la_CXXFLAGS = -Wall -I../../ @python_CFLAGS@
natus_la_LDFLAGS  = $(AM_LDFLAGS) @python_LIBS@

python_la_SOURCES  = natusmod.cc common.cc
python_la_CXXFLAGS = -Wall -I../../ @python_CFLAGS@
python_la_LDFLAGS  = $(AM_LDFLAGS) @python_LIBS@

install-data-hook:
	rm -f $(DESTDIR)$(pyexecdir)/natus.a $(DESTDIR)$(pyexecdir)/natus.la
	rm -f $(DESTDIR)$(moduledir)/python.a $(DESTDIR)$(moduledir)/python.la
