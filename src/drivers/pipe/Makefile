# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2020 Nippon Telegraph and Telephone Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_spp_pipe.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs
LDLIBS += -lrte_bus_vdev

EXPORT_MAP := rte_pmd_spp_pipe_version.map

LIBABIVER := 2

#
# all source are stored in SRCS-y
#
SRCS-y += rte_spp_pipe.c

include $(RTE_SDK)/mk/rte.lib.mk
