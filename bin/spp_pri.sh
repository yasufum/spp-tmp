# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Nippon Telegraph and Telephone Corporation

# Activate for debugging
#set -x

SPP_PRI_VHOST=""

function clean_sock_files() {
    # clean /tmp/sock*
    sudo rm -f /tmp/sock*
}

# Add vdevs if no physical ports.
function setup_vdevs() {
    if [ ${PRI_VHOST_IDS} ]; then
        for id in ${PRI_VHOST_IDS[@]}; do
            SPP_SOCK="/tmp/sock${id}"
            SPP_PRI_VHOST="${SPP_PRI_VHOST} --vdev eth_vhost${id},iface=${SPP_SOCK}"
        done
    fi
}

# Launch spp_primary.
function spp_pri() {
    SPP_PRI_BIN=${SPP_DIR}/src/primary/${RTE_TARGET}/spp_primary
    sudo ${SPP_PRI_BIN} \
        -l ${PRI_CORE_LIST} \
        -n ${PRI_MEMCHAN} \
        --socket-mem ${PRI_MEM} \
        --huge-dir ${SPP_HUGEPAGES} \
        --proc-type primary \
        --base-virtaddr 0x100000000 \
        --log-level ${LOGLEVEL} \
        ${SPP_PRI_VHOST} \
        -- \
        -p ${PRI_PORTMASK} \
        -n ${NUM_RINGS} \
        -s ${SPP_HOST_IP}:5555 \
        > ${SPP_DIR}/log/${PRI_LOG} 2>&1 &
}
