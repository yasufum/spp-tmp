#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

import argparse
import os
import subprocess
import sys

work_dir = os.path.dirname(__file__)
sys.path.append(work_dir + '/..')
from lib import app_helper
from lib import common


def parse_args():
    parser = argparse.ArgumentParser(
        description="Launcher for spp-primary application container")

    parser = app_helper.add_eal_args(parser)
    parser = app_helper.add_appc_args(parser)

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
        '-ip', '--ctl-ip',
        type=str,
        help="IP address of spp-ctl")
    parser.add_argument(
        '--ctl-port',
        type=int,
        default=5555,
        help="Port for primary of spp-ctl")

    parser = app_helper.add_sppc_args(parser)
    return parser.parse_args()


def main():
    args = parse_args()

    app_name = 'spp_primary'

    # Setup docker command.
    docker_cmd = ['sudo', 'docker', 'run', '\\']

    # Container image name such as 'sppc/spp-ubuntu:18.04'
    if args.container_image is not None:
        container_image = args.container_image
    else:
        container_image = common.container_img_name(
            common.IMG_BASE_NAMES['spp'],
            args.dist_name, args.dist_ver)

    app_opts = [
        '-v', '/var/run/:/var/run/', '\\',
        '-v', '/tmp:/tmp', '\\']

    # Use host network if attaching TAP device to show them on the host.
    for dev_uid in args.dev_uids.split(','):
        if 'tap' in dev_uid:
            app_opts += ['--net', 'host', '\\']

    docker_opts = app_helper.setup_docker_opts(
            args, None, app_opts)

    # Setup spp primary command.
    spp_cmd = [app_name, '\\']

    eal_opts = app_helper.setup_eal_opts(args, app_name=None,
                                         proc_type='primary', is_spp_pri=True)

    spp_opts = []
    # Check for other mandatory opitons.
    if args.port_mask is None:
        common.error_exit('port_mask')
    else:
        spp_opts += ['-p', args.port_mask, '\\']

    spp_opts += ['-n', str(args.nof_ring), '\\']

    # IP address of spp-ctl.
    ctl_ip = os.getenv('SPP_CTL_IP', args.ctl_ip)
    if ctl_ip is None:
        print('Env variable "SPP_CTL_IP" is not defined!')
        exit()
    else:
        spp_opts += ['-s', '{}:{}'.format(ctl_ip, args.ctl_port), '\\']

    cmds = docker_cmd + docker_opts + [container_image, '\\'] + \
        spp_cmd + eal_opts + spp_opts

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
