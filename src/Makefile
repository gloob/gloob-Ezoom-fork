##
#
# Beryl/Compiz plugin Makefile
#
# Copyright : (C) 2006 by Dennis Kasprzyk
# E-mail    : dennis.kasprzyk@rwth-aachen.de
#
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
##

## configuration

#enter plugin name here
PLUGIN = zoom

#enter dependencies here
BUILDDEP = compiz
## end of configuration

#enter beryl or compiz here
TARGET = compiz


DESTDIR = $(HOME)/.$(TARGET)/plugins
XMLDIR = $(HOME)/.$(TARGET)/metadata

BUILDDIR = build

CC        = gcc
LIBTOOL   = libtool
INSTALL   = install

BCOP       = `pkg-config --variable=bin bcop`

CFLAGS  = -g -Wall `pkg-config --cflags $(PKG_DEP) $(TARGET) `
LDFLAGS = `pkg-config --libs $(PKG_DEP) $(TARGET) ` 

is-bcop-target := $(shell if [ -e $(PLUGIN).xml ]; then cat $(PLUGIN).xml | grep "useBcop=\"true\"";fi )

bcop-target := $(shell if [ -n "$(is-bcop-target)" ]; then echo $(PLUGIN).xml; fi )
bcop-target-src := $(shell if [ -n "$(is-bcop-target)" ]; then echo $(PLUGIN)_options.c; fi )
bcop-target-hdr := $(shell if [ -n "$(is-bcop-target)" ]; then echo $(PLUGIN)_options.h; fi )

# find all the object files (including those from .moc.cpp files)

c-objs := $(patsubst %.c,%.lo,$(shell find -name '*.c' 2> /dev/null | grep -v "$(BUILDDIR)/" | sed -e 's/^.\///'))
c-objs := $(filter-out $(bcop-target-src:.c=.lo),$(c-objs))
c-objs += $(bcop-target-src:.c=.lo)

# system include path parameter, -isystem doesn't work on old gcc's
inc-path-param = $(shell if [ -z "`gcc --version | head -n 1 | grep ' 3'`" ]; then echo "-isystem"; else echo "-I"; fi)

# default color settings
color := $(shell if [ $$TERM = "dumb" ]; then echo "no"; else echo "yes"; fi)


#
# Do it.
#

.PHONY: $(BUILDDIR) build-dir bcop-build c-build-objs c-link-plugin

all: $(BUILDDIR) build-dir bcop-build c-build-objs c-link-plugin

bcop-build:   $(bcop-target-hdr) $(bcop-target-src)

c-build-objs: $(addprefix $(BUILDDIR)/,$(cxx-objs))

c-link-plugin: $(BUILDDIR)/lib$(PLUGIN).la

#
# Create build directory
#

$(BUILDDIR) :
	@mkdir -p $(BUILDDIR)

$(DESTDIR) :
	@mkdir -p q$(DESTDIR)

#
# BCOP'ing

%_options.h: %.xml
	@if [ '$(color)' != 'no' ]; then \
		echo -e -n "\033[0;1;5mbcop'ing  \033[0;1;37m: \033[0;32m$< \033[0;1;37m-> \033[0;31m$@\033[0m"; \
	else \
		echo "bcop'ing  $<  ->  $@"; \
	fi
	@$(BCOP) --header=$@ $<
	touch $@
	@if [ '$(color)' != 'no' ]; then \
		echo -e "\r\033[0mbcop'ing  : \033[34m$< -> $@\033[0m"; \
	fi

%_options.c: %.xml
	@if [ '$(color)' != 'no' ]; then \
		echo -e -n "\033[0;1;5mbcop'ing  \033[0;1;37m: \033[0;32m$< \033[0;1;37m-> \033[0;31m$@\033[0m"; \
	else \
		echo "bcop'ing  $<  ->  $@"; \
	fi
	@$(BCOP) --source=$@ $< 
	touch $@
	@if [ '$(color)' != 'no' ]; then \
		echo -e "\r\033[0mbcop'ing  : \033[34m$< -> $@\033[0m"; \
	fi


#
# Compiling
#

$(BUILDDIR)/%.lo: %.c
	@if [ '$(color)' != 'no' ]; then \
		echo -n -e "\033[0;1;5mcompiling \033[0;1;37m: \033[0;32m$< \033[0;1;37m-> \033[0;31m$@\033[0m"; \
	else \
		echo "compiling $< -> $@"; \
	fi
	@$(LIBTOOL) --quiet --mode=compile $(CC) $(CFLAGS) -c -o $@ $<
	@if [ '$(color)' != 'no' ]; then \
		echo -e "\r\033[0mcompiling : \033[34m$< -> $@\033[0m"; \
	fi

#
# Linking
#

cxx-rpath-prefix := -Wl,-rpath,

$(BUILDDIR)/lib$(PLUGIN).la: $(addprefix $(BUILDDIR)/,$(c-objs))
	@if [ '$(color)' != 'no' ]; then \
		echo -e -n "\033[0;1;5mlinking -> \033[0;31m$@\033[0m"; \
	else \
		echo "linking -> $@"; \
	fi
	@$(LIBTOOL) --quiet --mode=link $(CC) $(LDFLAGS) -rpath $(DESTDIR) -o $@ $(addprefix $(BUILDDIR)/,$(c-objs))
	@if [ '$(color)' != 'no' ]; then \
		echo -e "\r\033[0mlinking -> \033[34m$@\033[0m"; \
	fi


clean:
	rm -rf $(BUILDDIR)
	rm -f $(bcop-target-src)
	rm -f $(bcop-target-hdr)

install: $(DESTDIR) all
	mkdir -p ~/.compiz;
	@if [ '$(color)' != 'no' ]; then \
	    echo -n -e "\033[0;1;5minstall   \033[0;1;37m: \033[0;31m$(DESTDIR)/lib$(PLUGIN).so\033[0m"; \
	else \
	    echo "install   : $(DESTDIR)/lib$(PLUGIN).so"; \
	fi
	@mkdir -p $(DESTDIR)
	@$(INSTALL) $(BUILDDIR)/.libs/lib$(PLUGIN).so $(DESTDIR)/lib$(PLUGIN).so
	@if [ '$(color)' != 'no' ]; then \
	    echo -e "\r\033[0minstall   : \033[34m$(DESTDIR)/lib$(PLUGIN).so\033[0m"; \
	fi
	@if [ -e $(PLUGIN).xml ]; then \
	    if [ '$(color)' != 'no' ]; then \
		echo -n -e "\033[0;1;5minstall   \033[0;1;37m: \033[0;31m$(XMLDIR)/$(PLUGIN).xml\033[0m"; \
	    else \
		echo "install   : $(XMLDIR)/$(PLUGIN).xml"; \
	    fi; \
	    mkdir -p $(XMLDIR); \
	    $(INSTALL) $(PLUGIN).xml $(XMLDIR)/$(PLUGIN).xml; \
	    if [ '$(color)' != 'no' ]; then \
		echo -e "\r\033[0minstall   : \033[34m$(XMLDIR)/$(PLUGIN).xml\033[0m"; \
	    fi; \
	fi
	


