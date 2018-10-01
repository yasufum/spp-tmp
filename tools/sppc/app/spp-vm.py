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
        description="Launcher for spp-vm application container")

    parser = app_helper.add_eal_args(parser)
    parser = app_helper.add_appc_args(parser)

    # Application specific arguments
    parser.add_argument(
        '-i', '--sec-id',
        type=int,
        help='Secondary ID')
    parser.add_argument(
        '-ip', '--ctrl-ip',
        type=str,
        help="IP address of SPP controller")
    parser.add_argument(
        '--ctrl-port',
        type=int,
        default=6666,
        help="Port of SPP controller")
    parser.add_argument(
        '-p', '--port-mask',
        type=str,
        help="Port mask")

    parser = app_helper.add_sppc_args(parser)

    return parser.parse_args()


def main():
    args = parse_args()

    # Check for other mandatory opitons.
    if args.sec_id is None:
        common.error_exit('--sec-id')

    if args.dev_ids is None:
        common.error_exit('--dev-ids')

    if args.port_mask is None:
        common.error_exit('--port-mask')

    # Setup for vhost devices with given device IDs.
    dev_ids_list = app_helper.dev_ids_to_list(args.dev_ids)
    sock_files = app_helper.sock_files(dev_ids_list)

    # Setup docker command.
    docker_cmd = ['sudo', 'docker', 'run', '\\']
    docker_opts = app_helper.setup_docker_opts(
        args, target_name, sock_files)

    # IP address of SPP controller.
    ctrl_ip = os.getenv('SPP_CTRL_IP', args.ctrl_ip)
    if ctrl_ip is None:
        common.error_exit('SPP_CTRL_IP')

    # Setup spp_vm command.
    spp_cmd = ['spp_vm', '\\']

    file_prefix = 'spp-l2fwd-container%d' % dev_ids_list[0]
    eal_opts = app_helper.setup_eal_opts(args, file_prefix)

    spp_opts = [
        '-n', str(args.sec_id), '\\',
        '-p', args.port_mask, '\\',
        '-s', '%s:%d' % (ctrl_ip, args.ctrl_port)
    ]

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
