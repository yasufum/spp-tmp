#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Nippon Telegraph and Telephone Corporation

# This script is for launching spp-ctl, spp_primary and SPP CLI.
# You can launch secondaries from SPP CLI by using `pri; launch ...`.

# Activate for debugging
#set -x

SPP_DIR=$(cd $(dirname $0); pwd)/..

# import vars and functions
. ${SPP_DIR}/bin/env.sh

echo "Start spp-ctl"
python3 ${SPP_DIR}/src/spp-ctl/spp-ctl -b ${SPP_HOST_IP} \
    > ${SPP_DIR}/log/${SPP_CTL_LOG} 2>&1 &

echo "Start spp_primary"
. ${SPP_DIR}/bin/spp_pri.sh
clean_sock_files  # remove /tmp/sock* as initialization
setup_vdevs  # you use vdevs if you have no phy ports
spp_pri  # launch spp_primary

echo "Waiting for spp-ctl is ready ..."
sleep 1

python3 ${SPP_DIR}/src/spp.py -b ${SPP_HOST_IP}
