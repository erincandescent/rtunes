# $Id: GNUmakefile,v 1.7 2009/06/26 14:52:31 mbalmer Exp $

CCFLAGS = -O2 -pipe -Wall -Wstrict-prototypes -D_ISOC99_SOURCE -c

rtunes:		rtunes.o cipher.o mp4.o rtsp.o sys.o parse.o
		cc -o rtunes rtunes.o cipher.o mp4.o rtsp.o sys.o \
		    parse.o -lbsd -lcrypto
		strip rtunes

rtunes.o:	rtunes.c
		cc ${CCFLAGS} rtunes.c

cipher.o:	cipher.c
		cc ${CCFLAGS} cipher.c

mp4.o:		mp4.c
		cc ${CCFLAGS} mp4.c

rtsp.o:		rtsp.c
		cc ${CCFLAGS} rtsp.c

sys.o:		sys.c
		cc ${CCFLAGS} sys.c

parse.o:	parse.y
		yacc -y -d parse.y
		cc ${CCFLAGS} -o parse.o y.tab.c

clean:
		rm -f rtunes y.tab.c y.tab.h lex.yy.c *.o
