# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overridden by command line or environment
include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = spp_pcap

SPP_WKT_DIR = ../shared/secondary/spp_worker_th

# all source are stored in SRCS-y
SRCS-y := spp_pcap.c
SRCS-y += cmd_utils.c
SRCS-y += command_proc.c command_dec.c
SRCS-y += ../shared/common.c
SRCS-y += $(SPP_WKT_DIR)/command_conn.c
SRCS-y += $(SPP_WKT_DIR)/spp_port.c
SRCS-y += $(SPP_WKT_DIR)/ringlatencystats.c
SRCS-y += $(SPP_WKT_DIR)/string_buffer.c

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += $(WERROR_FLAGS) -O3 -MMD
CFLAGS += -I$(SRCDIR)/../
CFLAGS += -I$(SRCDIR)/../$(SPP_WKT_DIR)
CFLAGS += -I$(SRCDIR)/../shared

# Optional Settings
#CFLAGS += -DSPP_DEMONIZE
#CFLAGS += -DSPP_RINGLATENCYSTATS_ENABLE

LDLIBS += -llz4

ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),y)
LDLIBS += -lrte_pmd_ring
LDLIBS += -lrte_pmd_vhost
endif

include $(RTE_SDK)/mk/rte.extapp.mk
