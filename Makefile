# berry - a healthy, bite-sized window manager
# See LICENSE file for copyright and license details.

include config.mk

SRC = client.c utils.c wm.c
OBJ = ${SRC:.c=.o}

all: options berry berryc

options:
	@echo berry build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk globals.h ipc.h types.h utils.h

config.h:
	cp config.def.h $@

berry: utils.o wm.o
	${CC} -o $@ utils.o wm.o ${LDFLAGS}

berryc: client.o
	${CC} -o $@ client.o ${LDFLAGS}

clean:
	rm -f berry berryc ${OBJ} berry-${VERSION}.tar.gz

dist: clean
	mkdir -p berry-${VERSION}
	cp -R LICENSE Makefile README.md config.def.h config.mk\
		berry.1 berryc.1 globals.h ipc.h types.h utils.h ${SRC} berry-${VERSION}
	tar -cf berry-${VERSION}.tar berry-${VERSION}
	gzip berry-${VERSION}.tar
	rm -rf berry-${VERSION}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f berry ${DESTDIR}${PREFIX}/bin
	cp -f berryc ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/berry
	chmod 755 ${DESTDIR}${PREFIX}/bin/berryc
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < berry.1 > ${DESTDIR}${MANPREFIX}/man1/berry.1
	sed "s/VERSION/${VERSION}/g" < berryc.1 > ${DESTDIR}${MANPREFIX}/man1/berryc.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/berry.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/berryc.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/berry\
		${DESTDIR}${PREFIX}/bin/berryc\
		${DESTDIR}${MANPREFIX}/man1/berry.1\
		${DESTDIR}${MANPREFIX}/man1/berryc.1

.PHONY: all options clean dist install uninstall
