INCLUDES =              				\
	-DLIBIPTCDATA_LOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(top_srcdir)/libiptcdata			\
	-I$(top_srcdir)/intl				\
	-I$(top_srcdir)

lib_LTLIBRARIES = libiptcdata.la

libiptcdata_la_LDFLAGS = -version-info @LIBIPTCDATA_VERSION_INFO@
libiptcdata_la_SOURCES =		\
	iptc-data.c		\
	iptc-dataset.c		\
	iptc-jpeg.c		\
	iptc-log.c		\
	iptc-mem.c		\
	iptc-tag.c		\
	iptc-utils.c

libiptcdataincludedir = $(includedir)/libiptcdata
libiptcdatainclude_HEADERS = 	\
	iptc-data.h		\
	iptc-dataset.h		\
	iptc-jpeg.h		\
	iptc-log.h		\
	iptc-mem.h		\
	iptc-tag.h		\
	iptc-utils.h		\
	_stdint.h		\
	i18n.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libiptcdata.pc
EXTRA_DIST = libiptcdata.pc.in

DISTCLEANFILES = _stdint.h

