# Copyright (c) 2003-2004 E. Will et al.
# Rights to this code are documented in doc/LICENSE.
#
# This file contains build instructions.
#
# $Id: Makefile.in 8375 2007-06-03 20:03:26Z pippijn $
#

MODULE = botserv

SRCS =			\
	main.c	\
	bottalk.c	\
	help.c	\
	info.c	\
	set.c		\
	set_core.c	\
	set_fantasy.c	\
	set_nobot.c	\
	set_private.c	\
	set_saycaller.c \

include ../../extra.mk
include ../../buildsys.mk
include ../../buildsys.module.mk

CPPFLAGS	+= -I../../include
LIBS +=	-L../../libathemecore -lathemecore ${LDFLAGS_RPATH}

