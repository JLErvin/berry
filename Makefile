# berry - a healthy, bite-sized window manager
# See LICENSE file for copyright and license details.

-include Config.mk

################ Source files ##########################################

berry	:= $O${name}
berryc	:= ${berry}c
srcs	:= $(wildcard *.c)
objs	:= $(addprefix $O,$(srcs:.c=.o))
deps	:= ${objs:.o=.d}
confs	:= Config.mk config.h
oname   := $(notdir $(abspath $O))

################ Compilation ###########################################

.SUFFIXES:
.PHONY: all clean distclean maintainer-clean

all:	${berry} ${berryc}

${berry}:	$Outils.o $Owm.o
	@echo "Linking $@ ..."
	@${CC} ${ldflags} -o $@ $^ ${libs}

${berryc}:	$Oclient.o
	@echo "Linking $@ ..."
	@${CC} ${ldflags} -o $@ $^ ${libs}

$O%.o:	%.c
	@echo "    Compiling $< ..."
	@${CC} ${cflags} -MMD -MT "$(<:.c=.s) $@" -o $@ -c $<

%.s:	%.c
	@echo "    Compiling $< to assembly ..."
	@${CC} ${cflags} -S -o $@ -c $<

################ Installation ##########################################

.PHONY:	install installdirs
.PHONY: uninstall uninstall-man

ifdef bindir
exed	:= ${DESTDIR}${bindir}
berryi	:= ${exed}/$(notdir ${berry})
berryci	:= ${exed}/$(notdir ${berryc})

${exed}:
	@echo "Creating $@ ..."
	@${INSTALL} -d $@
${berryi}:	${berry} | ${exed}
	@echo "Installing $@ ..."
	@${INSTALL_PROGRAM} $< $@
${berryci}:	${berryc} | ${exed}
	@echo "Installing $@ ..."
	@${INSTALL_PROGRAM} $< $@

installdirs:	${exed}
install:	${berryi} ${berryci}
uninstall:
	@if [ -f ${exei} ]; then\
	    echo "Removing ${berryi} and ${berryci} ...";\
	    rm -f ${berryi} ${berryci};\
	fi
endif
ifdef man1dir
mand	:= ${DESTDIR}${man1dir}
mani	:= ${mand}/${name}.1
manci	:= ${mand}/${name}c.1

${mand}:
	@echo "Creating $@ ..."
	@${INSTALL} -d $@
${mani}:	${name}.1 | ${mand}
	@echo "Installing $@ ..."
	@${INSTALL_DATA} $< $@
${manci}:	${name}c.1 | ${mand}
	@echo "Installing $@ ..."
	@${INSTALL_DATA} $< $@

installdirs:	${mand}
install:	${mani} ${manci}
uninstall:	uninstall-man
uninstall-man:
	@if [ -f ${mani} ]; then\
	    echo "Removing ${mani} and ${manci} ...";\
	    rm -f ${mani} ${manci};\
	fi
endif

################ Maintenance ###########################################

clean:
	@if [ -d ${builddir} ]; then\
	    rm -f ${berry} ${berryc} ${objs} ${deps} $O.d;\
	    rmdir ${builddir};\
	fi

distclean:	clean
	@rm -f ${oname} ${confs} config.status

maintainer-clean: distclean

${builddir}/.d:
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	@touch $@
$O.d:	| ${builddir}/.d
	@[ -h ${oname} ] || ln -sf ${builddir} ${oname}

${objs}:	Makefile ${confs} | $O.d
config.h:	config.h.in | Config.mk
Config.mk:	Config.mk.in
${confs}:	configure
	@if [ -x config.status ]; then echo "Reconfiguring ...";\
	    ./config.status;\
	else echo "Running configure ...";\
	    ./configure;\
	fi

-include ${deps}
