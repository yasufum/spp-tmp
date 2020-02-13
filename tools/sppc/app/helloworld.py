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


def parse_args():
    parser = argparse.ArgumentParser(
        description="Launcher for l2fwd application container")

    parser = app_helper.add_eal_args(parser)
    parser = app_helper.add_appc_args(parser)

    # No application specific args for helloworld

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
    if args.dev_ids is None:
        common.error_exit('--dev-ids')

    # Setup for vhost devices with given device IDs.
    dev_ids_list = app_helper.dev_ids_to_list(args.dev_ids)
    sock_files = app_helper.sock_files(dev_ids_list)

    # Setup docker command.
    docker_cmd = ['sudo', 'docker', 'run', '\\']
    docker_opts = app_helper.setup_docker_opts(
        args, container_image, sock_files)

    # Setup helloworld run on container.
    cmd_path = '%s/examples/helloworld/%s/helloworld' % (
        env.RTE_SDK, env.RTE_TARGET)

    hello_cmd = [cmd_path, '\\']

    file_prefix = 'spp-hello-container%d' % dev_ids_list[0]
    eal_opts = app_helper.setup_eal_opts(args, file_prefix)

    # No application specific options for helloworld
    hello_opts = []

    cmds = docker_cmd + docker_opts + hello_cmd + eal_opts + hello_opts
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
