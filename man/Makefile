include ../config.mk

MAN = berryc.1
MANUAL = "Berry Manual"
ORG = "Berry"

all: build

build: $(MAN)

%.1: *.1.md
	ronn --manual=$(MANUAL) --organization=$(ORG) $@.md

clean:
	rm *.1
	rm *.1.html

install:
	mkdir -p "$(DESTDIR)$(MANDIR)/"
	cp -f $(MAN) "$(DESTDIR)$(MANDIR)/"

uninstall:
	@echo "uninstalling manpages"
	@for page in $(MAN); do \
		rm -f $(DESTDIR)$(MANDIR)/$$page; \
	done

