# To enable debug output:
#DEBUG_CPPFLAGS = -DDEBUG
#DEBUG_CFLAGS = -g

INSTALL = install
INSTALL_PROGRAM = $(INSTALL)
INSTALL_DATA = $(INSTALL) -m 644

prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
datarootdir = $(prefix)/share
mandir = $(datarootdir)/man
man1dir = $(mandir)/man1

__NAME__ = berry
__NAME_WM__ = $(__NAME__)
__NAME_C__ = $(__NAME__)c

__NAME_UPPERCASE__ = `echo $(__NAME__) | sed 's/.*/\U&/'`
__NAME_CAPITALIZED__ = `echo $(__NAME__) | sed 's/^./\U&\E/'`

__NAME_WM_UPPERCASE__ = `echo $(__NAME_WM__) | sed 's/.*/\U&/'`
__NAME_WM_CAPITALIZED__ = `echo $(__NAME_WM__) | sed 's/^./\U&\E/'`

__NAME_C_UPPERCASE__ = `echo $(__NAME_C__) | sed 's/.*/\U&/'`
__NAME_C_CAPITALIZED__ = `echo $(__NAME_C__) | sed 's/^./\U&\E/'`

NAME_DEFINES = \
		-D__NAME__=\"$(__NAME__)\" \
		-D__NAME_UPPERCASE__=\"$(__NAME_UPPERCASE__)\" \
		-D__NAME_CAPITALIZED__=\"$(__NAME_CAPITALIZED__)\" \
		-D__NAME_WM__=\"$(__NAME_WM__)\" \
		-D__NAME_WM_UPPERCASE__=\"$(__NAME_WM_UPPERCASE__)\" \
		-D__NAME_WM_CAPITALIZED__=\"$(__NAME_WM_CAPITALIZED__)\" \
		-D__NAME_C__=\"$(__NAME_C__)\" \
		-D__NAME_C_UPPERCASE__=\"$(__NAME_C_UPPERCASE__)\" \
		-D__NAME_C_CAPITALIZED__=\"$(__NAME_C_CAPITALIZED__)\" \

# Flags used in rules.
CPPFLAGS += $(NAME_DEFINES) -DSRVR_$$HOSTNAME $(DEBUG_CPPFLAGS)
#CFLAGS += -std=c99 -Wall -Wextra -O3 $(DEBUG_CFLAGS)
CFLAGS += -Wall -Wextra -O3 $(DEBUG_CFLAGS)
CFLAGS += -Icore -Iinclude -I/usr/include/freetype2
LDFLAGS += -lX11 -lXrandr -lXft
