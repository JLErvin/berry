__NAME__ = berry
__NAME_CLIENT__ = berryc
__CONFIG_NAME__ = autostart
VERCMD ?= git describe 2> /dev/null
__THIS_VERSION__ = $(shell $(VERCMD) || cat VERSION)

PREFIX    = /usr/local
MANPREFIX = $(PREFIX)/share/man
MANDIR    = $(MANPREFIX)/man1
DOCPREFIX = $(PREFIX)/share/doc

CFLAGS? += -Os -pedantic -Wall
LDFLAGS +=  -lX11 -lm
