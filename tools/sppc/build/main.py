#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

from __future__ import absolute_import
import argparse
import os
import subprocess
import sys

work_dir = os.path.dirname(__file__)
sys.path.append(work_dir + '/..')
from lib import common
from conf import env


def parse_args():
    parser = argparse.ArgumentParser(
        description="Docker image builder for DPDK applications")
    parser.add_argument(
        '-t', '--target',
        type=str,
        help="Build target ('dpdk', 'pktgen' or 'spp')")
    parser.add_argument(
        '-ci', '--container-image',
        type=str,
        help="Name of container image")
    parser.add_argument(
        '--dist-name',
        type=str,
        default='ubuntu',
        help="Name of Linux distribution")
    parser.add_argument(
        '--dist-ver',
        type=str,
        default='latest',
        help="Version of Linux distribution")
    parser.add_argument(
        '--dpdk-repo',
        type=str,
        default="http://dpdk.org/git/dpdk",
        help="Git URL of DPDK")
    parser.add_argument(
        '--dpdk-branch',
        type=str,
        help="Specific version or branch of DPDK")
    parser.add_argument(
        '--pktgen-repo',
        type=str,
        default="http://dpdk.org/git/apps/pktgen-dpdk",
        help="Git URL of pktgen-dpdk")
    parser.add_argument(
        '--pktgen-branch',
        type=str,
        help="Specific version or branch of pktgen-dpdk")
    parser.add_argument(
        '--spp-repo',
        type=str,
        default="http://dpdk.org/git/apps/spp",
        help="Git URL of SPP")
    parser.add_argument(
        '--spp-branch',
        type=str,
        help="Specific version or branch of SPP")
    parser.add_argument(
        '--only-envsh',
        action='store_true',
        help="Create config 'env.sh' and exit without docker build")
    parser.add_argument(
        '--dry-run',
        action='store_true',
        help="Print matrix for checking and exit without docker build")
    return parser.parse_args()


def create_env_sh(dst_dir):
    """Create config file for DPDK environment variables

    Create 'env.sh' which defines $RTE_SDK and $RTE_TARGET inside a
    container to be referredd from 'run.sh' and Dockerfile.
    """
    contents = "export RTE_SDK=%s\n" % env.RTE_SDK
    contents += "export RTE_TARGET=%s" % env.RTE_TARGET

    f = open('%s/env.sh' % dst_dir, 'w')
    f.write(contents)
    f.close()


def main():
    args = parse_args()

    if args.target is not None:
        target_dir = '%s/%s/%s' % (  # Dockerfile is contained here
            work_dir, args.dist_name, args.target)
        # Container image name, for exp 'sppc/dpdk-ubuntu:18.04'
        container_image = common.container_img_name(
            env.CONTAINER_IMG_NAME[args.target],
            args.dist_name,
            args.dist_ver)
    else:
        print("Error: Target '-t [dpdk|pktgen|spp]' is required!")
        exit()

    # Decide which of Dockerfile with given base image version
    dockerfile = '%s/Dockerfile.%s' % (target_dir, args.dist_ver)

    # Overwrite container's name if it is given.
    if args.container_image is not None:
        container_image = args.container_image

    # Setup branches if user specifies.
    if args.dpdk_branch is not None:
        dpdk_branch = "-b %s" % args.dpdk_branch
    else:
        dpdk_branch = ''

    if args.pktgen_branch is not None:
        pktgen_branch = "-b %s" % args.pktgen_branch
    else:
        pktgen_branch = ''

    if args.spp_branch is not None:
        spp_branch = "-b %s" % args.spp_branch
    else:
        spp_branch = ''

    # Check for just creating env.sh, or run docker build.
    if args.only_envsh is True:
        if args.dry_run is False:
            create_env_sh(target_dir)
            print("Info: '%s/env.sh' created." % target_dir)
            exit()
        else:
            print("Info: Nothin done because you gave %s with %s." % (
                '--only-envsh', '--dry-run'))
            exit()
    else:
        create_env_sh(target_dir)

    # Setup environment variables on host to pass 'docker build'.
    env_opts = [
        'http_proxy',
        'https_proxy',
        'no_proxy'
    ]

    docker_cmd = ['sudo', 'docker', 'build', '\\']

    for opt in env_opts:
        if opt in os.environ.keys():
            docker_cmd += [
                '--build-arg', '%s=%s' % (opt, os.environ[opt]), '\\']

    docker_cmd += [
        '--build-arg', 'home_dir=%s' % env.HOMEDIR, '\\',
        '--build-arg', 'rte_sdk=%s' % env.RTE_SDK, '\\',
        '--build-arg', 'rte_target=%s' % env.RTE_TARGET, '\\',
        '--build-arg', 'dpdk_repo=%s' % args.dpdk_repo, '\\',
        '--build-arg', 'dpdk_branch=%s' % dpdk_branch, '\\']

    if args.target == 'pktgen':
        docker_cmd += [
            '--build-arg', 'pktgen_repo=%s' % args.pktgen_repo, '\\',
            '--build-arg', 'pktgen_branch=%s' % pktgen_branch, '\\']
    elif args.target == 'spp':
        docker_cmd += [
            '--build-arg', 'spp_repo=%s' % args.spp_repo, '\\',
            '--build-arg', 'spp_branch=%s' % spp_branch, '\\']

    docker_cmd += [
        '-f', '%s' % dockerfile, '\\',
        '-t', container_image, '\\',
        target_dir]

    common.print_pretty_commands(docker_cmd)

    if args.dry_run is True:
        exit()

    # Remove delimiters for print_pretty_commands().
    while '\\' in docker_cmd:
        docker_cmd.remove('\\')
    subprocess.call(docker_cmd)


if __name__ == '__main__':
    main()
