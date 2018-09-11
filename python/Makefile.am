EXTRA_DIST = README examples/set_iptc.py examples/show_iptc.py
EXTRA_DIST += MANIFEST.in setup.py

EXTRA_DIST += python2/pyiptcdata.c python2/pyiptcdatamod.c python2/pyiptcdataset.c python2/pyiptcdata.h
EXTRA_DIST += python3/pyiptcdata.c python3/pyiptcdatamod.c python3/pyiptcdataset.c python3/pyiptcdata.h

pyexec_LTLIBRARIES = iptcdata.la

iptcdata_la_SOURCES = pyiptcdatamod.c pyiptcdata.c pyiptcdataset.c pyiptcdata.h
iptcdata_la_LIBADD = ../libiptcdata/libiptcdata.la
iptcdata_la_LDFLAGS = -module -avoid-version
iptcdata_la_CPPFLAGS = $(PYTHON_CPPFLAGS) -I$(top_srcdir) -fno-strict-aliasing -DNDEBUG -g -O2 -Wall -Wstrict-prototypes
