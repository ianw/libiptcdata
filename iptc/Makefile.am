INCLUDES =						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/libiptcdata			\
	-I$(top_srcdir)/intl				\
	-DIPTC_LOCALEDIR=\""$(datadir)/locale"\"

bin_PROGRAMS = iptc

iptc_SOURCES =			\
	main.c
iptc_LDADD =					\
	-L../libiptcdata -liptcdata		\
	$(INTLLIBS)
