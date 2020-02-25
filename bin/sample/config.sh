# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Nippon Telegraph and Telephone Corporation

SPP_CTL_IP=127.0.0.1
SPP_FILE_PREFIX=spp  # used for --file-prefix option

SPP_HUGEPAGES=/dev/hugepages

# spp_primary options
LOGLEVEL=7  # change to 8 if you refer debug messages.
PRI_CORE_LIST=0,1  # required one lcore usually.
PRI_MEM=1024
PRI_MEMCHAN=4  # change for your memory channels.
NUM_RINGS=8
PRI_PORTMASK=0x03  # total num of ports of spp_primary.

# Vdevs of spp_primary
#PRI_MEMIF_VDEVS=(0 1)  # IDs of `net_memif`
#PRI_VHOST_VDEVS=(11 12)  # IDs of `eth_vhost`
#PRI_RING_VDEVS=(1 2)  # IDs of `net_ring`
#PRI_TAP_VDEVS=(1 2)  # IDs of `net_tap`
# You can give whole of vdev options here.
#PRI_VDEVS=(
#net_memif0,socket=/tmp/memif.sock,id=0,role=master
#net_memif1,socket=/tmp/memif.sock,id=1,role=master
#)

# You do not need to change usually.
# Log files are created in 'spp/log/'.
SPP_CTL_LOG=spp_ctl.log
PRI_LOG=spp_primary.log

# number of ports for multi-queue setting.
#PRI_PORT_QUEUE=(
#    "0 rxq 16 txq 16"
#)

# Add a PCI device in white list.
# `dv_flow_en=1` is required for HW offload with Mellanox NIC.
# Set a nonzero value to enables the DV flow steering assuming it is
# supported by the driver.
# https://doc.dpdk.org/guides/nics/mlx5.html
#PRI_WHITE_LIST=(
#    "0000:04:00.0,dv_flow_en=1"
#    "0000:05:00.0"
#)
