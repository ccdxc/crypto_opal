#
# Makefile for MPEG4 video plugin for Unix
#
# Copyright (C) 2007 Post Increment, All Rights Reserved
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
# The Original Code is Open H323 library.
#
# The Initial Developer of the Original Code is Post Increment
#
# Contributor(s): ______________________________________.
#
# $Revision: 25962 $
# $Author: rjongbloed $
# $Date: 2011-06-02 20:10:44 -0500 (Thu, 02 Jun 2011) $
#

BASENAME := mpeg4_ffmpeg

COMMONDIR := ../common
SRCDIR    := .
SRCS      := mpeg4.cxx $(COMMONDIR)/dyna.cxx

CFLAGS +=   -I$(COMMONDIR)
LIBS   += -ldl

# Add LIBAVCODEC_SOURCE_DIR to the include path so we can #include <libavcodec/...h>
# Also add libavutil, so ffmpeg headers can #include "log.h".
LIBAVCODEC_SOURCE_DIR := 
ifneq (,$(LIBAVCODEC_SOURCE_DIR))
CFLAGS += -I$(LIBAVCODEC_SOURCE_DIR) -I$(LIBAVCODEC_SOURCE_DIR)/libavutil
endif

vpath   %.cxx $(COMMONDIR)

INSTALL_DIR := opal-3.10.11/codecs/video
PLUGINDIR   := /home/lee/sm_for_certify/sanweixinan/crypto_voip_sm/crypto_opal/opal-3.10.11_arm/plugins
include $(PLUGINDIR)/plugin-inc.mak

###########################################
