include config.mk

NAME_DEFINES = -D__NAME__=\"$(__NAME__)\"                 \
			   -D__NAME_CLIENT__=\"$(__NAME_CLIENT__)\"   \
			   -D__THIS_VERSION__=\"$(__THIS_VERSION__)\" \
			   -D__CONFIG_NAME__=\"$(__CONFIG_NAME__)\"   \

PREFIX?=/usr/X11R6
CFLAGS?=-Os -pedantic -Wall $(NAME_DEFINES)

all:
	$(CC) $(CFLAGS) -I$(PREFIX)/include src/utils.c src/wm.c -L$(PREFIX)/lib -lX11 -lm -lXinerama -lXft -lXext -o berry
	$(CC) $(CFLAGS) -I$(PREFIX)/include src/client.c -L$(PREFIX)/lib -lX11 -o berryc

install:
	mkdir -p "$(DESTDIR)$(PREFIX)/bin"
	install $(__NAME__) "$(DESTDIR)/usr/local/bin/$(__NAME__)"
	install $(__NAME_C__) "$(DESTIR)/usr/local/bin/$(__NAME_C__)"
	cd ./man; $(MAKE) install

clean:
	rm -f berry
	rm -f berryc
