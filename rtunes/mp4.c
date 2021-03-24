/* $Id: mp4.c,v 1.9 2009/06/27 10:10:32 mbalmer Exp $ */

/*
 * Copyright (c) 2006 Marcus Glocker <marcus@nazgul.ch>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "extern.h"
#include "proto.h"

/*
 * mp4_check()
 *	check what audio codec a file has
 * Return:
 *	1 = codec ok, 0 = unknown codec, -1 = error
 */
int
mp4_check(int fd)
{
	struct	stsd	*c;

	if (mp4_seek_box(fd, "stsd") != 1)
		return (-1);
	else if ((c = mp4_parse_stsd(fd)) == NULL)
		return (-1);
	
	/* Apple Lossless */
	if (!strcmp("alac", c->codec))
		return (1);

	free(c);

	return (0);
}

/*
 * mp4_seek_box()
 *	seek to a box
 * Return:
 *	1 = box found, 0 = box not found
 */
int
mp4_seek_box(int fd, const char *type)
{
	int		 r, box, size, level = 6;
	char		*boxes[6];
	unsigned char	buf[4];

	boxes[0] = strdup("moov");
	boxes[1] = strdup("trak");
	boxes[2] = strdup("mdia");
	boxes[3] = strdup("minf");
	boxes[4] = strdup("stbl");
	boxes[5] = strdup(type);

	/* reset file position */
	lseek(fd, 0, SEEK_SET);

	/* seek box */
	for (box = 0; box < level;) {
		/* get box size */
		r = read(fd, buf, 4);
		if (r < 1)
			break;
		size = mp4_revert(buf);

		/* get box name */
		read(fd, buf, 4);

		/* jump to next lower box level */
		if (!strncmp(boxes[box], (char *)buf, 4)) {
			box++;
			continue;
		}

		/* forward to next box */
		lseek(fd, size - 8, SEEK_CUR);
	}

	/* check if we found the target box */
	if (box != level)
		return (0);

	/* skip version */
	lseek(fd, 4, SEEK_CUR);

	return (1);
}

/*
 * mp4_parse_stco()
 *	parse STCO box values
 * Return:
 * 	<pointer to structure> = success, NULL = error
 */
struct stco *
mp4_parse_stco(int fd)
{
	int		 i;
	struct stco	*b;
	unsigned char	 buf[4];

	if ((b = malloc(sizeof(struct stco))) == NULL)
		return (NULL);

	memset(b, 0, sizeof(struct stco));

	/* get total number of chunks */
	read(fd, buf, 4);
	b->entry_count = mp4_revert(buf);

	/* get file-offsets for all chunks */
	for (i = 1; i < b->entry_count; i++) {
		read(fd, buf, 4);
		b->chunk_offset[i] = mp4_revert(buf);
	}

	return (b);
}

/*
 * mp4_parse_stsz()
 *	parse STSZ box values
 * Return
 *	<pointer to structure> = success, NULL = error
 */
struct stsz *
mp4_parse_stsz(int fd)
{
	int		 i;
	struct stsz	*b;
	unsigned char	 buf[4];

	if ((b = malloc(sizeof(struct stsz))) == NULL)
		return (NULL);

	memset(b, 0, sizeof(struct stsz));

	/* get sample size */
	read(fd, buf, 4);
	b->sample_size = mp4_revert(buf);

	/* get sample count */
	read(fd, buf, 4);
	b->sample_count = mp4_revert(buf);

	/* get samples sizes if they differ */
	if (b->sample_size == 0) {
		for (i = 1; i < b->sample_count; i++) {
			read(fd, buf, 4);
			b->entry_size[i] = mp4_revert(buf);
		}
	}

	return (b);
}

/*
 * mp4_parse_stsc()
 *	parse STSC box values
 * Return:
 *	<pointer to structure> = success, NULL = error
 */
struct stsc *
mp4_parse_stsc(int fd)
{
	int		 i;
	struct stsc	*b;
	unsigned char	 buf[4];

	if ((b = malloc(sizeof(struct stsc))) == NULL)
		return (NULL);

	memset(b, 0, sizeof(struct stsc));

	/* get table count */
	read(fd, buf, 4);
	b->entry_count = mp4_revert(buf);

	/* get samples per chunk info */
	for (i = 1; i < b->entry_count; i++) {
		read(fd, buf, 4);
		b->first_chunk[i] = mp4_revert(buf);
		read(fd, buf, 4);
		b->samples_per_chunk[i] = mp4_revert(buf);
		read(fd, buf, 4);
		b->sample_description_index[i] = mp4_revert(buf);
	}

	return (b);
}

/*
 * mp4_parse_stsd()
 *	parse STSD box values
 * Return:
 *	<pointer to structure> = success, NULL = error
 */
struct stsd *
mp4_parse_stsd(int fd)
{
	struct stsd	*b;
	unsigned char	 buf[4];

	if ((b = malloc(sizeof(struct stsd))) == NULL)
		return (NULL);

	memset(b, 0, sizeof(struct stsd));

	/* seek to codec */
	lseek(fd, 8, SEEK_CUR);

	/* get codec */
	read(fd, buf, 4);
	memset(b->codec, 0, sizeof(b->codec));
	memcpy(b->codec, buf, 4);

	return (b);
}

/*
 * mp4_revert()
 *	change byte order to match our cpu type.  mp4 is big endian order
 * Return:
 *	<integer> = success
 */
int
mp4_revert(unsigned char *buf)
{
	int		i;
	unsigned char	rev[4];

#if BYTE_ORDER == LITTLE_ENDIAN
	rev[0] = buf[3];
	rev[1] = buf[2];
	rev[2] = buf[1];
	rev[3] = buf[0];
#else
	rev[0] = buf[0];
	rev[1] = buf[1];
	rev[2] = buf[2];
	rev[3] = buf[3];
#endif

	memcpy(&i, rev, 4);

	return (i);
}
