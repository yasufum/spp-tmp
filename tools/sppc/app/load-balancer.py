#!/usr/bin/env python3
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


APP_NAME = 'load_balancer'


def parse_args():
    parser = argparse.ArgumentParser(
        description="Launcher for load-balancer application container")

    parser = app_helper.add_eal_args(parser)
    parser = app_helper.add_appc_args(parser)

    # Application specific args
    parser.add_argument(
        '-rx', '--rx-ports',
        type=str,
        help="List of rx ports and queues handled by the I/O rx lcores")
    parser.add_argument(
        '-tx', '--tx-ports',
        type=str,
        help="List of tx ports and queues handled by the I/O tx lcores")
    parser.add_argument(
        '-wl', '--worker-lcores',
        type=str,
        help="List of worker lcores")
    parser.add_argument(
        '-rsz', '--ring-sizes',
        type=str,
        help="Ring sizes of 'rx_read,rx_send,w_send,tx_written'")
    parser.add_argument(
        '-bsz', '--burst-sizes',
        type=str,
        help="Burst sizes of rx, worker or tx")
    parser.add_argument(
        '--lpm',
        type=str,
        help="List of LPM rules")
    parser.add_argument(
        '--pos-lb',
        type=int,
        help="Position of the 1-byte field used for identify worker")

    parser = app_helper.add_sppc_args(parser)
    return parser.parse_args()


def main():
    args = parse_args()

    # Container image name such as 'sppc/dpdk-ubuntu:18.04'
    if args.container_image is not None:
        container_image = args.container_image
    else:
        container_image = common.container_img_name(
            common.IMG_BASE_NAMES['dpdk'],
            args.dist_name, args.dist_ver)

    c_dpdk_ver = app_helper.get_dpdk_ver_in_container(
            env.RTE_SDK, container_image)
    expected = '19.08-rc1'
    if app_helper.compare_version(expected, c_dpdk_ver) > 0:
        print("Load-balancer example was removed after DPDK 'v{}'.".
              format(expected))
        print("You cannot run it because DPDK in the container is 'v{}'.".
              format(c_dpdk_ver))
        exit()

    # Setup devices with given device UIDs.
    dev_uids = None
    sock_files = None
    if args.dev_uids is not None:
        if app_helper.is_valid_dev_uids(args.dev_uids) is False:
            print('Invalid option: {}'.format(args.dev_uids))
            exit()

        dev_uids_list = args.dev_uids.split(',')
        sock_files = app_helper.sock_files(dev_uids_list)

    # Setup docker command.
    docker_cmd = ['sudo', 'docker', 'run', '\\']
    docker_opts = app_helper.setup_docker_opts(args, sock_files)

    cmd_path = '{0:s}/examples/{2:s}/{1:s}/{2:s}'.format(
        env.RTE_SDK, env.RTE_TARGET, APP_NAME)

    # Setup testpmd command.
    lb_cmd = [cmd_path, '\\']

    # Setup EAL options.
    eal_opts = app_helper.setup_eal_opts(args, APP_NAME)

    lb_opts = []

    # Check for other mandatory opitons.
    if args.rx_ports is None:
        common.error_exit('--rx-ports')
    else:
        lb_opts += ['--rx', '"{:s}"'.format(args.rx_ports), '\\']

    if args.tx_ports is None:
        common.error_exit('--tx-ports')
    else:
        lb_opts += ['--tx', '"{:s}"'.format(args.tx_ports), '\\']

    if args.worker_lcores is None:
        common.error_exit('--worker-lcores')
    else:
        lb_opts += ['--w', '{:s}'.format(args.worker_lcores), '\\']

    if args.lpm is None:
        common.error_exit('--lpm')
    else:
        lb_opts += ['--lpm', '"{:s}"'.format(args.lpm), '\\']

    # Check optional opitons.
    if args.ring_sizes is not None:
        lb_opts += ['--ring-sizes', args.ring_sizes, '\\']
    if args.burst_sizes is not None:
        lb_opts += ['--burst-sizes', args.burst_sizes, '\\']
    if args.pos_lb is not None:
        lb_opts += ['--pos-lb', str(args.pos_lb)]

    cmds = docker_cmd + docker_opts + [container_image, '\\'] + \
        lb_cmd + eal_opts + lb_opts
    if cmds[-1] == '\\':
        cmds.pop()
    common.print_pretty_commands(cmds)

    if args.dry_run is True:
        exit()

    # Remove delimiters for print_pretty_commands().
    while '\\' in cmds:
        cmds.remove('\\')
    # Call with shell=True because parsing '--w' option is failed
    # without it.
    subprocess.call(' '.join(cmds), shell=True)


if __name__ == '__main__':
    main()
