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
        description="Launcher for spp-nfv application container")

    parser = app_helper.add_eal_args(parser)

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

    if args.container_image is not None:
        container_image = args.container_image
    else:
        # Container image name, for exp 'sppc/dpdk-ubuntu:18.04'
        container_image = common.container_img_name(
            env.CONTAINER_IMG_NAME[target_name],
            args.dist_name,
            args.dist_ver)

    docker_opts += [
        '--privileged', '\\',  # should be privileged
        '-v', '/dev/hugepages:/dev/hugepages', '\\',
        '-v', '/var/run/:/var/run/', '\\',
        '-v', '/tmp/:/tmp/', '\\',
        container_image, '\\'
    ]

    # This container is running in backgroud in defualt.
    if args.foreground is not True:
        docker_run_opt = '-d'
    else:
        docker_run_opt = '-it'

    # Setup spp_nfv command.
    spp_cmd = ['spp_nfv', '\\']

    # Do not use 'app_helper.setup_eal_opts()' because spp_nfv does
    # not use virtio.
    core_opt = app_helper.get_core_opt(args)
    mem_opt = app_helper.get_mem_opt(args)
    eal_opts = [
        core_opt['attr'], core_opt['val'], '\\',
        '-n', str(args.nof_memchan), '\\',
        mem_opt['attr'], mem_opt['val'], '\\',
        '--proc-type', 'secondary', '\\',
        '--', '\\']

    spp_opts = []
    # Check for other mandatory opitons.
    if args.sec_id is None:
        common.error_exit('--sec-id')
    else:
        spp_opts += ['-n', str(args.sec_id), '\\']

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
