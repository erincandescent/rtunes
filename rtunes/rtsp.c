/* $Id: rtsp.c,v 1.9 2009/06/27 10:10:32 mbalmer Exp $ */

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/rand.h>

#include "config.h"
#include "extern.h"
#include "proto.h"

#ifdef __linux__
#include "bsd/string.h"
#endif

/* global vars local */
int			 cseq;
char			 sid[12];
char			 session[128];
static const char	*useragent = "iTunes/6.0.2 (Macintosh; N; PPC)";

/*
 * rtsp_generate_sid()
 *	generate a random session id
 * Return:
 *	0 = success
 */
int
rtsp_generate_sid(char *buf, int len)
{
	int	i = 0;

	while (i < 1000000000)
		RAND_bytes((unsigned char*) &i, sizeof(i));

	snprintf(buf, len, "%d", i);

	return (0);
}

/*
 * rtsp_conn()
 *	RTSP connection sequence
 * Return:
 *	>0 = streaming port, -1 = error
 */
int
rtsp_conn(void)
{
	int			 stream_port;
	char			 tmp[1024];
	struct rtsp_response	*resp;

	/* reset sequence number */
	cseq = 0;

	/* generate sid */
	rtsp_generate_sid(sid, sizeof(sid));

	/* Announce */
	if (rtsp_Announce() == -1)
		return (-1);
	if ((resp = rtsp_parse_response()) == NULL)
		return (-1);
	if (resp->status_n != 200)
		return (-1);
	free(resp);

	/* Setup */
	if (rtsp_Setup() == -1)
		return (-1);
	if ((resp = rtsp_parse_response()) == NULL)
		return (-1);
	if (resp->status_n != 200)
		return (-1);
	strlcpy(session, resp->session, sizeof(session));
	if (!rtsp_parse_token(tmp, resp->transport, "server_port", sizeof(tmp)))
		return (-1);
	stream_port = atoi(tmp);
	free(resp);

	/* Record */
	if (rtsp_Record() == -1)
		return (-1);
	if ((resp = rtsp_parse_response()) == NULL)
		return (-1);
	if (resp->status_n != 200)
		return (-1);
	free(resp);

	/* Setparameter */
	if (rtsp_Setparameter() == -1)
		return (-1);
	if ((resp = rtsp_parse_response()) == NULL)
		return (-1);
	if (resp->status_n != 200)
		return (-1);
	free(resp);

	return (stream_port);
}

/*
 * rtsp_disconnect()
 *	RTSP disconnect sequence
 * Return:
 *	0 = success, -1 = error
 */
int
rtsp_disconnect(void)
{
	struct rtsp_response	*resp;

	/* Flush */
#if 0
	if (rtsp_Flush() == -1)
		return (-1);
	if ((resp = rtsp_parse_response()) == NULL)
		return (-1);
	if (resp->status_n != 200)
		return (-1);
	free(resp);
#endif
	/* Teardown */
	if (rtsp_Teardown() == -1)
		return (-1);
	if ((resp = rtsp_parse_response()) == NULL)
		return (-1);
	if (resp->status_n != 200)
		return (-1);
	free(resp);

	return (0);
}

/*
 * rtsp_Announce()
 *	ANNOUNCE method
 * Return:
 *	0 = success, -1 = error
 */
int
rtsp_Announce(void)
{
	size_t		 len, len_body, len_header;
	int		 r = 0;
	char		 body[1024], header[1024];
	char		*entity;
	unsigned char	*tmp_aeskey, *tmp_rsaaeskey;
	unsigned char	 base64_aesiv[512], base64_rsaaeskey[512];

	memset(base64_aesiv, 0, sizeof(base64_aesiv));
	memset(base64_rsaaeskey, 0, sizeof(base64_rsaaeskey));

	/* generate aes iv */
	aesiv = cipher_aes_generate_key();

	/* generate aes key */
	tmp_aeskey = cipher_aes_generate_key();
	if (AES_set_encrypt_key(tmp_aeskey, 128, &aeskey) != 0)
		return (-1);

	/* RSA encrypt aes key */
	tmp_rsaaeskey = cipher_rsa_encrypt_aeskey(tmp_aeskey);

	/* base64 encode the keys */
	cipher_base64_encode(base64_aesiv, aesiv, sizeof(base64_aesiv), 16);
	cipher_base64_encode(base64_rsaaeskey, tmp_rsaaeskey,
	    sizeof(base64_rsaaeskey), 256);

	/* remove base64 padding */
	cipher_base64_rp((char *)base64_aesiv, strlen((char *)base64_aesiv));
	cipher_base64_rp((char *)base64_rsaaeskey,
	    strlen((char *)base64_rsaaeskey));

	/* build body */
	len_body = snprintf(body, sizeof(body),
	    "v=0\r\n"
	    "o=iTunes %s 0 IN IP4 %s\r\n"
	    "s=iTunes\r\n"
	    "c=IN IP4 %s\r\n"
	    "t=0 0\r\n"
	    "m=audio 0 RTP/AVP 96\r\n"
	    "a=rtpmap:96 AppleLossless\r\n"
	    "a=fmtp:96 4096 0 16 40 10 14 2 255 0 0 44100\r\n"
	    "a=rsaaeskey:%s\r\n"
	    "a=aesiv:%s\r\n",
	    sid, localip, host, base64_rsaaeskey, base64_aesiv);

	/* build header */
	len_header = snprintf(header, sizeof(header),
	    "%s"
	    "Content-Type: application/sdp\r\n"
	    "Content-Length: %ld\r\n"
	    "\r\n",
	    rtsp_execreq("ANNOUNCE"), (long)len_body);

	/* concatenate entity */
	len = len_header + len_body + 1;
	entity = malloc(len);
	strlcpy(entity, header, len);
	strlcat(entity, body, len);

	/* send */
	if (sys_write(sfd_rtsp, entity, strlen(entity)) < strlen(entity))
		r = -1;

	free(entity);

	return (r);
}

/*
 * rtsp_Setup()
 *	SETUP method
 * Return:
 * 	0 = success, -1 = error
 */
int
rtsp_Setup(void)
{
	int	r = 0;
	char	header[1024];

	/* build header */
	snprintf(header, sizeof(header),
	    "%s"
	    "Transport: RTP/AVP/TCP;unicast;interleaved=0-1;"
	    "mode=record;control_port=0;timing_port=0\r\n"
	    "\r\n",
	    rtsp_execreq("SETUP"));

	/* send */
	if (sys_write(sfd_rtsp, header, strlen(header)) < strlen(header))
		r = -1;

	return (r);
}

/*
 * rtsp_Recod()
 *	RECORD method
 * Return:
 *	0 = success, -1 = error
 */
int
rtsp_Record(void)
{
	int	r = 0;
	char	header[1024];

	/* build header */
	snprintf(header, sizeof(header),
	    "%s"
	    "Session: %s\r\n"
	    "Range: npt=0-\r\n"
	    "RTP-Info: seq=0;rtptime=0\r\n" 
	    "\r\n",
	    rtsp_execreq("RECORD"), session);

	/* send */
	if (sys_write(sfd_rtsp, header, strlen(header)) < strlen(header))
		r = -1;

	return (r);
}

/*
 * rtsp_Setparameter()
 *	SETPARAMETER method
 * Return:
 * 	0 = success, -1 = error
 */
int
rtsp_Setparameter(void)
{
	int	 r = 0, len, len_body, len_header;
	char	 body[1024], header[1024];
	char	*entity;

	/* build body */
	len_body = snprintf(body, sizeof(body),
	    "volume: 0.000000\r\n");

	/* build header */
	len_header = snprintf(header, sizeof(header),
	    "%s"
	    "Session: %s\r\n"
	    "Content-Type: text/parameters\r\n"
	    "Content-Length: %d\r\n"
	    "\r\n",
	    rtsp_execreq("SET_PARAMETER"), session, len_body);

	/* concatinate entity */
	len = len_header + len_body + 1;
	entity = malloc(len);
	strlcpy(entity, header, len);
	strlcat(entity, body, len);

	/* send */
	if (sys_write(sfd_rtsp, entity, strlen(entity)) < strlen(entity))
		r = -1;

	free(entity);

	return (r);
}

/*
 * rtsp_Flush()
 *	I don't know for what this exactly, yet :-/
 * Return:
 *	0 = success, -1 = error
 */
int
rtsp_Flush(void)
{
	int	r = 0;
	char	header[1024];

	/* build header */
	snprintf(header, sizeof(header),
	    "%s"
	    "Session: %s\r\n"
	    "RTP-Info: seq=0;rtptime=0\r\n" 
	    "\r\n",
	    rtsp_execreq("FLUSH"), session);

	/* send */
	if (sys_write(sfd_rtsp, header, strlen(header)) < strlen(header))
		r = -1;

	return (r);
}

/*
 * rtsp_Teardown()
 *	TEARDOWN method
 * Return:
 *	0 = success, -1 = error
 */
int
rtsp_Teardown(void)
{
	int	r = 0;
	char	header[1024];

	/* build header */
	snprintf(header, sizeof(header),
	    "%s"
	    "Session: %s\r\n"
	    "\r\n",
	    rtsp_execreq("TEARDOWN"), session);
	
	/* send */
	if (sys_write(sfd_rtsp, header, strlen(header)) < strlen(header))
		r = -1;

	return (r);
}

/*
 * rtsp_parse_token()
 *	parses one token value from a token string
 * Return:
 *	1 = token found, 0 = not token found
 */
int
rtsp_parse_token(char *buf, const char *string, const char *token,
    int len)
{
	int	 r;
	char	 t[128], v[128];
	char	*p, *s;

	/* don't destroy the original token string */
	s = strdup(string);

	/* search */
	while ((p = strsep(&s, ";"))) {
		r = sys_strcuts(t, p, '\0', '=', sizeof(t));
		if (r == -1)
			continue;
		r = sys_strcuts(v, p, '=', '\0', sizeof(v));
		if (r == -1)
			continue;
		if (!strcmp(t, token)) {
			strlcpy(buf, v, len);
			return (1);
		}
	}

	free(s);

	return (0);
}

/*
 * rtsp_exeqreq()
 *	build the default request header part
 * Return:
 * 	<pointer to static header> = success
 */
char *
rtsp_execreq(const char *method)
{
	static char	buf[1024];

	/* bump sequence number */
	cseq++;

	/* build default request header */
	snprintf(buf, sizeof(buf),
	    "%s rtsp://%s/%s RTSP/1.0\r\n"
	    "CSeq: %d\r\n"
	    "User-Agent: %s\r\n",
	    method, localip, sid, cseq, useragent);

	return (buf);
}

/*
 * rtsp_parse_response()
 *	parse server response
 * Return:
 *	<pointer to response structure> = success, NULL = error
 */
struct rtsp_response *
rtsp_parse_response(void)
{
	int			 i;
	char			 response[1024];
	char			 tmp[1024], line[1024], option[1024];
	struct rtsp_response	*h = NULL;

	memset(response, 0, sizeof(response));

	if ((h = malloc(sizeof(struct rtsp_response))) == NULL)
		return (NULL);

	/* get response */
	sys_read(sfd_rtsp, response, sizeof(response));

	/* parse response status line */
	sys_strcutl(line, response, 1, sizeof(line));
	sys_strcutw(h->protocol, line, 1, sizeof(h->protocol));
	sys_strcutw(tmp, line, 2, sizeof(tmp));
	h->status_n = atoi(tmp);
	sys_strcutw(h->status_s, line, 3, sizeof(h->status_s));

	/* parse response options */
	for (i = 2; sys_strcutl(line, response, i, sizeof(line)) != -1; i++) {
		sys_strcutw(option, line, 1, sizeof(option));
		/* CSeq: */
		if (!strcasecmp(option, "CSeq:")) {
			sys_strcuts(tmp, line, ' ', '\0',
			    sizeof(tmp));
			h->cseq = atoi(tmp);
		}
		/* Audio-Jack-Status: */
		if (!strcasecmp(option, "Audio-Jack-Status:")) {
			sys_strcuts(h->ajs, line, ' ', '\0',
			    sizeof(h->ajs));
		}
		/* Session: */
		if (!strcasecmp(option, "Session:")) {
			sys_strcuts(h->session, line, ' ', '\0',
			    sizeof(h->session));
		}
		/* Transport: */
		if (!strcasecmp(option, "Transport:")) {
			sys_strcuts(h->transport, line, ' ', '\0',
			    sizeof(h->transport));
		}
	}

	return (h);
}
