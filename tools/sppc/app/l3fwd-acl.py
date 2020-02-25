#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Nippon Telegraph and Telephone Corporation

import argparse
import os
import re
import subprocess
import sys

work_dir = os.path.dirname(__file__)
sys.path.append(work_dir + '/..')
from conf import env
from lib import app_helper
from lib import common


APP_NAME = 'l3fwd-acl'


def parse_args():
    parser = argparse.ArgumentParser(
        description="Launcher for l3fwd-acl application container")

    parser = app_helper.add_eal_args(parser)
    parser = app_helper.add_appc_args(parser)

    # Application specific args
    parser.add_argument(
        '-p', '--port-mask',
        type=str,
        help='(Mandatory) Port mask')
    parser.add_argument(
        '--config',
        type=str,
        help='(Mandatory) Define set of port, queue, lcore for ports')
    parser.add_argument(
        '-P', '--promiscous',
        action='store_true',
        help='Set all ports to promiscous mode (default is None)')
    parser.add_argument(
        '--rule_ipv4',
        type=str,
        help='Specifies the IPv4 ACL and route rules file')
    parser.add_argument(
        '--rule_ipv6',
        type=str,
        help='Specifies the IPv6 ACL and route rules file')
    parser.add_argument(
        '--scalar',
        action='store_true',
        help='Use a scalar function to perform rule lookup')
    parser.add_argument(
        '--enable-jumbo',
        action='store_true',
        help='Enable jumbo frames, [--enable-jumbo [--max-pkt-len PKTLEN]]')
    parser.add_argument(
        '--max-pkt-len',
        type=int,
        help='Max packet length (64-9600) if jumbo is enabled.')
    parser.add_argument(
        '--no-numa',
        action='store_true',
        help='Disable NUMA awareness (default is None)')

    parser = app_helper.add_sppc_args(parser)

    return parser.parse_args()


def check_config_format(config_opt, nof_queues):
    """Check if config format is valid.

    Config options is for Determining which queues from which ports
    are mapped to which cores.

      --config (port,queue,lcore)[,(port,queue,lcore)]

    Queue IDs should be started from 0 and sequential for each of
    ports. For example, '(0,1,0),(0,0,1)' is invlid because queue IDs
    of port 0 are '1, 0' and it does started from 1 and not sequential.

    The number of tx queues should be equal or less than the number of
    queues of virtio device defined with '-nq' or '--nof-queues' option.
    """

    config_opt = re.sub(r'\s+', '', config_opt)

    config_opt = config_opt.lstrip('(').rstrip(')')
    nof_tx_queues = len(config_opt.split('),('))

    ptn = re.compile(r'(\d+)\,(\d+)\,(\d+)')
    port_conf = {}
    for c in config_opt.split('),('):
        m = re.match(ptn, c)
        port_idx = int(m.group(1))
        if port_conf.get(port_idx) is None:
            port_conf[port_idx] = [[int(m.group(2)), int(m.group(3))]]
        else:
            port_conf[port_idx].append([int(m.group(2)), int(m.group(3))])

    for attrs in port_conf.values():
        i = 0
        for attr in attrs:
            if attr[0] != i:
                print('Error: queue ids should be started from 0 ' +
                      'and sequential!')
                return False
            i = i + 1

    if nof_tx_queues > nof_queues:
        print('Error: {0:s}={1:d} should be equal or less than {2:s}={3:d}!'.
              format('tx_queues', nof_tx_queues, 'nof_queues', nof_queues))
        print("\tnof_queues is defiend with '-nq' or '--nof-queues' option")
        return False

    return True


def check_jumbo_opt(enable_jumbo, max_pkt_len):
    """Check if jumbo frame option is valid.

    Jumbo frame is enabled with '--enable-jumbo' and max packet size is
    defined with '--max-pkt-len'.
    '--max-pkt-len' cannot be used without '--enable-jumbo'.
    """

    if (enable_jumbo is None) and (max_pkt_len is not None):
        print('Error: --enable-jumbo is required')
        return False

    if max_pkt_len is not None:
        if (max_pkt_len < 64) or (max_pkt_len > 9600):
            print('Error: --max-pkt-len {0:d} should be {1:d}-{2:d}'.
                  format(max_pkt_len, 64, 9600))
            return False

    return True


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

    # Check given number of ports is enough for portmask.
    if (args.port_mask is None) or (args.dev_uids is None):
        pass
    elif app_helper.is_sufficient_ports(args) is not True:
        print("Error: Not enough ports, {0:d} devs for '{1:s}(=0b{2:s})'.".
              format(len(args.dev_uids.split(',')), args.port_mask,
                     format(int(args.port_mask, 16), 'b')))
        exit()

    # Setup l3fwd-acl command runs on container.
    cmd_path = '{0:s}/examples/{2:s}/{1:s}/{2:s}'.format(
        env.RTE_SDK, env.RTE_TARGET, APP_NAME)

    l3fwd_cmd = [cmd_path, '\\']

    # Setup EAL options.
    eal_opts = app_helper.setup_eal_opts(args, APP_NAME)

    # Setup l3fwd-acl options.
    l3fwd_opts = ['-p', args.port_mask, '\\']

    if args.config is None:
        common.error_exit('--config')
    elif check_config_format(args.config, args.nof_queues) is not True:
        print('Invalid config: {}'.format(args.config))
        exit()
    else:
        l3fwd_opts += ['--config', '"{:s}"'.format(args.config), '\\']

    jumbo_opt_valid = False
    if args.enable_jumbo is True:
        jumbo_opt_valid = check_jumbo_opt(
            args.enable_jumbo, args.max_pkt_len)
        if jumbo_opt_valid is False:
            print('Error: invalid jumbo frame option(s)')
            exit()

    if args.rule_ipv4 is not None:
        if os.path.exists(args.rule_ipv4):
            l3fwd_opts += ['--rule_ipv4', '"{:s}"'.format(args.rule_ipv4),
                           '\\']
        elif args.dry_run is not True:
            print('Error: "{}" does not exist'.format(args.rule_ipv4))
            exit()
    if args.rule_ipv6 is not None:
        if os.path.exists(args.rule_ipv6):
            l3fwd_opts += ['--rule_ipv6', '"{:s}"'.format(args.rule_ipv6),
                           '\\']
        elif args.dry_run is not True:
            print('Error: "{}" does not exist'.format(args.rule_ipv6))
            exit()

    if args.promiscous is True:
        l3fwd_opts += ['-P', '\\']
    if args.scalar is True:
        l3fwd_opts += ['--scalar', '\\']
    if (args.enable_jumbo is not None) and (jumbo_opt_valid is True):
        l3fwd_opts += ['--enable-jumbo', '\\']
        if args.max_pkt_len is not None:
            l3fwd_opts += ['--max-pkt-len {:d}'.format(args.max_pkt_len), '\\']
    if args.no_numa is True:
        l3fwd_opts += ['--no-numa', '\\']

    cmds = docker_cmd + docker_opts + [container_image, '\\'] + \
        l3fwd_cmd + eal_opts + l3fwd_opts
    if cmds[-1] == '\\':
        cmds.pop()
    common.print_pretty_commands(cmds)

    if args.dry_run is True:
        exit()

    # Remove delimiters for print_pretty_commands().
    while '\\' in cmds:
        cmds.remove('\\')
    subprocess.call(' '.join(cmds), shell=True)


if __name__ == '__main__':
    main()
