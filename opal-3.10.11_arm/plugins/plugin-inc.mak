#
# Common included Makefile for plug ins
#
# Copyright (C) 2011 Vox Lucida
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is OPAL.
#
# The Initial Developer of the Original Code is Robert Jongbloed
#
# Contributor(s): ______________________________________.
#
# $Revision$
# $Author$
# $Date$
#

prefix        := /usr/local
exec_prefix   := ${prefix}
libdir        := ${exec_prefix}/lib

CC            := arm-arago-linux-gnueabi-gcc
CXX           := arm-arago-linux-gnueabi-g++
CFLAGS        +=  -DPTRACING=1 -D_REENTRANT -fno-exceptions -I/usr/local/include -I/usr/local/Gmssl_arm/include   -fPIC   -I/usr/local/include -I$(PLUGINDIR)/../include -I$(PLUGINDIR)
LDFLAGS       += -L/usr/local/srtp2_arm/lib -shared -Wl,-soname,$(SONAME)
PLUGINEXT     :=so

ifneq ($(DEBUG),)
CFLAGS        += -g
endif

OBJDIR := $(PLUGINDIR)/../lib_linux_arm/plugins/$(BASENAME)

SONAME	= $(BASENAME)_ptplugin

PLUGIN_NAME = $(SONAME).$(PLUGINEXT)
PLUGIN_PATH = $(OBJDIR)/$(PLUGIN_NAME)

all: $(PLUGIN_PATH)


ifeq ($(VERBOSE),)
Q_CC = @echo [CC] `echo $< | sed s^$(SRCDIR)/^^` ;
Q_LD = @echo [LD] `echo $@ | sed s^$(OBJDIR)/^^` ;
endif


vpath	%.o $(OBJDIR)
vpath	%.c $(SRCDIR)
vpath	%.cxx $(SRCDIR)

$(OBJDIR)/%.o : %.c
	@mkdir -p $(OBJDIR) >/dev/null 2>&1
	$(Q_CC)$(CC) -c $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o : %.cxx
	@mkdir -p $(OBJDIR) >/dev/null 2>&1
	$(Q_CC)$(CXX) -c $(CXXFLAGS) $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o : %.cpp
	@mkdir -p $(OBJDIR) >/dev/null 2>&1
	$(Q_CC)$(CXX) -c $(CXXFLAGS) $(CFLAGS) -o $@ $<

OBJECTS	= $(addprefix $(OBJDIR)/,$(patsubst %.cxx,%.o,$(patsubst %.cpp,%.o,$(patsubst %.c,%.o,$(notdir $(SRCS))))))

$(PLUGIN_PATH): $(SUBDIRS) $(OBJECTS)
	$(Q_LD)$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)

ifneq ($(SUBDIRS),)
.PHONY: $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@
endif

install:
	@$(foreach dir,$(SUBDIRS),if test -d ${dir} ; then $(MAKE) -C $(dir) install; fi ; )
	mkdir -p $(DESTDIR)$(libdir)/$(INSTALL_DIR)
	install $(PLUGIN_PATH) $(DESTDIR)$(libdir)/$(INSTALL_DIR)

uninstall:
	@$(foreach dir,$(SUBDIRS),if test -d ${dir} ; then $(MAKE) -C $(dir) uninstall; fi ; )
	rm -f $(DESTDIR)$(libdir)/$(INSTALL_DIR)/$(PLUGIN_NAME)

clean:
	@$(foreach dir,$(SUBDIRS),if test -d ${dir} ; then $(MAKE) -C $(dir) clean; fi ; )
	rm -rf $(OBJDIR)

###########################################
