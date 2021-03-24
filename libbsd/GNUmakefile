# $Id: GNUmakefile,v 1.5 2008/02/10 15:16:20 mbalmer Exp $

CCFLAGS = -O2 -pipe -Wall -Werror -Wstrict-prototypes -c

libbsd.a:	strlcpy.o strlcat.o arc4random.o strtonum.o
		ar -r libbsd.a strlcpy.o strlcat.o arc4random.o strtonum.o
		ranlib libbsd.a

strlcpy.o:	strlcpy.c
		cc ${CCFLAGS} strlcpy.c
strlcat.o:	strlcat.c
		cc ${CCFLAGS} strlcat.c
arc4random.o:	arc4random.c
		cc ${CCFLAGS} arc4random.c
strtonum.o:	strtonum.c
		cc -D _ISOC99_SOURCE ${CCFLAGS} strtonum.c
clean:
		rm -f libbsd.a *.o
