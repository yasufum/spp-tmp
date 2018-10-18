#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015-2016 Intel Corporation

from __future__ import absolute_import

import argparse
from .shell import Shell
import sys


def main(argv):

    parser = argparse.ArgumentParser(description="SPP Controller")
    args = parser.parse_args()

    shell = Shell()
    shell.cmdloop()
    shell = None


if __name__ == "__main__":

    main(sys.argv[1:])
