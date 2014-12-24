##
## Default drawing engine module Makefile
##

# System-specific configuration is in system.mk
TOPDIR=..
include $(TOPDIR)/build/system-inc.mk

######################################

INCLUDES += $(X11_INCLUDES) $(LIBTU_INCLUDES) $(LIBEXTL_INCLUDES) -I.. `pkg-config --cflags xft`
CFLAGS += $(XOPEN_SOURCE) $(C99_SOURCE)

SOURCES=init.c draw.c font.c colour.c brush.c style.c precompose.c

MODULE=de

MAKE_EXPORTS=de

LIBS += `pkg-config --libs xft`

######################################

include $(TOPDIR)/build/rules.mk

######################################

_install: module_install
