#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015-2016 Intel Corporation

import logging
import os

# Type definitions.
PORT_TYPES = ['phy', 'ring', 'vhost', 'pcap', 'memif', 'nullpmd']
SEC_TYPES = ['nfv', 'vf', 'mirror', 'pcap']

# Setup logger object
# Logfile is generated as 'spp/log/spp_cli.log'.
log_filename = 'spp_cli.log'  # name of logfile
logdir = '{}/../../log'.format(os.path.dirname(__file__))
logfile = '{}/{}'.format(logdir, log_filename)

os.system('mkdir -p {}'.format(logdir))

# handler = logging.StreamHandler()
handler = logging.FileHandler(logfile)
handler.setLevel(logging.DEBUG)
formatter = logging.Formatter(
    '%(asctime)s,[%(filename)s][%(name)s][%(levelname)s]%(message)s')
handler.setFormatter(formatter)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)
logger.addHandler(handler)

# Current server under management of SPP CLI.
cur_server_addr = None
