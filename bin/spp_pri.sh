# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Nippon Telegraph and Telephone Corporation

# Activate for debugging
#set -x

SPP_PRI_VHOST=""
SPP_PRI_RING=""
SPP_PRI_TAP=""
SPP_PRI_MEMIF=""
SPP_PRI_VDEVS=""
SPP_PRI_PORT_QUEUE=""
SPP_PRI_WHITE_LIST=""

SOCK_VHOST="/tmp/sock"
SOCK_MEMIF="/tmp/spp-memif.sock"

function clean_sock_files() {
    # clean /tmp/sock*
    sudo rm -rf ${SOCK_VHOST}*
    sudo rm -rf ${SOCK_MEMIF}
}

# Add vhost vdevs named as such as `eth_vhost0`.
function setup_vhost_vdevs() {
    if [ ${PRI_VHOST_VDEVS} ]; then
        for id in ${PRI_VHOST_VDEVS[@]}; do
            SPP_PRI_VHOST="${SPP_PRI_VHOST} --vdev eth_vhost${id},iface=${SOCK_VHOST}${id}"
        done
    fi
}

# Add ring vdevs named as such as `net_ring0`.
function setup_ring_vdevs() {
    if [ ${PRI_RING_VDEVS} ]; then
        for id in ${PRI_RING_VDEVS[@]}; do
            SPP_PRI_RING="${SPP_PRI_RING} --vdev net_ring${id}"
        done
    fi
}

# Add tap vdevs named as such as `net_tap0`.
function setup_tap_vdevs() {
    if [ ${PRI_TAP_VDEVS} ]; then
        for id in ${PRI_TAP_VDEVS[@]}; do
            SPP_PRI_TAP="${SPP_PRI_TAP} --vdev net_tap${id},iface=vtap${id}"
        done
    fi
}

# Add memif vdevs named as such as `net_memif`.
function setup_memif_vdevs() {
    if [ ${PRI_MEMIF_VDEVS} ]; then
        for id in ${PRI_MEMIF_VDEVS[@]}; do
            SPP_PRI_MEMIF="${SPP_PRI_MEMIF} --vdev net_memif${id},id=${id},role=master,socket=${SOCK_MEMIF}"
        done
    fi
}

# Add any of vdevs.
function setup_vdevs() {
    if [ ${PRI_VDEVS} ]; then
        for vdev in ${PRI_VDEVS[@]}; do
            SPP_PRI_VDEVS="${SPP_PRI_VDEVS} --vdev ${vdev}"
        done
    fi
}

# Add queue number to port
function setup_queue_number() {
    if [ ${#PRI_PORT_QUEUE[@]} ]; then
        for (( i=0; i < ${#PRI_PORT_QUEUE[@]}; i++)); do
            SPP_PRI_PORT_QUEUE="
                ${SPP_PRI_PORT_QUEUE} --port-num ${PRI_PORT_QUEUE[${i}]}"
        done
    fi
}

# Add whitelist
function setup_whitelist() {
    if [ ${#PRI_WHITE_LIST[@]} ]; then
        for (( i=0; i < ${#PRI_WHITE_LIST[@]}; i++)); do
            SPP_PRI_WHITE_LIST="
                ${SPP_PRI_WHITE_LIST} -w ${PRI_WHITE_LIST[${i}]}"
        done
    fi
}

# Launch spp_primary.
function spp_pri() {
    SPP_PRI_BIN=${SPP_DIR}/src/primary/${RTE_TARGET}/spp_primary

    if [ ${SPP_FILE_PREFIX} ]; then
        FILE_PREFIX_OPT="--file-prefix ${SPP_FILE_PREFIX}"
    fi

    cmd="sudo ${SPP_PRI_BIN} \
        -l ${PRI_CORE_LIST} \
        -n ${PRI_MEMCHAN} \
        --socket-mem ${PRI_MEM} \
        --huge-dir ${SPP_HUGEPAGES} \
        --proc-type primary \
        --base-virtaddr 0x100000000 \
        ${FILE_PREFIX_OPT} \
        --log-level ${LOGLEVEL} \
        ${SPP_PRI_VHOST} \
        ${SPP_PRI_RING} \
        ${SPP_PRI_TAP} \
        ${SPP_PRI_MEMIF} \
        ${SPP_PRI_VDEVS} \
        ${SPP_PRI_WHITE_LIST} \
        -- \
        -p ${PRI_PORTMASK} \
        -n ${NUM_RINGS} \
        -s ${SPP_CTL_IP}:5555 \
        ${SPP_PRI_PORT_QUEUE}"

    if [ ${DRY_RUN} ]; then
        echo ${cmd}
    else
        ${cmd} > ${SPP_DIR}/log/${PRI_LOG} 2>&1 &
    fi
}
