#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

# Generate PDF images form SVG to embed targetting PDF document.

import os
import subprocess

DPI = 300  # resolution for export


def filter_list(alist, ext='svg'):
    """Filter files with given extension"""

    res = []
    for ent in alist:
        ent_ext = ent.split('.').pop()
        if ent_ext == ext:
            res.append(ent)
    return res


def main():
    work_dir = os.path.dirname(__file__)
    if work_dir == '':
        work_dir = '.'

    img_dir_info = os.walk('%s/images' % work_dir)
    for root, dirs, files in img_dir_info:
        if len(files) > 0:
            svg_files = filter_list(files)
            for fname in svg_files:
                # setup inkscape options
                tmp = fname.split('.')
                tmp.pop()
                base_fname = tmp[0]
                svg_f = base_fname + '.svg'
                pdf_f = base_fname + '.pdf'
                svg_fpath = '%s/%s' % (root, svg_f)
                pdf_fpath = '%s/%s' % (root, pdf_f)

                cmd = 'inkscape -d %d -D -f %s --export-pdf %s' % (
                    DPI, svg_fpath, pdf_fpath)
                print(cmd)
                subprocess.call(cmd, shell=True)


if __name__ == "__main__":
    main()
