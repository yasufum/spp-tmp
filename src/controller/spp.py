#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015-2016 Intel Corporation

from __future__ import absolute_import

import argparse
from .shell import Shell
from . import spp_ctl_client
import sys


def main(argv):

    parser = argparse.ArgumentParser(description="SPP Controller")
    parser.add_argument('-b', '--bind-addr', type=str, default='127.0.0.1',
                        help='bind address, default=127.0.0.1')
    parser.add_argument('-a', '--api-port', type=int, default=7777,
                        help='bind address, default=7777')
    args = parser.parse_args()

    try:
        spp_ctl_cli = spp_ctl_client.SppCtlClient(args.bind_addr,
                                                  args.api_port)
        if spp_ctl_cli.is_server_running() is False:
            print('Is not spp-ctl running, nor correct IP address?')
            exit()
        shell = Shell(spp_ctl_cli)
        shell.cmdloop()
        shell = None
    except Exception as e:
        print(e)


if __name__ == "__main__":

    main(sys.argv[1:])
