# $Id: Makefile,v 1.4 2009/06/27 10:10:32 mbalmer Exp $

PROG=		rtunes
SRCS=		rtunes.c cipher.c mp4.c rtsp.c sys.c parse.y

OS!= uname -s
.if ${OS} != "OpenBSD"
YFLAGS= -d
SRCS+= y.tab.h
y.tab.h: parse.y
.endif

.if ${OS} == "NetBSD"
VPATH=	../libbsd
SRCS+=	strtonum.c
.endif

CFLAGS+=	-Wall -Wstrict-prototypes -ansi
LDADD+=		-lcrypto
LINTFLAGS+=	-u

CLEANFILES+=	y.tab.h

PIPE=		-pipe

MAN=		rtunes.1 rtunes.conf.5
MANDIR=		/usr/local/man/cat

BINDIR=		/usr/local/bin

.include <bsd.prog.mk>
