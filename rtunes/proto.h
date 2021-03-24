/* $Id: proto.h,v 1.5 2006/09/17 12:03:34 mbalmer Exp $ */

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

/* cipher.c */
int			 cipher_base64_encode(unsigned char *,
			     const unsigned char *, int, int);
int			 cipher_base64_decode(unsigned char *,
			     const unsigned char *, int, int);
int			 cipher_base64_rp(char *, int);
size_t			 cipher_aes_sendsample(unsigned char *, size_t);
unsigned char		*cipher_rsa_encrypt_aeskey(unsigned char *);
unsigned char		*cipher_aes_generate_key(void);

/* mp4.c */
int			 mp4_check(int);
int			 mp4_revert(unsigned char *);
int			 mp4_seek_box(int, const char *);
struct stco		*mp4_parse_stco(int);
struct stsz		*mp4_parse_stsz(int);
struct stsc		*mp4_parse_stsc(int);
struct stsd		*mp4_parse_stsd(int);

/* rtsp.c */
int			 rtsp_generate_sid(char *, int);
int			 rtsp_conn(void);
int			 rtsp_disconnect(void);
int			 rtsp_Announce(void);
int			 rtsp_Setup(void);
int			 rtsp_Record(void);
int			 rtsp_Setparameter(void);
int			 rtsp_Teardown(void);
int			 rtsp_parse_token(char *, const char *, const char *,
			     int);
char			*rtsp_execreq(const char *);
struct rtsp_response	*rtsp_parse_response(void);

/* sys.c */
int			 sys_connect(const char *, int);
int			 sys_getlocalip(char *, size_t);
ssize_t			 sys_read(int, void *, size_t);
ssize_t			 sys_write(int, const void *, size_t);
int			 sys_strcutl(char *, const char *, int, size_t);
int			 sys_strcutw(char *, const char *, int, size_t);
int			 sys_strcuts(char *, const char *, char, char, size_t);
