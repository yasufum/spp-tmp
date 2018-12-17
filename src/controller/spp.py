#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015-2016 Intel Corporation

from __future__ import absolute_import

import argparse
import re
from .shell import Shell
from . import spp_ctl_client
import sys


def main(argv):

    # Default
    api_ipaddr = '127.0.0.1'
    api_port = 7777

    parser = argparse.ArgumentParser(description="SPP Controller")
    parser.add_argument('-b', '--bind-addr', action='append',
                        default=['%s:%s' % (api_ipaddr, api_port)],
                        help='bind address, default=127.0.0.1:7777')
    args = parser.parse_args()

    if len(args.bind_addr) > 1:
        args.bind_addr.pop(0)

    spp_cli_objs = []
    for addr in args.bind_addr:
        if ':' in addr:
            api_ipaddr, api_port = addr.split(':')
        else:
            api_ipaddr = addr

        if not re.match(r'\d*\.\d*\.\d*\.\d*', addr):
            print('Invalid address "%s"' % args.bind_addr)
            exit()

        spp_ctl_cli = spp_ctl_client.SppCtlClient(api_ipaddr, int(api_port))

        if spp_ctl_cli.is_server_running() is False:
            print('Is not spp-ctl running on %s, nor correct IP address?' %
                  api_ipaddr)
            exit()

        spp_cli_objs.append(spp_ctl_cli)

    shell = Shell(spp_cli_objs)
    shell.cmdloop()
    shell = None


if __name__ == "__main__":

    main(sys.argv[1:])
