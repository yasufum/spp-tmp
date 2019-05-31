# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Nippon Telegraph and Telephone Corporation

SPP_HOST_IP=127.0.0.1
SPP_HUGEPAGES=/dev/hugepages

# spp_primary options
LOGLEVEL=7  # change to 8 if you refer debug messages.
PRI_CORE_LIST=0  # required one lcore usually.
PRI_MEM=1024
PRI_MEMCHAN=4  # change for your memory channels.
NUM_RINGS=8
PRI_PORTMASK=0x03  # total num of ports of spp_primary.
#PRI_VHOST_IDS=(11 12)  # you use if you have no phy ports.

# You do not need to change usually.
# Log files created in 'spp/log/'.
SPP_CTL_LOG=spp_ctl.log
PRI_LOG=spp_primary.log
