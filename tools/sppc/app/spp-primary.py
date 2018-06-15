#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

import argparse
import os
import subprocess
import sys

work_dir = os.path.dirname(__file__)
sys.path.append(work_dir + '/..')
from conf import env
from lib import app_helper
from lib import common

target_name = 'spp'


def parse_args():
    parser = argparse.ArgumentParser(
        description="Launcher for spp-primary application container")

    parser = app_helper.add_eal_args(parser)

    # Application specific arguments
    parser.add_argument(
        '-n', '--nof-ring',
        type=int,
        default=10,
        help='Maximum number of Ring PMD')
    parser.add_argument(
        '-p', '--port-mask',
        type=str,
        help='Port mask')
    parser.add_argument(
        '-dv', '--dev-vhost-ids',
        type=str,
        help='vhost device IDs')
    parser.add_argument(
        '-dt', '--dev-tap-ids',
        type=str,
        help='TAP device IDs')
    parser.add_argument(
        '-ip', '--ctrl-ip',
        type=str,
        help="IP address of SPP controller")
    parser.add_argument(
        '--ctrl-port',
        type=int,
        default=5555,
        help="Port of SPP controller")

    parser = app_helper.add_sppc_args(parser)

    return parser.parse_args()


def main():
    args = parse_args()

    # Setup docker command.
    docker_cmd = ['sudo', 'docker', 'run', '\\']
    docker_opts = []

    # This container is running in backgroud in defualt.
    if args.foreground is not True:
        docker_opts += ['-d', '\\']
    else:
        docker_opts += ['-it', '\\']

    docker_opts += [
        '--privileged', '\\',  # should be privileged
        '-v', '/dev/hugepages:/dev/hugepages', '\\',
        '-v', '/var/run/:/var/run/', '\\']

    if args.dev_vhost_ids is not None:
        docker_opts += ['-v', '/tmp:/tmp', '\\']

    # Setup for TAP devices with given device IDs.
    if args.dev_tap_ids is not None:
        dev_tap_ids = app_helper.dev_ids_to_list(args.dev_tap_ids)
    else:
        dev_tap_ids = []

    # Setup for vhost devices with given device IDs.
    if args.dev_vhost_ids is not None:
        dev_vhost_ids = app_helper.dev_ids_to_list(args.dev_vhost_ids)
        socks = []
        for dev_id in dev_vhost_ids:
            socks.append({
                'host': '/tmp/sock%d' % dev_id,
                'guest': '/tmp/sock%d' % dev_id})
    else:
        dev_vhost_ids = []

    if args.container_image is not None:
        container_image = args.container_image
    else:
        # Container image name, for exp 'sppc/dpdk-ubuntu:18.04'
        container_image = common.container_img_name(
            env.CONTAINER_IMG_NAME[target_name],
            args.dist_name,
            args.dist_ver)

    docker_opts += [
        container_image, '\\']

    # Setup spp primary command.
    cmd_path = '%s/../spp/src/primary/%s/spp_primary' % (
        env.RTE_SDK, env.RTE_TARGET)

    spp_cmd = [cmd_path, '\\']

    # Do not use 'app_helper.setup_eal_opts()' because spp_primary does
    # not use virtio vdev but TAP or vhost, which should be added manually.
    core_opt = app_helper.get_core_opt(args)
    mem_opt = app_helper.get_mem_opt(args)
    eal_opts = [
        core_opt['attr'], core_opt['val'], '\\',
        '-n', str(args.nof_memchan), '\\',
        mem_opt['attr'], mem_opt['val'], '\\',
        '--huge-dir', '/dev/hugepages', '\\',
        '--proc-type', 'primary', '\\']

    # Add TAP vdevs
    for i in range(len(dev_tap_ids)):
        eal_opts += [
            '--vdev', 'net_tap%d,iface=foo%d' % (
                dev_tap_ids[i], dev_tap_ids[i]), '\\']

    # Add vhost vdevs
    for i in range(len(dev_vhost_ids)):
        eal_opts += [
            '--vdev', 'eth_vhost%d,iface=%s' % (
                dev_vhost_ids[i], socks[i]['guest']), '\\']

    eal_opts += ['--', '\\']

    spp_opts = []
    # Check for other mandatory opitons.
    if args.port_mask is None:
        common.error_exit('port_mask')
    else:
        spp_opts += ['-p', args.port_mask, '\\']

    spp_opts += ['-n', str(args.nof_ring), '\\']

    # IP address of SPP controller.
    ctrl_ip = os.getenv('SPP_CTRL_IP', args.ctrl_ip)
    if ctrl_ip is None:
        common.error_exit('SPP_CTRL_IP')
    else:
        spp_opts += ['-s', '%s:%d' % (ctrl_ip, args.ctrl_port), '\\']

    cmds = docker_cmd + docker_opts + spp_cmd + eal_opts + spp_opts
    if cmds[-1] == '\\':
        cmds.pop()
    common.print_pretty_commands(cmds)

    if args.dry_run is True:
        exit()

    # Remove delimiters for print_pretty_commands().
    while '\\' in cmds:
        cmds.remove('\\')

    subprocess.call(cmds)


if __name__ == '__main__':
    main()
