/* iptc-dataset.c
 *
 * Copyright © 2005 David Moore <dcm@acm.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "iptc-dataset.h"
#include "iptc-utils.h"
#include "i18n.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#undef  MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

struct _IptcDataSetPrivate
{
	unsigned int ref_count;

	IptcMem *mem;
};

/**
 * iptc_dataset_new:
 *
 * Allocates a new dataset, which is initialized to be empty (undefined tag and
 * empty value).  The default memory allocation functions (malloc, etc.) are
 * used.  If you need custom memory management functions, use
 * iptc_dataset_new_mem() instead.  This allocation will set the #IptcDataSet
 * refcount to 1, so use iptc_dataset_unref() when finished with the pointer.
 *
 * Returns: pointer to the new #IptcDataSet object, NULL on error
 */
IptcDataSet *
iptc_dataset_new (void)
{
	IptcMem *mem = iptc_mem_new_default ();
	IptcDataSet *e = iptc_dataset_new_mem (mem);

	iptc_mem_unref (mem);

	return e;
}

/**
 * iptc_dataset_new_mem:
 * @mem: Pointer to an #IptcMem object that defines custom memory managment
 * functions.  The refcount of @mem will be incremented.  It is decremented
 * when the returned #IptcDataSet object is freed.
 *
 * Allocates a new dataset, which is initialized to be empty (undefined tag and
 * empty value), using custom memory management functions.  This allocation
 * will set the #IptcDataSet refcount to 1, so use iptc_dataset_unref() when
 * finished with the pointer.
 *
 * Returns: pointer to the new #IptcDataSet object, NULL on error
 */
IptcDataSet *
iptc_dataset_new_mem (IptcMem *mem)
{
	IptcDataSet *e = NULL;

	e = iptc_mem_alloc (mem, sizeof (IptcDataSet));
	if (!e) return NULL;
	e->priv = iptc_mem_alloc (mem, sizeof (IptcDataSetPrivate));
	if (!e->priv) { iptc_mem_free (mem, e); return NULL; }
	e->priv->ref_count = 1;

	e->priv->mem = mem;
	iptc_mem_ref (mem);

	return e;
}

/**
 * iptc_dataset_ref:
 * @dataset: the referenced pointer
 *
 * Increments the reference count of an #IptcDataSet object.  This function
 * should be called whenever a copy of a pointer is made by the application.
 * iptc_dataset_unref() can then be used when the pointer is no longer needed
 * to ensure that the object is freed once the object is completely unused.
 */
void
iptc_dataset_ref (IptcDataSet *e)
{
	if (!e) return;

	e->priv->ref_count++;
}

/**
 * iptc_dataset_unref:
 * @dataset: the unreferenced pointer
 *
 * Decrements the reference count of an #IptcDataSet object.  The object will
 * automatically be freed when the count reaches 0.  This function should
 * be called whenever a pointer is no longer needed by an application.  It
 * is also good practice to set the local copy of the pointer to NULL to
 * shield against accidently reusing the value.
 */
void
iptc_dataset_unref (IptcDataSet *e)
{
	if (!e) return;

	e->priv->ref_count--;
	if (!e->priv->ref_count)
		iptc_dataset_free (e);
}

/**
 * iptc_dataset_free:
 * @dataset: the object to free
 *
 * Frees an #IptcDataSet object and its contained data.  This function should
 * be used only for error handling since iptc_dataset_unref() provides a
 * safer mechanism for freeing that allows multiple components to have access
 * to an object.
 */
void
iptc_dataset_free (IptcDataSet *e)
{
	if (!e) return;

	if (e->priv) {
		IptcMem *mem = e->priv->mem;
		if (e->data)
			iptc_mem_free (mem, e->data);
		iptc_mem_free (mem, e->priv);
		iptc_mem_free (mem, e);
		iptc_mem_unref (mem);
	}
}

/**
 * iptc_dataset_set_tag:
 * @dataset: dataset for which the record/tag numbers should be set
 * @record: record number of the dataset
 * @tag: tag (dataset number) of the dataset
 *
 * Sets the record and tag number for a dataset object.  Each record/tag pair
 * is assigned a specific meaning by the IPTC IIM specification.  This function
 * allows any value to be set for the dataset object, regardless of its
 * meaning.  This function can also be used to change the record/tag number in
 * place even if it has been already set.
 */
void
iptc_dataset_set_tag (IptcDataSet *e, IptcRecord record, IptcTag tag)
{
	if (!e)
		return;

	e->record = record;
	e->tag    = tag;
	e->info   = iptc_tag_get_info (record, tag);
}

/**
 * iptc_dataset_set_data:
 * @dataset: dataset for which the value should be set
 * @buf: buffer containing the value
 * @size: number of bytes to copy
 * @validate: whether or not the passed value should be validated against
 * the IPTC specification for this dataset's tag.
 *
 * Copies bytes from a buffer as the new value for a dataset.  This is the
 * correct method for setting a dataset value to a string or raw binary
 * sequence.  The length of a string should be specified without the trailing
 * NULL.  New memory is allocated for the value and @size bytes are copied
 * from @buf into this new memory (which is freed when the dataset is freed or
 * when a new value is set).  If @validate is set to %IPTC_VALIDATE, the buffer
 * will only be copied if the size does not violate the IPTC specification (it
 * must not be too long or too short).  This check is ignored if the dataset's
 * tag is undefined or no information from the specification is available for
 * that tag.
 *
 * Returns: -1 on error, 0 if validation failed, the number of bytes copied
 * on success
 */
int
iptc_dataset_set_data (IptcDataSet *e, const char * buf, unsigned int size,
		IptcValidate validate)
{
	if (!e || !buf || !size)
		return -1;

	if (validate && e->info) {
		if (size > e->info->maxbytes || size < e->info->minbytes)
			return 0;
	}

	if (e->data)
		iptc_mem_free (e->priv->mem, e->data);

	e->size = 0;
	e->data = iptc_mem_alloc (e->priv->mem, size);
	if (!e->data)
		return -1;
	memcpy (e->data, buf, size);
	e->size = size;
	return size;
}

/**
 * iptc_dataset_set_value:
 * @dataset: dataset for which the value should be set
 * @value: value for the dataset
 * @validate: whether or not the passed value should be validated against
 * the IPTC specification for this dataset's tag.
 *
 * Sets the value of a dataset as an integer.  This is the correct method for
 * setting a dataset value with type byte, short, or long.  If @validate is
 * set to %IPTC_VALIDATE, the value will only be set if the tag has type
 * byte, short, or long according the IPTC specification.  This check is
 * ignored if the dataset's tag is undefined or no information from the
 * specification is available for that tag.
 *
 * Returns: -1 on error, 0 if validation failed, the number of bytes copied
 * on success
 */
int
iptc_dataset_set_value (IptcDataSet *e, unsigned int value,
		IptcValidate validate)
{
	IptcFormat format = IPTC_FORMAT_LONG;
	int size;

	if (!e)
		return -1;

	if (e->info)
		format = e->info->format;

	switch (format) {
		case IPTC_FORMAT_BYTE:
			size = 1;
			break;
		case IPTC_FORMAT_SHORT:
			size = 2;
			break;
		case IPTC_FORMAT_LONG:
			size = 4;
			break;
		default:
			size = 4;
			if (validate)
				return 0;
			break;
	}
	
	if (e->data)
		iptc_mem_free (e->priv->mem, e->data);

	e->size = 0;
	e->data = iptc_mem_alloc (e->priv->mem, size);
	if (!e->data)
		return -1;
	
	e->size = size;
	switch (format) {
		case IPTC_FORMAT_BYTE:
			e->data[0] = value;
			break;
		case IPTC_FORMAT_SHORT:
			iptc_set_short (e->data, IPTC_BYTE_ORDER_MOTOROLA, value);
			break;
		default:
			iptc_set_long (e->data, IPTC_BYTE_ORDER_MOTOROLA, value);
			break;
	}
	return size;
}

/**
 * iptc_dataset_get_format:
 * @dataset: pointer to an #IptcDataSet
 *
 * Retrieves the data format of a dataset tag according to the IPTC
 * specification.
 *
 * Returns: the format of the tag which will be %IPTC_FORMAT_UNKNOWN if the tag
 * number has not been set or the tag is not in the specification
 */
IptcFormat
iptc_dataset_get_format (IptcDataSet *e)
{
	if (!e || !e->info)
		return IPTC_FORMAT_UNKNOWN;

	return e->info->format;
}

/**
 * iptc_dataset_get_data:
 * @dataset: pointer to an #IptcDataSet
 * @buf: buffer to which the data will be copied
 * @size: maximum size of the buffer
 *
 * Copies the value of a dataset into a buffer.  If the buffer has extra
 * space, the data will be NULL-terminated to assist in dealing with
 * strings.
 *
 * Returns: the number of bytes copied, or -1 on failure
 */
int
iptc_dataset_get_data (IptcDataSet *e, char *val, unsigned int size)
{
	int s;
	if (!e || !val || !size || !e->data)
		return -1;
	s = MIN(e->size+1, size);
	memcpy (val, e->data, MIN(e->size, size));
	val[s-1] = '\0';

	return s;
}

/**
 * iptc_dataset_get_value:
 * @dataset: pointer to an #IptcDataSet
 *
 * Gets the value of a dataset as an unsigned integer.  This function is meant
 * to be used when the format of a tag is a byte, short, or long.  Undefined
 * results occur when this function is used on a tag containing data in another
 * format.
 *
 * Returns: value contained in the dataset
 */
unsigned int
iptc_dataset_get_value (IptcDataSet *e)
{
	if (!e)
		return 0;
	switch (e->size) {
		case 1:
			return e->data[0];
		case 2:
			return iptc_get_short (e->data, IPTC_BYTE_ORDER_MOTOROLA);
		case 3:
			return (iptc_get_short (e->data, IPTC_BYTE_ORDER_MOTOROLA) << 8)
				| e->data[2];
		default:
			return iptc_get_long (e->data, IPTC_BYTE_ORDER_MOTOROLA);
	}
}

/**
 * iptc_dataset_dump:
 * @dataset: dataset to print
 * @indent: indentation level of the printout
 *
 * A debugging aid that prints out the contents of a dataset to
 * standard output.
 */
void
iptc_dataset_dump (IptcDataSet *e, unsigned int indent)
{
	char buf[1024];
	char value[1024];
	unsigned int i;

	for (i = 0; i < 2 * indent; i++)
		buf[i] = ' ';
	buf[i] = '\0';

	if (!e)
		return;

	printf ("%sTag: %d:%d ('%s')\n", buf, e->record, e->tag,
		iptc_tag_get_name (e->record, e->tag));
/*	printf ("%s  Format: %i ('%s')\n", buf, e->format,
		iptc_format_get_name (e->format)); */
	printf ("%s  Size: %i\n", buf, e->size);
	printf ("%s  Value: %s\n", buf, iptc_dataset_get_as_str (e, value, sizeof(value)));
}

#define TAG_ID(r,t)	(((r) << 8) | (t))

/**
 * iptc_dataset_get_as_str:
 * @dataset: pointer to an #IptcDataSet
 * @buf: buffer to which the string will be copied
 * @size: maximum size of the buffer
 *
 * Copies the value of a dataset into a buffer as a printable, NULL-terminated
 * string.  For tags of type byte, short, or long, the value will be formatted
 * as a string.  For tags of type string (including dates and times), the
 * string will simply be copied into the output buffer.  For tags containing
 * binary data, the data will be formatted as a string in hexidecimal.
 *
 * Returns: the output string on success, or NULL on failure
 */
const char *
iptc_dataset_get_as_str (IptcDataSet *e, char *val, unsigned int maxlen)
{
	unsigned int i;
	IptcByte v_byte;
	IptcShort v_short;
	IptcLong v_long;
	IptcFormat format = IPTC_FORMAT_BINARY;

	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	bindtextdomain (GETTEXT_PACKAGE, LIBIPTCDATA_LOCALEDIR);

	if (!e || !val || !maxlen)
		return NULL;

	if (e->info)
		format = e->info->format;

	switch (TAG_ID(e->record, e->tag)) {
	default:
		switch (format) {
		case IPTC_FORMAT_BYTE:
			v_byte = e->data[0];
			snprintf (val, maxlen, "%hhu", v_byte);
			break;
		case IPTC_FORMAT_SHORT:
			v_short = iptc_get_short (e->data, IPTC_BYTE_ORDER_MOTOROLA);
			snprintf (val, maxlen, "%hu", v_short);
			break;
		case IPTC_FORMAT_LONG:
			v_long = iptc_get_long (e->data, IPTC_BYTE_ORDER_MOTOROLA);
			snprintf (val, maxlen, "%u", v_long);
			break;
		case IPTC_FORMAT_STRING:
		case IPTC_FORMAT_NUMERIC_STRING:
		case IPTC_FORMAT_DATE:
		case IPTC_FORMAT_TIME:
			strncpy (val, e->data, MIN (maxlen, e->size));
			if (maxlen > e->size)
				val[e->size] = '\0';
			break;
		default:
			for (i=0; i < MIN (maxlen/3, e->size); i++) {
				if (i > 0)
					val[i*3-1] = ' ';
				sprintf(val+i*3, "%02x", e->data[i]);
			}
			break;
		}
	}

	return val;
}

