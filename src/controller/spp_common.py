#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015-2016 Intel Corporation

import logging
import os

PORT_TYPES = ['phy', 'ring', 'vhost', 'pcap', 'nullpmd']

# Maximum num of sock queues for secondaries
MAX_SECONDARY = 16

# Setup logger object
logger = logging.getLogger(__name__)
# handler = logging.StreamHandler()
os.system("mkdir -p %s/log" % (os.path.dirname(__file__)))

logfile = '%s/log/%s' % (os.path.dirname(__file__), 'spp.log')
handler = logging.FileHandler(logfile)
handler.setLevel(logging.DEBUG)
formatter = logging.Formatter(
    '%(asctime)s,[%(filename)s][%(name)s][%(levelname)s]%(message)s')
handler.setFormatter(formatter)
logger.setLevel(logging.DEBUG)
logger.addHandler(handler)
