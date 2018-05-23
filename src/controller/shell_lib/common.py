#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation

import os


def decorate_dir(curdir, filelist):
    """Add '/' the end of dirname for path completion

    'filelist' is a list of files contained in a directory.
    """

    res = []
    for f in filelist:
        if os.path.isdir('%s/%s' % (curdir, f)):
            res.append('%s/' % f)
        else:
            res.append(f)
    return res


def compl_common(text, line, ftype=None):
    """File path completion for 'complete_*' method

    This method is called from 'complete_*' to complete 'do_*'.
    'text' and 'line' are arguments of 'complete_*'.

    `complete_*` is a member method of builtin Cmd class and
    called if tab key is pressed in a command defiend by 'do_*'.
    'text' and 'line' are contents of command line.
    For example, if you type tab at 'command arg1 ar',
    last token 'ar' is assigned to 'text' and whole line
    'command arg1 ar' is assigned to 'line'.

    NOTE:
    If tab is typed after '/', empty text '' is assigned to
    'text'. For example 'aaa b/', text is not 'b/' but ''.
    """

    if text == '':  # tab is typed after command name or '/'
        tokens = line.split(' ')
        target_dir = tokens[-1]  # get dirname for competion
        if target_dir == '':  # no dirname means current dir
            res = decorate_dir(
                '.', os.listdir(os.getcwd()))
        else:  # after '/'
            res = decorate_dir(
                target_dir, os.listdir(target_dir))
    else:  # tab is typed in the middle of a word
        tokens = line.split(' ')
        target = tokens[-1]  # target dir for completion

        if '/' in target:  # word is a path such as 'path/to/file'
            seg = target.split('/')[-1]  # word to be completed
            target_dir = '/'.join(target.split('/')[0:-1])
        else:
            seg = text
            target_dir = os.getcwd()

        matched = []
        for t in os.listdir(target_dir):
            if t.find(seg) == 0:  # get words matched with 'seg'
                matched.append(t)
        res = decorate_dir(target_dir, matched)

    if ftype is not None:  # filtering by ftype
        completions = []
        if ftype == 'directory':
            for fn in res:
                if fn[-1] == '/':
                    completions.append(fn)
        elif ftype == 'py' or ftype == 'python':
            for fn in res:
                if fn[-3:] == '.py':
                    completions.append(fn)
        elif ftype == 'file':
            for fn in res:
                if fn[-1] != '/':
                    completions.append(fn)
        else:
            completions = res
    else:
        completions = res
    return completions


def is_comment_line(line):
    """Find commend line to not to interpret as a command

    Return True if given line is a comment, or False.
    Supported comment styles are
      * python ('#')
      * C ('//')
    """

    input_line = line.strip()
    if len(input_line) > 0:
        if (input_line[0] == '#') or (input_line[0:2] == '//'):
            return True
        else:
            return False
