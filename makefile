# ----------------------------
# Makefile Options
# ----------------------------

NAME = CHESS
DESCRIPTION = "A chess board"
COMPRESSED = YES

CFLAGS = -Wall -Wextra -Oz
CXXFLAGS = -Wall -Wextra -Oz
HAS_PRINTF = NO

DEPS = $(SRCDIR)/piece_renders.c
EXTRA_C_SOURCES = $(SRCDIR)/piece_renders.c

# ----------------------------

include $(shell cedev-config --makefile)