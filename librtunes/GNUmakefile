# $Id: GNUmakefile,v 1.5 2008/02/09 12:27:40 mbalmer Exp $

CCFLAGS = -O2 -pipe -Wall -Wstrict-prototypes -c
RT = ../rtunes
LB = ../libbsd

librtunes.a:	rtunes.o ${RT}/cipher.o ${RT}/mp4.o ${RT}/rtsp.o ${RT}/sys.o
		ar -r librtunes.a rtunes.o ${RT}/cipher.o ${RT}/mp4.o ${RT}/rtsp.o ${RT}/sys.o ${LB}/arc4random.o ${LB}/strlcat.o ${LB}/strlcpy.o

rtunes.o:	rtunes.c
		cc ${CCFLAGS} rtunes.c

clean:
		rm -f librtunes.a *.o
