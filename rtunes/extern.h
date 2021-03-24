/* $Id: extern.h,v 1.4 2006/09/17 12:04:31 mbalmer Exp $ */

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

#include <sys/types.h>

#include <openssl/aes.h>

/* general variables */
extern AES_KEY		 aeskey;	/* AES session key */
extern unsigned char	*aesiv;		/* AES session iv */
extern int		 sfd_rtsp, sfd_stream;
extern char		 localip[16];
extern char		*host;

/* RTSP response structure */
struct rtsp_response {
	char	protocol[8];
	int	status_n;
	char	status_s[128];
	int	cseq;
	char	ajs[128];
	char	session[128];
	char	transport[128];
};

/* MP4 demuxing */
struct stco {
	int	entry_count;

	off_t	chunk_offset[16384];
};

struct stsz {
	int	sample_size;
	int	sample_count;

	size_t	entry_size[16384];
};

struct stsc {
	int	entry_count;

	int	first_chunk[16384];
	int	samples_per_chunk[16384];
	int	sample_description_index[16384];
};

struct stsd {
	char	codec[5];
};
