#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015-2016 Intel Corporation

from __future__ import absolute_import
from controller import spp
import sys

if __name__ == "__main__":

    spp.main(sys.argv[1:])
