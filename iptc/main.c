#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <config.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
#define getopt_long(a,b,c,d,e) getopt(a,b,c)
#endif

#include <locale.h>

#if defined(HAVE_ICONV_H) && defined(HAVE_WCHAR_H)
#include <iconv.h>
#include <wchar.h>
#include <langinfo.h>
#endif

#include "i18n.h"
#include <libiptcdata/iptc-data.h>
#include <libiptcdata/iptc-jpeg.h>

static char help_str[] = N_("\
Examples:\n\
  iptc image.jpg       # display the IPTC metadata contained in image.jpg\n\
  iptc -a Caption -v \"Foo\" image.jpg\n\
                       # add caption \"Foo\" to the IPTC data in image.jpg\n\
\n\
Operations:\n\
  -a, --add=TAG        add new tag with identifier TAG\n\
  -m, --modify=TAG     modify tag with identifier TAG\n\
  -v, --value=VALUE    value for added/modified tag\n\
  -d, --delete=TAG     delete tag with identifier TAG\n\
  -p, --print=TAG      print value of tag with identifier TAG\n\
\n\
Options:\n\
  -q, --quiet          produce less verbose output\n\
  -b, --backup         backup any modified files\n\
  -s, --sort           sort tags before displaying or saving\n\
\n\
Informative output:\n\
  -l, --list           list the names of all known tags (i.e. Caption, etc.)\n\
  -L, --list-desc=TAG  print the name and description of TAG\n\
      --help           print this help, then exit\n\
      --version        print iptc program version number, then exit\n\
");

unsigned char buf[256*256];
unsigned char outbuf[256*256];

static void
print_help(char ** argv)
{
	printf("%s\n\n%s: %s [%s]... [%s]\n\n%s",
			_("Utility for viewing and modifying the contents of \
IPTC metadata in images"),
			_("Usage"), argv[0], _("OPTION"), _("FILE"),
			_(help_str));
}

static void
print_version()
{
	printf("iptc %s\n%s\n", VERSION,
			_("Written by David Moore <dcm@acm.org>"));
}

#if defined(HAVE_ICONV_H) && defined(HAVE_WCHAR_H)

static char *
locale_to_utf8 (char * str)
{
	unsigned int in_len = strlen (str);
	int out_size = strlen (str) * 4 + 1;
	unsigned int out_left = out_size;
	char * a;
	char * outstr;
	iconv_t ic;

	ic = iconv_open ("UTF-8", nl_langinfo (CODESET));
	if (ic == (iconv_t) -1)
		return strdup (str);

	outstr = malloc (out_size);
	if (!outstr)
		return NULL;
	
	a = (char *) outstr;
	iconv (ic, &str, &in_len, &a, &out_left);
	outstr[out_size - out_left] = '\0';
	iconv_close (ic);

	return outstr;
}

static char *
str_to_locale (char * str, char * charset, int * len)
{
	unsigned int in_len = strlen (str);
	unsigned int w_size = (in_len+1) * 4;
	unsigned int w_left = w_size;
	unsigned int w_len;
	unsigned int out_len;
	wchar_t * wstr;
	char * a;
	char * outstr;
	int i, j;
	iconv_t ic;
	mbstate_t ps;

	ic = iconv_open ("WCHAR_T", charset);
	if (ic == (iconv_t) -1)
		return strdup (str);

	wstr = malloc (w_size);
	if (!wstr)
		return NULL;
	
	a = (char *) wstr;
	iconv (ic, &str, &in_len, &a, &w_left);
	w_len = (w_size - w_left) / 4;
	iconv_close (ic);

	out_len = 2 * w_len + MB_CUR_MAX;
	outstr = malloc (out_len);

	memset (&ps, '\0', sizeof (ps));

	if (len) {
		if (*len < w_len)
			w_len = *len;
		*len = w_len;
	}
	j = 0;
	for (i = 0; i < w_len; i++) {
		int n;
		while (out_len - j < MB_CUR_MAX + 1) {
			out_len *= 2;
			outstr = realloc (outstr, out_len);
			if (!outstr)
				return NULL;
		}
		n = wcrtomb (outstr + j, wstr[i], &ps);
		if (n == -1) {
			outstr[j] = '?';
			n = 1;
		}
		j += n;
	}
	outstr[j] = '\0';

	free (wstr);
	return outstr;
}

#else /* defined(HAVE_ICONV_H) && defined(HAVE_WCHAR_H) */

static char *
locale_to_utf8 (char * str)
{
	return strdup (str);
}

static char *
str_to_locale (char * str, char * charset, int * len)
{
	int in_len = strlen (str);

	if (len) {
		if (*len < in_len)
			in_len = *len;
		*len = in_len;
	}

	return strndup (str, in_len);
}

#endif

static int
print_tag_info (IptcRecord r, IptcTag t, int verbose)
{
	const char * name = iptc_tag_get_name (r, t);
	char * desc;
	if (!name)
		return -1;

	printf ("%2d:%03d %s\n", r, t, name);

	if (!verbose)
		return 0;

	desc = iptc_tag_get_description (r, t);
	if (desc) {
		char * convbuf;
		convbuf = str_to_locale (desc, "UTF-8", NULL);
		printf ("\n%s\n", convbuf);
		free (convbuf);
	}
	return 0;
}

static void
print_tag_list ()
{
	int r, t;

	printf("%6.6s %s\n", _("Tag"), _("Name"));
	printf(" ----- --------------------\n");

	for (r = 1; r <= 9; r++) {
		for (t = 0; t < 256; t++) {
			print_tag_info (r, t, 0);
		}
	}
}

static void
print_iptc_data (IptcData * d)
{
	int i;
	char * charset;

	if (d->count) {
		printf("%6.6s %-20.20s %-9.9s %6s  %s\n", _("Tag"), _("Name"),
				_("Type"), _("Size"), _("Value"));
		printf(" ----- -------------------- --------- ------  -----\n");
	}
	
	if (iptc_data_get_encoding (d) == IPTC_ENCODING_UTF8) {
		charset = "UTF-8";
	}
	else {
		/* technically this violates the IPTC IIM spec, but most
		 * other applications are broken. */
		charset = "ISO-8859-1";
	}

	for (i=0; i < d->count; i++) {
		IptcDataSet * e = d->datasets[i];
		unsigned char * buf;
		char * convbuf;
		int len;

		printf("%2d:%03d ", e->record, e->tag);
		len = 20;
		convbuf = str_to_locale (iptc_tag_get_title (e->record, e->tag),
				"UTF-8", &len);
		printf("%s%*s ", convbuf, 20 - len, "");
		free (convbuf);
		len = 9;
		convbuf = str_to_locale (iptc_format_get_name (iptc_dataset_get_format (e)),
				"UTF-8", &len);
		printf("%s%*s ", convbuf, 9 - len, "");
		free (convbuf);
		printf("%6d  ", e->size);

		switch (iptc_dataset_get_format (e)) {
			case IPTC_FORMAT_BYTE:
			case IPTC_FORMAT_SHORT:
			case IPTC_FORMAT_LONG:
				printf("%d\n", iptc_dataset_get_value (e));
				break;
			case IPTC_FORMAT_BINARY:
                                buf = malloc (3 * e->size + 1);
				iptc_dataset_get_as_str (e, (char *)buf, 3*e->size+1);
				printf("%s\n", buf);
                                free (buf);
				break;
			default:
                                buf = malloc (e->size + 1);
				iptc_dataset_get_data (e, buf, e->size+1);
				convbuf = str_to_locale ((char *)buf, charset, NULL);
                                free (buf);
				printf("%s\n", convbuf);
                                free (convbuf);
				break;
		}
	}
}

typedef enum {
	OP_ADD,
	OP_DELETE,
	OP_PRINT
} OpType;

typedef struct _Operation {
	OpType          op;
	IptcRecord      record;
	IptcTag         tag;
	int             num;
	IptcDataSet    *ds;
} Operation;

typedef struct _OpList {
	Operation      *ops;
	int             count;
} OpList;

static void
new_operation (OpList * list, OpType op, IptcRecord record,
		IptcTag tag, int num, IptcDataSet * ds)
{
	Operation * newop;
	if (!list)
		return;

	list->count++;
	list->ops = realloc (list->ops, list->count * sizeof(Operation));
	if (!list->ops)
		return;

	newop = list->ops + list->count - 1;
	newop->op = op;
	newop->record = record;
	newop->tag = tag;
	newop->num = num;
	newop->ds = ds;
}

static int
perform_operations (IptcData * d, OpList * list)
{
	int i, j;

	if (!d || !list)
		return 0;

	for (i = 0; i < list->count; i++) {
		Operation * op = list->ops + i;
		IptcDataSet * ds = NULL;

		if (op->record) {
			ds = iptc_data_get_dataset (d, op->record, op->tag);
			for (j = 0; j < op->num && ds; j++) {
				iptc_dataset_unref (ds);
				ds = iptc_data_get_next_dataset (d, ds,
						op->record, op->tag);
			}
			if (!ds) {
				fprintf(stderr, _("Could not find dataset %d:%d\n"), op->record, op->tag);
				return -1;
			}
		}
		
		if (op->op == OP_ADD) {
			if (ds)
				iptc_data_add_dataset_before (d, ds, op->ds);
			else
				iptc_data_add_dataset (d, op->ds);
			iptc_dataset_unref (op->ds);
		}
		else if (op->op == OP_DELETE) {
			if (!ds)
				return -1;
			iptc_data_remove_dataset (d, ds);
		}
		else if (op->op == OP_PRINT) {
			if (!ds)
				return -1;
			fwrite (ds->data, 1, ds->size, stdout);
		}

		if (ds)
			iptc_dataset_unref (ds);
	}
	if (list->ops)
		free (list->ops);
	list->count = 0;

	return 0;
}

static int
parse_tag_id (char * str, IptcRecord *r, IptcTag *t)
{
	if (isdigit (str[0])) {
		char * a;
		*r = strtoul (str, &a, 10);
		if (a[0] != ':' || !isdigit (a[1]))
			return -1;
		*t = strtoul (a + 1, NULL, 10);
		if (*r < 1 || *r > 9 || *t < 0 || *t > 255)
			return -1;
	}
	else {
		if (iptc_tag_find_by_name (str, r, t) < 0)
			return -1;
	}
	return 0;
}

int
main (int argc, char ** argv)
{
	FILE * infile, * outfile;
	IptcData * d = NULL;
	IptcDataSet * ds = NULL;
	int ps3_len, iptc_off;
        unsigned int iptc_len;
	IptcRecord record;
	IptcTag tag;
	const IptcTagInfo * tag_info;
	IptcFormat format;
	char c;
	int modified = 0;
	int added_string = 0;
	int is_quiet = 0;
	int do_backup = 0;
	int do_sort = 0;
	int add_tag = 0;
	int modify_tag = 0;
	OpList oplist = { 0, 0 };

#ifdef HAVE_GETOPT_H
	struct option longopts[] = {
		{ "quiet", no_argument, NULL, 'q' },
		{ "backup", no_argument, NULL, 'b' },
		{ "sort", no_argument, NULL, 's' },
		{ "list", no_argument, NULL, 'l' },
		{ "list-desc", required_argument, NULL, 'L' },
		{ "add", required_argument, NULL, 'a' },
		{ "modify", required_argument, NULL, 'm' },
		{ "delete", required_argument, NULL, 'd' },
		{ "print", required_argument, NULL, 'p' },
		{ "value", required_argument, NULL, 'v' },
		{ "help", no_argument, NULL, 'h' },
		{ "version", no_argument, NULL, 'V' },
		{ 0, 0, 0, 0 }
	};
#endif

	setlocale (LC_ALL, "");
	textdomain (IPTC_GETTEXT_PACKAGE);
	bindtextdomain (IPTC_GETTEXT_PACKAGE, IPTC_LOCALEDIR);

	while ((c = getopt_long (argc, argv, "qsblL:a:m:d:p:v:", longopts, NULL)) >= 0) {
		char * convbuf;
		switch (c) {
			case 'q':
				is_quiet = 1;
				break;
			case 'b':
				do_backup = 1;
				break;
			case 's':
				do_sort = 1;
				break;
			case 'l':
				print_tag_list ();
				return 0;
			case 'L':
				if (parse_tag_id (optarg, &record, &tag) < 0) {
					fprintf(stderr, _("\"%s\" is not a known tag\n"), optarg);
					return 1;
				}
				if (print_tag_info (record, tag, 1) < 0) {
					fprintf(stderr, _("No information about tag\n"));
				}
				return 0;
			case 'a':
			case 'm':
			case 'd':
			case 'p':
				if (add_tag || modify_tag) {
					fprintf(stderr, _("Must specify value for add/modify operation\n"));
					return 1;
				}
				if (parse_tag_id (optarg, &record, &tag) < 0) {
					fprintf(stderr, _("\"%s\" is not a known tag\n"), optarg);
					return 1;
				}
				if (c == 'a') {
					add_tag = 1;
					modified = 1;
				}
				else if (c == 'm') {
					modify_tag = 1;
					modified = 1;
				}
				else if (c == 'd') {
					new_operation (&oplist, OP_DELETE,
							record, tag, 0, NULL);
					modified = 1;
				}
				else if (c == 'p') {
					new_operation (&oplist, OP_PRINT,
							record, tag, 0, NULL);
					is_quiet = 1;
				}
					
				break;

			case 'v':
				if (!add_tag && !modify_tag) {
					fprintf(stderr, _("Must specify tag to add or modify\n"));
					return 1;
				}
				if (add_tag && modify_tag) {
					fprintf(stderr, _("Must specify value for add/modify operation\n"));
					return 1;
				}
				tag_info = iptc_tag_get_info (record, tag);
				if (!tag_info)
					format = IPTC_FORMAT_UNKNOWN;
				else
					format = tag_info->format;
				ds = iptc_dataset_new ();
				iptc_dataset_set_tag (ds, record, tag);
				switch (format) {
				case IPTC_FORMAT_BYTE:
				case IPTC_FORMAT_SHORT:
				case IPTC_FORMAT_LONG:
					if (!isdigit (*optarg)) {
						fprintf(stderr, _("Value must be an integer\n"));
						iptc_dataset_unref (ds);
						return 1;
					}
					iptc_dataset_set_value (ds,
							strtoul (optarg, NULL, 10),
							IPTC_DONT_VALIDATE);
					break;
				case IPTC_FORMAT_STRING:
					added_string = 1;
					convbuf = locale_to_utf8 (optarg);
					iptc_dataset_set_data (ds, (unsigned char *) convbuf,
							strlen (convbuf),
							IPTC_DONT_VALIDATE);
					free (convbuf);
					break;
				default:
					iptc_dataset_set_data (ds, (unsigned char *) optarg,
							strlen (optarg),
							IPTC_DONT_VALIDATE);
					break;
				}
				if (add_tag) {
					new_operation (&oplist, OP_ADD,
							0, 0, 0, ds);
					add_tag = 0;
				}
				if (modify_tag) {
					new_operation (&oplist, OP_ADD,
							record, tag, 0, ds);
					new_operation (&oplist, OP_DELETE,
							record, tag, 1, NULL);
					modify_tag = 0;
				}
				break;

			case 'h':
				print_help(argv);
				return 0;

			case 'V':
				print_version();
				return 0;

			default:
				print_help(argv);
				return 1;
		}
	}
	if (add_tag || modify_tag) {
		fprintf(stderr, _("Error: Must specify value for add/modify operation\n"));
		print_help (argv);
		return 1;
	}

	if (argc != optind + 1) {
		fprintf(stderr, _("Error: Must specify one file\n"));
		print_help (argv);
		return 1;
	}

	infile = fopen(argv[optind], "r");
	if (!infile) {
		fprintf(stderr, _("Error opening %s\n"), argv[1]);
		return 1;
	}

	ps3_len = iptc_jpeg_read_ps3 (infile, buf, sizeof(buf));
	fclose (infile);
	if (ps3_len < 0) {
		fprintf(stderr, _("Error reading file\n"));
		return 1;
	}

	if (ps3_len) {
		iptc_off = iptc_jpeg_ps3_find_iptc (buf, ps3_len, &iptc_len);
		if (iptc_off < 0) {
			fprintf(stderr, _("Error reading file\n"));
			return 1;
		}
		if (iptc_off)
			d = iptc_data_new_from_data (buf + iptc_off, iptc_len);
	}

	if (modified && !d)
		d = iptc_data_new ();

	if (perform_operations (d, &oplist) < 0)
		return 1;
	
	/* Make sure we specify the text encoding for the data */
	if (added_string) {
		IptcEncoding enc = iptc_data_get_encoding (d);
		if (enc == IPTC_ENCODING_UNSPECIFIED) {
			iptc_data_set_encoding_utf8 (d);
		}
		else if (enc != IPTC_ENCODING_UTF8) {
			fprintf (stderr, _("Warning: Strings encoded in UTF-8 "
				"have been added to the IPTC data, but\n"
				"pre-existing data may have been encoded "
				"with a different character set.\n"));
		}
	}

	if (do_sort)
		iptc_data_sort (d);

	if (!is_quiet) {
		if (d)
			print_iptc_data (d);
		else
			printf(_("No IPTC data found\n"));
	}


	if (modified) {
		unsigned char * iptc_buf = NULL;
		char tmpfile[strlen(argv[optind])+8];
		char bakfile[strlen(argv[optind])+8];
		int v;
		
		if (iptc_data_save (d, &iptc_buf, &iptc_len) < 0) {
			fprintf(stderr, _("Failed to generate IPTC bytestream\n"));
			return 1;
		}
		ps3_len = iptc_jpeg_ps3_save_iptc (buf, ps3_len,
				iptc_buf, iptc_len, outbuf, sizeof(outbuf));
		iptc_data_free_buf (d, iptc_buf);
		if (ps3_len < 0) {
			fprintf(stderr, _("Failed to generate PS3 header\n"));
			return 1;
		}

		infile = fopen (argv[optind], "r");
		if (!infile) {
			fprintf(stderr, _("Can't reopen input file\n"));
			return 1;
		}
		sprintf(tmpfile, "%s.%d", argv[optind], getpid());
		outfile = fopen (tmpfile, "w");
		if (!outfile) {
			fprintf(stderr, _("Can't open temporary file for writing\n"));
			return 1;
		}
		
		v = iptc_jpeg_save_with_ps3 (infile, outfile, outbuf, ps3_len);
		fclose (infile);
		fclose (outfile);

		if (v >= 0) {
			if (do_backup) {
				sprintf (bakfile, "%s~", argv[optind]);
				unlink (bakfile);
				if (link (argv[optind], bakfile) < 0) {
					fprintf (stderr, _("Failed to create backup file, aborting\n"));
					unlink (tmpfile);
					return 1;
				}
			}
			if (rename (tmpfile, argv[optind]) < 0) {
				fprintf(stderr, _("Failed to save image\n"));
				unlink (tmpfile);
				return 1;
			}
			fprintf(stderr, _("Image saved\n"));
		}
		else {
			unlink (tmpfile);
			fprintf(stderr, _("Failed to save image\n"));
		}
	}
	
	if (d)
		iptc_data_unref(d);

	return 0;
}
