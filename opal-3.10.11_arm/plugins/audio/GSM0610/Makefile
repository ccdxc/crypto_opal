#
# Makefile for GSM 06.10
#
# Copyright (C) 2004 Post Increment, All Rights Reserved
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

BASENAME := gsm0610

SRCDIR := ./src
SRCS   := gsm06_10_codec.c

GSM_SYSTEM=no
ifeq ($(GSM_SYSTEM),no)
SRCS += $(SRCDIR)/gsm_create.c \
        $(SRCDIR)/gsm_destroy.c \
        $(SRCDIR)/gsm_decode.c \
        $(SRCDIR)/gsm_encode.c \
        $(SRCDIR)/gsm_option.c \
        $(SRCDIR)/code.c \
        $(SRCDIR)/decode.c \
        $(SRCDIR)/add.c \
        $(SRCDIR)/gsm_lpc.c \
        $(SRCDIR)/rpe.c \
        $(SRCDIR)/preprocess.c \
        $(SRCDIR)/long_term.c \
        $(SRCDIR)/short_term.c \
        $(SRCDIR)/table.c
CFLAGS += -I./inc -DWAV49 -DNeedFunctionPrototypes=1
else
CFLAGS += 
LIBS   += 
endif

INSTALL_DIR := opal-3.10.11/codecs/audio
PLUGINDIR   := /home/lee/sm_for_certify/sanweixinan/crypto_voip_sm/crypto_opal/opal-3.10.11_arm/plugins
include $(PLUGINDIR)/plugin-inc.mak

###########################################
