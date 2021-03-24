/* $Id: rtunes.c,v 1.4 2006/05/14 19:55:07 hacki Exp $ */

/*
 * Copyright (c) 2006 Marcus Glocker <marcus@nazgul.ch>
 * Copyright (c) 2006 Marc Balmer <mbalmer@openbsd.org>
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

#include <err.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../rtunes/config.h"
#include "../rtunes/extern.h"
#include "../rtunes/proto.h"

/* prototypes */

int			rtunes_init(const char *);
int			rtunes_play(const char *, int);
int			rtunes_close(void);
size_t			encode_alac(unsigned char *, const unsigned char *,
			    size_t);
void			encode_alac_writebits(unsigned char *, int, int);

/* global vars local */

int                     bitoffset = 0;
int                     byteoffset = 0;

/* global vars extern */

AES_KEY			 aeskey;
unsigned char		*aesiv;
int			 sfd_rtsp, sfd_stream;
char			 localip[16];
char			*host = NULL;

/*
 * encode_alac()
 *	encode RAW PCM data to ALAC
 * Return:
 *	<bytes> = size of ALAC sample
 */

size_t
encode_alac(unsigned char *dst, const unsigned char *src, size_t len)
{
	size_t	i;

	encode_alac_writebits(dst, 1, 3);  /* chan, 0 = mono, 1 = stereo */
	encode_alac_writebits(dst, 0, 4);  /* unknown */
	encode_alac_writebits(dst, 0, 12); /* unknown */
	encode_alac_writebits(dst, 0, 1);  /* has_size flag */
	encode_alac_writebits(dst, 0, 2);  /* uknown */
	encode_alac_writebits(dst, 1, 1);  /* no_compression flag */

	/* endian swap 16 bits samples */
	for (i = 0; i < len; i += 2) {
#if BYTE_ORDER == LITTLE_ENDIAN
		encode_alac_writebits(dst, src[i + 1], 8);
		encode_alac_writebits(dst, src[i], 8);
#else
		encode_alac_writebits(dst, src[i], 8);
		encode_alac_writebits(dst, src[i + 1], 8);
#endif
	}

	return (i + 3);
}

/*
 * encode_alac_writebits
 *	encode RAW PCM data to ALAC, encoding engine
 * Return:
 *	none
 */

void
encode_alac_writebits(unsigned char *buf, int data, int numbits)
{
	int		numwritebits;
	unsigned char	bitstowrite;

	static const unsigned char masks[8] = {
	    0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF };

	if (bitoffset != 0 && bitoffset + numbits > 8) {
		numwritebits = 8 - bitoffset;
		bitstowrite = (unsigned char)
		    ((data >> (numbits - numwritebits)) <<
		    (8 - bitoffset - numwritebits));
		buf[byteoffset] |= bitstowrite;
		numbits -= numwritebits;
		bitoffset = 0;
		byteoffset++;
	}

	while (numbits >= 8) {
		bitstowrite = (unsigned char)((data >> (numbits - 8)) & 0xFF);
		buf[byteoffset] |= bitstowrite;
		numbits -= 8;
		bitoffset = 0;
		byteoffset++;
	}

	if (numbits > 0) {
		bitstowrite = (unsigned char)((data & masks[numbits]) <<
		    (8 - bitoffset - numbits));
		buf[byteoffset] |= bitstowrite;
		bitoffset += numbits;
		if (bitoffset == 8) {
			bitoffset = 0;
			byteoffset++;
		}
	}
}

/* rtunes streams audio files to Apple's AirPort Express device */

int
rtunes_init(const char *device)
{
	int	r = 0;

	if (device == NULL)
		return (-1); /* no host defined */
	host = strdup(device);

	/* get local ip address */

	if (sys_getlocalip(localip, sizeof(localip)) == -1)
		errx(1, "Can't get local hosts IP address");

	/* connect airport rtsp port */

	if ((sfd_rtsp = sys_connect(host, AIRPORT)) == -1)
		err(1, "Connect rtsp port");

	/* rtsp connection sequence */

	if ((r = rtsp_conn()) == -1)
		errx(1, "RTSP connection sequence failed");

	/* connect airport audio stream port */

	if ((sfd_stream = sys_connect(host, r)) == -1)
		err(1, "Connect stream port");

	return (0);
}

int
rtunes_play(const char *data, int len)
{
	int		nread, a, e, s;
	unsigned char	bufraw[4096 * 2 * 2], bufala[(4096 * 2 * 2) + 3];
	unsigned char	*buf;

	/* RAW PCM audio */

	memset(bufraw, 0, sizeof(bufraw));
	memset(bufala, 0, sizeof(bufala));

	if (len > 1 && len <= sizeof(bufraw)) {
		nread = len;
		memcpy(bufraw, data, nread);
	} else
		return (-1);

	bitoffset = 0;
	byteoffset = 0;
	a = encode_alac(bufala, bufraw, (size_t)nread);
	buf = malloc(a + 32);
	memcpy(buf, bufala, a);

	e = cipher_aes_sendsample(buf, a);

	s = sys_write(sfd_stream, buf, e);

	free(buf);

	return (0);
}

int
rtunes_close(void)
{
	/* disconnect airport */

	sleep(8); /* give the airport buffer time to flush */

	close(sfd_stream);

	if (rtsp_disconnect() == -1)
		err(1, "Disconnect failed");

	close(sfd_rtsp);

	return (0);
}
