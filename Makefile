include config.mk

PREFIX?=/usr/X11R6
CFLAGS?=-Os -pedantic -Wall

all:
	$(CC) $(CFLAGS) -I$(PREFIX)/include wm.c -L$(PREFIX)/lib -lX11 -lm -o berry
	$(CC) $(CFLAGS) -I$(PREFIX)/include client.c -L$(PREFIX)/lib -lX11 -o berryc

install:
	mkdir -p "$(DESTDIR)$(PREFIX)/bin"
	install $(__NAME__) "$(DESTDIR)$(PREFIX)/bin/$(__NAME__)"
	install $(__NAME_CLIENT__) "$(DESTDIR)$(PREFIX)/bin/$(__NAME_CLIENT__)"

clean:
	rm -f berry
	rm -r berryc
