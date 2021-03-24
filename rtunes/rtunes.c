/* $Id: rtunes.c,v 1.12 2008/02/10 11:51:02 mbalmer Exp $ */

/*
 * Copyright (c) 2006-2008 Marcus Glocker <marcus@nazgul.ch>
 * Copyright (c) 2006-2008 Marc Balmer <mbalmer@openbsd.org>
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
#include <sys/socket.h>

#include <err.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "extern.h"
#include "pathnames.h"
#include "proto.h"

extern void		rtunes_init(void);
static void 		usage(void);
void 			sighandler(int);
size_t			encode_alac(unsigned char *, const unsigned char *,
			    size_t);
void			encode_alac_writebits(unsigned char *, int, int);

/* global vars local */
int			bitoffset = 0;
int			byteoffset = 0;
volatile sig_atomic_t	quit = 0;

/* global vars extern */
AES_KEY			 aeskey;
unsigned char		*aesiv;
int			 sfd_rtsp, sfd_stream;
int			 ai_family = PF_UNSPEC;
char			 localip[16];
char			*host = NULL;
char			*cfgfile = _PATH_CFGFILE;

/*
 * usage()
 *	print usage message
 * Return:
 *	none
 */
static void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr, "usage: %s [-46] [-f configfile] [-h host] <file(s)>\n",
	    __progname);

	exit(1);
}

/*
 * sighandler()
 *	signal handler
 * Return:
 *	none
 */
void
sighandler(int sig)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		quit = 1;
		break;
	default:
		/* ignore */
		break;
	}
}

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
main(int argc, char *argv[])
{
	size_t		 a, e;
	ssize_t		 nread;
	int		 port = AIRPORT;
	int		 i, r, s, ch, fd, raw = 0, sample;
	char		 cwd[1024];
	unsigned char	 bufraw[4096 * 2 * 2], bufala[(4096 * 2 * 2) + 3];
	unsigned char	*buf;
	struct stco	*o;
	struct stsz	*z;
	struct stsc	*c;

	/* get command line options */
	while ((ch = getopt(argc, argv, "46f:h:p:y")) != -1) {
		switch (ch) {
		case '4':
			if (ai_family != PF_UNSPEC)
				usage();
			ai_family = PF_INET;
			break;
		case '6':
			if (ai_family != PF_UNSPEC)
				usage();
			ai_family = PF_INET6;
			break;
		case 'f':
			cfgfile = optarg;
			break;
		case 'h':
			host = optarg;
			break;
		case 'p':
			port = atoi(optarg);
			break;
		default:
			usage();
		}
	}

	rtunes_init();

	if (host == NULL)
		usage(); /* no host defined */

	if (!(argc - optind))
		usage(); /* no audio files defined */

	/* get local ip address */
	if (sys_getlocalip(localip, sizeof(localip)) == -1)
		errx(1, "Can't get local hosts IP address");

	/* connect airport rtsp port */
	if ((sfd_rtsp = sys_connect(host, port)) == -1)
		err(1, "Connect rtsp port");

	/* rtsp connection sequence */
	if ((r = rtsp_conn()) == -1)
		errx(1, "RTSP connection sequence failed");

	/* connect airport audio stream port */
	if ((sfd_stream = sys_connect(host, r)) == -1)
		err(1, "Connect stream port");

	/* change to current directory */
	if (getcwd(cwd, sizeof(cwd)) == NULL)
		err(1, "getcwd()");
	if (chdir(cwd) == -1)
		err(1, "chdir()");

	/* signal handler */
	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);

	/* RAW PCM audio from STDIN */
	if (!strcmp(argv[optind], "-"))
		raw = 1;

	/* cycle through all audio files */
	for (i = optind; argv[i] != NULL && !raw && !quit; i++) {
		/* open file */
		if ((fd = open(argv[i], O_RDONLY)) == -1) {
			warn("open()");
			continue;
		}

		/* check codec */
		if (mp4_check(fd) != 1) {
			fprintf(stderr, "File is not Apple Lossless\n");
			close(fd);
			continue;
		}

		/* demux */
		/* STCO */
		if (!mp4_seek_box(fd, "stco")) {
			fprintf(stderr, "STCO box not found\n");
			close(fd);
			continue;
		}
		if ((o = mp4_parse_stco(fd)) == NULL) {
			fprintf(stderr, "STCO parse error\n");
			close(fd);
			continue;
		}
		/* STSZ */
		if (!mp4_seek_box(fd, "stsz")) {
			fprintf(stderr, "STSZ box not found\n");
			close(fd);
			continue;
		}
		if ((z = mp4_parse_stsz(fd)) == NULL) {
			fprintf(stderr, "STSZ parse error\n");
			free(o);
			close(fd);
			continue;
		}
		/* STSC */
		if (!mp4_seek_box(fd, "stsc")) {
			fprintf(stderr, "STSC box not found\n");
			close(fd);
			continue;
		}
		if ((c = mp4_parse_stsc(fd)) == NULL) {
			fprintf(stderr, "STSC parse error\n");
			free(o);
			free(z);
			close(fd);
			continue;
		}
		/* MDAT */
		if (!mp4_seek_box(fd, "mdat")) {
			fprintf(stderr, "MDAT box not found\n");
			free(o);
			free(z);
			free(c);
			close(fd);
			continue;
		}

		/* seek to first chunk */
		lseek(fd, o->chunk_offset[1], SEEK_SET);

		/* print some info */
		printf("Playing %s\n", argv[i]);

		/* play */
		for (sample = 1; sample <= z->sample_count && !quit; sample++) {
			buf = malloc(z->entry_size[sample] + 32);

			nread = sys_read(fd, buf, z->entry_size[sample]);
			if (nread < 1) {
				free(buf);
				break;
			}

			e = cipher_aes_sendsample(buf, nread);

			s = sys_write(sfd_stream, buf, e);

			free(buf);
		}

		/* cleanup */
		free(o);
		free(z);
		free(c);
		close(fd);
	}

	/* RAW PCM audio from STDIN */
	while (raw && !quit) {
		memset(bufraw, 0, sizeof(bufraw));
		memset(bufala, 0, sizeof(bufala));

		nread = sys_read(0, bufraw, sizeof(bufraw));
		if (nread < 1)
			break;

		bitoffset = 0;
		byteoffset = 0;
		a = encode_alac(bufala, bufraw, (size_t)nread);
		buf = malloc(a + 32);
		memcpy(buf, bufala, a);

		e = cipher_aes_sendsample(buf, a);

		s = sys_write(sfd_stream, buf, e);

		free(buf);
	}

	/* disconnect airport */
	if (quit)
		printf("\n");
	printf("Closing rtunes stream, please wait ...\n");

	sleep(8); /* give the airport buffer time to flush */

	close(sfd_stream);

	if (rtsp_disconnect() == -1)
		err(1, "Disconnect failed");

	close(sfd_rtsp);

	return (0);
}
