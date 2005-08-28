#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <getopt.h>
#include <locale.h>

#include <config.h>

#include "i18n.h"
#include <libiptcdata/iptc-data.h>
#include <libiptcdata/iptc-jpeg.h>

static char help_str[] = "\
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
  -L, --list-desc      list the names and descriptions of all known tags\n\
      --help           print this help, then exit\n\
      --version        print iptc program version number, then exit\n\
";

unsigned char buf[256*256];
unsigned char outbuf[256*256];

static void
print_help(char ** argv)
{
	printf("%s\n\nUsage: %s [OPTION]... [FILE]\n\n%s",
			"Utility for viewing and modifying the contents of \
IPTC metadata in images",
			argv[0], help_str);
}

static void
print_version()
{
	printf("iptc %s\n%s\n", VERSION,
			"Written by David Moore <dcm@acm.org>");
}

static void
print_text_block (int indent, const char * text)
{
	int cols = 80;
	int size = cols - indent;
	char str[size];
	const char * a = text;

	while (*a != '\0') {
		int len;
		strncpy (str, a, size);
		str[size-1] = '\0';
		len = strlen (str);
		if (len == size - 1) {
			char * b = strrchr (str, ' ');
			if (b)
				*b = '\0';
			len = b - str + 1;
		}
		a += len;
		printf ("%*s%s\n", indent, "", str);
	}
}

static void
print_tag_list (int verbose)
{
	int r, t;

	printf("%6.6s %s\n", "Tag", "Name");
	printf(" ----- --------------------\n");

	for (r = 1; r <= 9; r++) {
		for (t = 0; t < 256; t++) {
			const char * name = iptc_tag_get_name (r, t);
			const char * desc;
			if (!name)
				continue;

			printf ("%2d:%03d %s\n", r, t, name);

			if (!verbose)
				continue;

			desc = iptc_tag_get_description (r, t);
			if (desc)
				print_text_block (10, desc);
		}
	}
}

static void
print_iptc_data (IptcData * d)
{
	int i;

	if (d->count) {
		printf("%6.6s %-20.20s %-9.9s %6s  %s\n", "Tag", "Name", "Type",
				"Size", "Value");
		printf(" ----- -------------------- --------- ------  -----\n");
	}
	
	for (i=0; i < d->count; i++) {
		IptcDataSet * e = d->datasets[i];
		unsigned char * buf;

		printf("%2d:%03d %-20.20s %-9.9s %6d  ",
				e->record, e->tag,
				iptc_tag_get_title (e->record, e->tag),
				iptc_format_get_name (iptc_dataset_get_format (e)),
				e->size);
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
				printf("%s\n", buf);
                                free (buf);
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
				fprintf(stderr, "Could not find dataset %d:%d\n", op->record, op->tag);
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
			char buf[256];
			if (!ds)
				return -1;
			iptc_dataset_get_as_str (ds, buf, sizeof(buf));
			printf("%s\n", buf);
		}

		if (ds)
			iptc_dataset_unref (ds);
	}
	if (list->ops)
		free (list->ops);
	list->count = 0;

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

	struct option longopts[] = {
		{ "quiet", no_argument, NULL, 'q' },
		{ "backup", no_argument, NULL, 'b' },
		{ "sort", no_argument, NULL, 's' },
		{ "list", no_argument, NULL, 'l' },
		{ "list-desc", no_argument, NULL, 'L' },
		{ "add", required_argument, NULL, 'a' },
		{ "modify", required_argument, NULL, 'm' },
		{ "delete", required_argument, NULL, 'd' },
		{ "print", required_argument, NULL, 'p' },
		{ "value", required_argument, NULL, 'v' },
		{ "help", no_argument, NULL, 'h' },
		{ "version", no_argument, NULL, 'V' },
		{ 0, 0, 0, 0 }
	};

	setlocale (LC_ALL, "");
	textdomain (IPTC_GETTEXT_PACKAGE);
	bindtextdomain (IPTC_GETTEXT_PACKAGE, IPTC_LOCALEDIR);

	while ((c = getopt_long (argc, argv, "qsblLa:m:d:p:v:", longopts, NULL)) >= 0) {
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
				print_tag_list (0);
				return 0;
			case 'L':
				print_tag_list (1);
				return 0;
			case 'a':
			case 'm':
			case 'd':
			case 'p':
				if (add_tag || modify_tag) {
					fprintf(stderr, "Must specify value for add/modify operation\n");
					return 1;
				}
				if (isdigit (*optarg)) {
					char * a;
					record = strtoul (optarg, &a, 10);
					if (a[0] != ':' || !isdigit (a[1]))
						goto invalid_tag;
					tag = strtoul (a + 1, NULL, 10);
					if (record < 1 || record > 9 ||
							tag < 0 || tag > 255)
						goto invalid_tag;
				}
				else {
					if (iptc_tag_find_by_name (optarg,
								&record, &tag) < 0)
						goto invalid_tag;
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
invalid_tag:
				fprintf(stderr, "\"%s\" is not a known tag\n", optarg);
				return 1;

			case 'v':
				if (!add_tag && !modify_tag) {
					fprintf(stderr, "Must specify tag to add or modify\n");
					return 1;
				}
				if (add_tag && modify_tag) {
					fprintf(stderr, "Must specify value for add/modify operation\n");
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
						fprintf(stderr, "Value must be an integer\n");
						iptc_dataset_unref (ds);
						return 1;
					}
					iptc_dataset_set_value (ds,
							strtoul (optarg, NULL, 10),
							IPTC_DONT_VALIDATE);
					break;
				case IPTC_FORMAT_STRING:
					added_string = 1;
					iptc_dataset_set_data (ds, (unsigned char *) optarg,
							strlen (optarg),
							IPTC_DONT_VALIDATE);
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
				fprintf(stderr, "Try '%s --help' for more information.\n",
						argv[0]);
				return 1;
		}
	}
	if (add_tag || modify_tag) {
		fprintf(stderr, "Must specify value for add/modify operation\n");
		fprintf(stderr, "Try '%s --help' for more information.\n",
				argv[0]);
		return 1;
	}

	if (argc != optind + 1) {
		fprintf(stderr, "Must specify one file\n");
		fprintf(stderr, "Try '%s --help' for more information.\n",
				argv[0]);
		return 1;
	}

	infile = fopen(argv[optind], "r");
	if (!infile) {
		fprintf(stderr, "Error opening %s\n", argv[1]);
		return 1;
	}

	ps3_len = iptc_jpeg_read_ps3 (infile, buf, sizeof(buf));
	fclose (infile);
	if (ps3_len < 0) {
		fprintf(stderr, "Error reading file\n");
		return 1;
	}

	if (ps3_len) {
		iptc_off = iptc_jpeg_ps3_find_iptc (buf, ps3_len, &iptc_len);
		if (iptc_off < 0) {
			fprintf(stderr, "Error reading file\n");
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
			fprintf (stderr, "Warning: Strings encoded in UTF-8 "
				"have been added to the IPTC data, but\n"
				"pre-existing data may have been encoded "
				"with a different character set.\n");
		}
	}

	if (do_sort)
		iptc_data_sort (d);

	if (!is_quiet) {
		if (d)
			print_iptc_data (d);
		else
			printf("No IPTC data found\n");
	}


	if (modified) {
		unsigned char * iptc_buf = NULL;
		char tmpfile[strlen(argv[optind])+8];
		char bakfile[strlen(argv[optind])+8];
		int v;
		
		if (iptc_data_save (d, &iptc_buf, &iptc_len) < 0) {
			fprintf(stderr, "Failed to generate IPTC bytestream\n");
			return 1;
		}
		ps3_len = iptc_jpeg_ps3_save_iptc (buf, ps3_len,
				iptc_buf, iptc_len, outbuf, sizeof(outbuf));
		iptc_data_free_buf (d, iptc_buf);
		if (ps3_len < 0) {
			fprintf(stderr, "Failed to generate PS3 header\n");
			return 1;
		}

		infile = fopen (argv[optind], "r");
		if (!infile) {
			fprintf(stderr, "Can't reopen input file\n");
			return 1;
		}
		sprintf(tmpfile, "%s.%d", argv[optind], getpid());
		outfile = fopen (tmpfile, "w");
		if (!outfile) {
			fprintf(stderr, "Can't open temporary file for writing\n");
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
					fprintf (stderr, "Failed to create backup file, aborting\n");
					unlink (tmpfile);
					return 1;
				}
			}
			if (rename (tmpfile, argv[optind]) < 0) {
				fprintf(stderr, "Failed to save image\n");
				unlink (tmpfile);
				return 1;
			}
			fprintf(stderr, "Image saved\n");
		}
		else {
			unlink (tmpfile);
			fprintf(stderr, "Failed to save image\n");
		}
	}
	
	if (d)
		iptc_data_unref(d);

	return 0;
}
