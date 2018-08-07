PREFIX?=/usr/X11R6
CFLAGS?=-Os -pedantic -Wall

all:
	$(CC) $(CFLAGS) -I$(PREFIX)/include wm.c -L$(PREFIX)/lib -lX11 -o berry
	$(CC) $(CFLAGS) -I$(PREFIX)/include client.c -L$(PREFIX)/lib -lX11 -o berryc

clean:
	rm -r berry
