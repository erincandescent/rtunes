/* $Id: sys.c,v 1.7 2008/02/10 11:41:10 mbalmer Exp $ */

/*
 * Copyright (c) 2008 Marc Balmer <mbalmer@openbsd.org>
 * Copyright (c) 2006-2008 Marcus Glocker <marcus@nazgul.ch>
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

/* Make glibc expose NI_MAXHOST/NI_MAXSERV */
#define _DEFAULT_SOURCE

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int ai_family;

/*
 * sys_connect()
 *	connect a host
 * Return:
 *	>0 = success, -1 = error
 */
int
sys_connect(const char *hostname, int port)
{
	struct addrinfo hints, *res, *res0;
	int s;
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	char buf[1024];
	int error;

	/* resolve address/port into sockaddr */
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = ai_family;
	snprintf(buf, sizeof(buf), "%d", port);
	error = getaddrinfo(hostname, buf, &hints, &res0);
	if (error) {
		fprintf(stderr, "%s: %s\n", hostname,
		    gai_strerror(error));
		exit(1);
		/*NOTREACHED*/
	}

	/* try all the sockaddrs until connection goes successful */
	for (res = res0; res; res = res->ai_next) {
		error = getnameinfo(res->ai_addr, res->ai_addrlen, hbuf,
		    sizeof(hbuf), sbuf, sizeof(sbuf),
		    NI_NUMERICHOST | NI_NUMERICSERV);
		if (error) {
			fprintf(stderr, "%s: %s\n", hostname,
			    gai_strerror(error));
			continue;
		}
		fprintf(stderr, "trying %s port %s\n", hbuf, sbuf);

		s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (s < 0)
			continue;

		if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
			close(s);
			s = -1;
			continue;
		}
		return s;
	}

	fprintf(stderr, "no destination to connect to\n");
	exit(1);
	/*NOTREACHED*/
}

/*
 * sys_getlocalip()
 *	get ip address of local host
 * Return:
 *	0 = success, -1 = error
 */
int
sys_getlocalip(char *ip, size_t len)
{
	char		 hostname[MAXHOSTNAMELEN + 1];
	struct hostent	*host;

	if (gethostname(hostname, sizeof(hostname)) == -1)
		return (-1);

	if ((host = gethostbyname2(hostname, AF_INET)) == NULL)
		host = gethostbyname2("localhost", AF_INET);
	if (host == NULL)
		errx(1, "kiss my ass, sugababe");

	if (inet_ntop(AF_INET, host->h_addr, ip, len) == NULL)
		errx(1, "inet_ntop failed for host %s", hostname);

	return (0);
}

/*
 * sys_read()
 *	synchronous read
 *	used for blocking descriptors
 * Return:
 *	>0 = bytes read, 0 = descriptor closed, -1 = error
 */
ssize_t
sys_read(int sfd, void *buf, size_t len)
{
	ssize_t	r;

	for (;;) {
		r = read(sfd, buf, len);

		/* handle errors */
		if (r == -1) {
			if (errno == EINTR) {
				usleep(1);
				continue;
			}
			break;
		}

		/* descriptor closed by remote */
		if (r == 0)
			break;

		/* got data */
		if (r > 0)
			break;
	}

	return (r);
}

/*
 * sys_write()
 *	synchronous write
 *	used for non-blocking and blocking sockets
 * Return:
 *	>0 = bytes written, -1 = error
 */
ssize_t
sys_write(int sfd, const void *buf, size_t len)
{
	ssize_t		 r, sent = 0;
	const char	*p = (const char *)buf;

	for (;;) {
		r = write(sfd, p + sent, len - sent);

		/* handle errors */
		if (r == -1) {
			if (errno == EINTR) {
				usleep(1);
				continue;
			}
			if (errno == EAGAIN) {
				usleep(1);
				continue;
			}
			return (r);
		}

		/* sent data */
		if (r > 0)
			sent += r;

		/* sent all */
		if (sent == len)
			break;
	}
	return (sent);
}

/*
 * sys_strcutl()
 *	cuts a specific line in a string separated by LF's
 * Return:
 *	<total number of lines> = success, -1 = failed
 */
int
sys_strcutl(char *dst, const char *src, int line, size_t dsize)
{
	int	i = 0, j = 0, cl = 0;

	/* first count all lines */
	while (1) {
		if (src[i] == '\n' && src[i + 1] == '\0') {
			cl++;
			break;
		}
		if (src[i] == '\0') {
			cl++;
			break;
		}
		if (src[i] == '\n')
			cl++;

		i++;
	}

	/* do we have the requested line ? */
	if (line > cl || line == 0)
		return (-1);

	/* go to line start */
	for (i = 0, j = 0; j != line - 1; i++)
		if (src[i] == '\n')
			j++;

	/* read requested line */
	for (j = 0; src[i] != '\n' && src[i] != '\0' && j != dsize - 1; i++) {
		if (src[i] != '\r') {
			dst[j] = src[i];
			j++;
		}
	}

	/* terminate string */
	dst[j] = '\0';

	return (cl);
}

/*
 * sys_strcutw()
 *	cuts a specific word in a string separated by spaces
 * Return:
 *	0 = success, -1 = failed
 */
int
sys_strcutw(char *dst, const char *src, int word, size_t dsize)
{
	int	i, j;

	for (i = 0, j = 1; j != word; i++) {
		if (src[i] == ' ')
			j++;
		/* word not found */
		if (src[i] == '\0')
			return (-1);
	}
		
	for (j = 0; src[i] != ' ' && src[i] != '\0' && j != dsize - 1; i++, j++)
		dst[j] = src[i];

	/* terminate string */
	dst[j] = '\0';

	return (0);
}

/*
 * sys_strcuts()
 *	cuts a string from a string starting at start char until end char
 * Return:
 *	0 = success, -1 = failed
 */
int
sys_strcuts(char *dst, const char *src, char start, char end, 
    size_t dsize)
{
	int     i, j, len;

	len = strlen(src);

        for (i = 0, j = 0; src[i] != start && start != '\0'; i++) {
		/* start not found */
		if (i > len)
			return (-1);
	}

	if (start == '\0') {
		dst[0] = src[0];
		j++;
	}
	i++;

        for (; src[i] != end && i < len && j != dsize - 1; i++, j++)
                dst[j] = src[i];

	/* terminate string */
	dst[j] = '\0';

	/* end not found */
	if (src[i] != end)
		return (-1);

        return (0);	
}
