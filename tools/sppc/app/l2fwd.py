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


APP_NAME = 'l2fwd'


def parse_args():
    parser = argparse.ArgumentParser(
        description="Launcher for l2fwd application container")

    parser = app_helper.add_eal_args(parser)
    parser = app_helper.add_appc_args(parser)

    # Application specific args
    parser.add_argument(
        '-p', '--port-mask',
        type=str,
        help="Port mask")

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

    # Check for other mandatory opitons.
    if args.port_mask is None:
        common.error_exit('--port-mask')

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

    # Check if the number of ports is even for l2fwd.
    nof_ports = app_helper.count_ports(args.port_mask)
    if (nof_ports % 2) != 0:
        print("Error: Number of ports must be an even number!")
        exit()

    # Setup l2fwd command run on container.
    cmd_path = '{0:s}/examples/{2:s}/{1:s}/{2:s}'.format(
        env.RTE_SDK, env.RTE_TARGET, APP_NAME)

    l2fwd_cmd = [cmd_path, '\\']

    # Setup EAL options.
    eal_opts = app_helper.setup_eal_opts(args, APP_NAME)

    # Setup l2fwd options.
    l2fwd_opts = ['-p', args.port_mask, '\\']

    # Check given number of ports is enough for portmask.
    if (args.port_mask is None) or (args.dev_uids is None):
        pass
    elif app_helper.is_sufficient_ports(args) is not True:
        print("Error: Not enough ports, {0:d} devs for '{1:s}(=0b{2:s})'.".
              format(len(args.dev_uids.split(',')), args.port_mask,
                     format(int(args.port_mask, 16), 'b')))
        exit()

    cmds = docker_cmd + docker_opts + [container_image, '\\'] + \
        l2fwd_cmd + eal_opts + l2fwd_opts
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
