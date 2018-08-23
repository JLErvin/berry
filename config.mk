__NAME__ = berry
__NAME_CLIENT__ = berryc
__CONFIG_NAME__ = autostart
__THIS_VERSION__ = $(shell cat VERSION)

PREFIX    = /usr/local
MANPREFIX = $(PREFIX)/share/man
MANDIR    = $(MANPREFIX)/man1
DOCPREFIX = $(PREFIX)/share/doc
X11INC    = /usr/X11R6/include
X11LIB    = /usr/X11R6/lib

CFLAGS? += -Os -pedantic -Wall
LDFLAGS += -lm -lX11 -I${X11INC} -L${X11LIB}
