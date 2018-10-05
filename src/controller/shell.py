#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015-2016 Intel Corporation

from __future__ import absolute_import

import cmd
import json
import os
from queue import Empty
import re
import readline
from .shell_lib import common
from . import spp_common
from .spp_common import logger
import subprocess
from . import topo


class Shell(cmd.Cmd, object):
    """SPP command prompt"""

    hist_file = '.spp_history'

    intro = 'Welcome to the spp.   Type help or ? to list commands.\n'
    prompt = 'spp > '
    recorded_file = None

    CMD_OK = "OK"
    CMD_NG = "NG"
    CMD_NOTREADY = "NOTREADY"
    CMD_ERROR = "ERROR"

    PORT_TYPES = ['phy', 'ring', 'vhost', 'pcap', 'nullpmd']

    PRI_CMDS = ['status', 'exit', 'clear']
    SEC_CMDS = ['status', 'exit', 'forward', 'stop', 'add', 'patch', 'del']
    SEC_SUBCMDS = ['vhost', 'ring', 'pcap', 'nullpmd']
    BYE_CMDS = ['sec', 'all']

    HIST_EXCEPT = ['bye', 'exit', 'history', 'redo']

    PLUGIN_DIR = 'command'
    subgraphs = {}
    topo_size = '60%'

    # setup history file
    if os.path.exists(hist_file):
        readline.read_history_file(hist_file)
    else:
        readline.write_history_file(hist_file)

    def default(self, line):
        """Define defualt behaviour

        If user input is commend styled, controller simply echo
        as a comment.
        """

        if common.is_comment_line(line):
            print("%s" % line.strip())

        else:
            super(Shell, self).default(line)

    def emptyline(self):
        """Do nothin for empty input

        It override Cmd.emptyline() which runs previous input as default
        to do nothing.
        """
        pass

    def clean_hist_file(self):
        """Remove useless entries in history file."""

        entries = []

        try:
            for line in open(self.hist_file):
                l = line.strip()
                if not (l.split(' ')[0] in self.HIST_EXCEPT):
                    entries.append(l)
            f = open(self.hist_file, "w+")
            contents = '\n'.join(entries)
            contents += '\n'
            f.write(contents)
            f.close()
        except IOError:
            print('Error: Cannot open history file "%s"' %
                    self.hist_file)

    def close_all_secondary(self):
        """Terminate all secondary processes"""

        tmp_list = []
        for i in spp_common.SECONDARY_LIST:
            tmp_list.append(i)
        for i in tmp_list:
            self.command_secondary(i, 'exit')
        spp_common.SECONDARY_COUNT = 0

    def get_status(self):
        """Return status of primary and secondary processes

        It is called from do_status() method and return primary status
        and a list of secondary processes as status.
        """

        secondary = []
        for i in spp_common.SECONDARY_LIST:
            secondary.append("%d" % i)
        stat = {
            # PRIMARY is 1 if it is running
            "primary": "%d" % spp_common.PRIMARY,
            "secondary": secondary
            }
        return stat

    def print_status(self):
        """Display information about connected clients"""

        print("Soft Patch Panel Status :")
        print("primary: %d" % spp_common.PRIMARY)  # it is 1 if PRIMA == True
        print("secondary count: %d" % len(spp_common.SECONDARY_LIST))
        for i in spp_common.SECONDARY_LIST:
            print("Connected secondary id: %d" % i)

    def print_pri_status(self, json_obj):
        """Parse SPP primary's status and print.

        Primary returns the status as JSON format, but it is just a little
        long.

            {
                "phy_ports": [
                    {
                        "eth": "56:48:4f:12:34:00",
                        "id": 0,
                        "rx": 78932932,
                        "tx": 78932931,
                        "tx_drop": 1,
                    }
                    ...
                ],
                "ring_ports": [
                    {
                        "id": 0,
                        "rx": 89283,
                        "rx_drop": 0,
                        "tx": 89283,
                        "tx_drop": 0
                    },
                    ...
                ]
            }

        It is formatted to be simple and more understandable.

            Physical Ports:
              ID          rx          tx     tx_drop  mac_addr
               0    78932932    78932931           1  56:48:4f:53:54:00
            Ring Ports:
              ID          rx          tx     rx_drop     rx_drop
               0       89283       89283           0           0
               ...
        """

        if json_obj.has_key('phy_ports'):
            print('Physical Ports:')
            print('  ID          rx          tx     tx_drop  mac_addr')
            for pports in json_obj['phy_ports']:
                print('  %2d  %10d  %10d  %10d  %s' % (
                    pports['id'], pports['rx'],  pports['tx'],
                    pports['tx_drop'], pports['eth']))

        if json_obj.has_key('ring_ports'):
            print('Ring Ports:')
            print('  ID          rx          tx     rx_drop     rx_drop')
            for rports in json_obj['ring_ports']:
                print('  %2d  %10d  %10d  %10d  %10d' % (
                    rports['id'], rports['rx'],  rports['tx'],
                    rports['rx_drop'], rports['tx_drop']))

    def print_sec_status(self, msg):
        """Parse and print message from SPP secondary.

        Print status received from secondary.

          spp > sec 1;status
          - status: idling
          - ports:
            - phy:0 -> ring:0
            - phy:1

        The format of the received message is JSON and ended with
        series of null character "\x00". The value of "ports" attribute
        is a set of combinations of patches. If a port is not patched,
        the "dst" is set to "null".

          {"status":"idling","ports":[{"src":"phy:0", "dst": ...,]}'\x00\x00..
        """

        msg = msg.replace("\x00", "")  # clean sec's msg

        try:
            sec_attr = json.loads(msg)
            print('- status: %s' % sec_attr['status'])
            print('- ports:')
            for port in sec_attr['ports']:
                if port['dst'] == 'null':
                    print('  - %s' % port['src'])
                else:
                    print('  - %s -> %s' % (port['src'], port['dst']))
        except ValueError as err:
            print('Invalid format: {0}.'.format(err))
            print("  '%s'" % msg)

    def command_primary(self, command):
        """Send command to primary process"""

        if spp_common.PRIMARY:
            spp_common.MAIN2PRIMARY.put(command.encode('utf-8'))
            recv = spp_common.PRIMARY2MAIN.get(True)
            json_obj = json.loads(recv)
            self.print_pri_status(json_obj)
            return self.CMD_OK, recv
        else:
            recv = "primary not started"
            print(recv)
            return self.CMD_NOTREADY, recv

    def command_secondary(self, sec_id, command):
        """Send command to secondary process with sec_id"""

        if sec_id in spp_common.SECONDARY_LIST:
            spp_common.MAIN2SEC[sec_id].put(command.encode('utf-8'))
            recv = spp_common.SEC2MAIN[sec_id].get(True)
            if command == 'status':
                self.print_sec_status(recv)
            else:
                print(recv)
            return self.CMD_OK, recv
        else:
            message = "secondary id %d not exist" % sec_id
            print(message)
            return self.CMD_NOTREADY, message

    def is_patched_ids_valid(self, id1, id2, delim=':'):
        """Check if port IDs are valid

        Supported format is port ID of integer or resource ID such as
        'phy:0' or 'ring:1'. Default delimiter ':' can be overwritten
        by giving 'delim' option.
        """

        if str.isdigit(id1) and str.isdigit(id2):
            return True
        else:
            ptn = r"\w+\%s\d+" % delim  # Match "phy:0" or "ring:1" or so
            if re.match(ptn, id1) and re.match(ptn, id2):
                pt1 = id1.split(delim)[0]
                pt2 = id2.split(delim)[0]
                if (pt1 in self.PORT_TYPES) and (pt2 in self.PORT_TYPES):
                    return True
        return False

    def check_sec_cmds(self, cmds):
        """Validate secondary commands before sending"""

        level1 = ['status', 'exit', 'forward', 'stop']
        level2 = ['add', 'patch', 'del']
        patch_args = ['reset']
        add_del_args = ['ring', 'vhost', 'pcap', 'nullpmd']
        cmdlist = cmds.split(' ')
        valid = 0

        length = len(cmdlist)
        if length == 1:
            if cmdlist[0] in level1:
                valid = 1
        elif length == 2:
            if cmdlist[0] == 'patch':
                if cmdlist[1] in patch_args:
                    valid = 1
        elif length == 3:
            if cmdlist[0] in level2:
                if cmdlist[0] == 'add' or cmdlist[0] == 'del':
                    if cmdlist[1] in add_del_args:
                        if str.isdigit(cmdlist[2]):
                            valid = 1
                elif cmdlist[0] == 'patch':
                    if self.is_patched_ids_valid(cmdlist[1], cmdlist[2]):
                        valid = 1

        return valid

    def clean_cmd(self, cmdstr):
        """remove unwanted spaces to avoid invalid command error"""

        tmparg = re.sub(r'\s+', " ", cmdstr)
        res = re.sub(r'\s?;\s?', ";", tmparg)
        return res

    def response(self, result, message):
        """Enqueue message from other than CLI"""

        try:
            rcmd = spp_common.RCMD_EXECUTE_QUEUE.get(False)
        except Empty:
            return

        if (rcmd == spp_common.REMOTE_COMMAND):
            param = result + '\n' + message
            spp_common.RCMD_RESULT_QUEUE.put(param.encode('utf-8'))
        else:
            if logger is not None:
                logger.debug("unknown remote command = %s" % rcmd)

    def precmd(self, line):
        """Called before running a command

        It is called for checking a contents of command line.
        """

        if self.recorded_file:
            if not (
                    ('playback' in line) or
                    ('bye' in line) or
                    ('exit' in line)):
                self.recorded_file.write("%s\n" % line)
        return line

    def close(self):
        """Close record file"""

        if self.recorded_file:
            print("closing file")
            self.recorded_file.close()
            self.recorded_file = None

    def do_status(self, _):
        """Display status info of SPP processes

        spp > status
        """

        self.print_status()
        stat = self.get_status()
        self.response(self.CMD_OK, json.dumps(stat))

    def do_pri(self, command):
        """Send command to primary process

        Spp primary takes sub commands.

        spp > pri;status
        spp > pri;clear
        """

        # Remove unwanted spaces and first char ';'
        command = self.clean_cmd(command)[1:]

        if logger is not None:
            logger.info("Receive pri command: '%s'" % command)

        if command and (command in self.PRI_CMDS):
            result, message = self.command_primary(command)
            self.response(result, message)
        else:
            message = "Invalid pri command: '%s'" % command
            print(message)
            self.response(self.CMD_ERROR, message)

    def complete_pri(self, text, line, begidx, endidx):
        """Completion for primary process commands"""

        if not text:
            completions = self.PRI_CMDS[:]
        else:
            completions = [p
                           for p in self.PRI_CMDS
                           if p.startswith(text)
                           ]
        return completions

    def do_sec(self, arg):
        """Send command to secondary process

        SPP secondary process is specified with secondary ID and takes
        sub commands.

        spp > sec 1;status
        spp > sec 1;add ring 0
        spp > sec 1;patch 0 2
        """

        # remove unwanted spaces to avoid invalid command error
        tmparg = self.clean_cmd(arg)
        cmds = tmparg.split(';')
        if len(cmds) < 2:
            message = "error"
            print(message)
            self.response(self.CMD_ERROR, message)
        elif str.isdigit(cmds[0]):
            sec_id = int(cmds[0])
            if self.check_sec_cmds(cmds[1]):
                result, message = self.command_secondary(sec_id, cmds[1])
                self.response(result, message)
            else:
                message = "invalid cmd"
                print(message)
                self.response(self.CMD_ERROR, message)
        else:
            print(cmds[0])
            print("first %s" % cmds[1])
            self.response(self.CMD_ERROR, "invalid format")

    def complete_sec(self, text, line, begidx, endidx):
        """Completion for secondary process commands"""

        try:
            cleaned_line = self.clean_cmd(line)
            if len(cleaned_line.split()) == 1:
                completions = [str(i)+";" for i in spp_common.SECONDARY_LIST]
            elif len(cleaned_line.split()) == 2:
                if not (";" in cleaned_line):
                    tmplist = [str(i) for i in spp_common.SECONDARY_LIST]
                    completions = [p+";"
                                   for p in tmplist
                                   if p.startswith(text)
                                   ]
                elif cleaned_line[-1] == ";":
                    completions = self.SEC_CMDS[:]
                else:
                    seccmd = cleaned_line.split(";")[1]
                    if cleaned_line[-1] != " ":
                        completions = [p
                                       for p in self.SEC_CMDS
                                       if p.startswith(seccmd)
                                       ]
                    elif ("add" in seccmd) or ("del" in seccmd):
                        completions = self.SEC_SUBCMDS[:]
                    else:
                        completions = []
            elif len(cleaned_line.split()) == 3:
                subcmd = cleaned_line.split()[-1]
                if ("add" == subcmd) or ("del" == subcmd):
                    completions = self.SEC_SUBCMDS[:]
                else:
                    if cleaned_line[-1] == " ":
                        completions = []
                    else:
                        completions = [p
                                       for p in self.SEC_SUBCMDS
                                       if p.startswith(subcmd)
                                       ]
            else:
                completions = []
            return completions
        except Exception as e:
            print(len(cleaned_line.split()))
            print(e)

    def do_record(self, fname):
        """Save commands to a log file

        Save command history to a log file for loading from playback
        command later as a config file.
        Config is a series of SPP command and you can also create it
        from scratch without playback command.

        spp > record path/to/file
        """

        if fname == '':
            print("Record file is required!")
        else:
            self.recorded_file = open(fname, 'w')
            self.response(self.CMD_OK, "record")

    def complete_record(self, text, line, begidx, endidx):
        return common.compl_common(text, line)

    def do_playback(self, fname):
        """Load a config file to reproduce network configuration

        Config is a series of SPP command and you can also create it
        from scratch without playback command.

        spp > playback path/to/config
        """

        if fname == '':
            print("Record file is required!")
        else:
            self.close()
            try:
                with open(fname) as recorded_file:
                    lines = []
                    for line in recorded_file:
                        if not common.is_comment_line(line):
                            lines.append("# %s" % line)
                        lines.append(line)
                    self.cmdqueue.extend(lines)
                    self.response(self.CMD_OK, "playback")
            except IOError:
                message = "Error: File does not exist."
                print(message)
                self.response(self.CMD_NG, message)

    def complete_playback(self, text, line, begidx, endidx):
        return common.compl_common(text, line)

    def do_pwd(self, args):
        """Show corrent directory

        It behaves as UNIX's pwd command.

        spp > pwd
        """

        print(os.getcwd())

    def do_ls(self, args):
        """Show a list of specified directory

        It behaves as UNIX's ls command.

        spp > ls path/to/dir
        """

        if args == '' or os.path.isdir(args):
            c = 'ls -F %s' % args
            subprocess.call(c, shell=True)
        else:
            print("No such a directory.")

    def complete_ls(self, text, line, begidx, endidx):
        return common.compl_common(text, line)

    def do_cd(self, args):
        """Change current directory

        spp > cd path/to/dir
        """

        if os.path.isdir(args):
            os.chdir(args)
            print(os.getcwd())
        else:
            print("No such a directory.")

    def complete_cd(self, text, line, begidx, endidx):
        return common.compl_common(text, line, 'directory')

    def do_mkdir(self, args):
        """Create a new directory

        It behaves as 'mkdir -p'.

        spp > mkdir path/to/dir
        """

        c = 'mkdir -p %s' % args
        subprocess.call(c, shell=True)

    def complete_mkdir(self, text, line, begidx, endidx):
        return common.compl_common(text, line)

    def do_bye(self, arg):
        """Terminate SPP processes and controller

        It also terminates logging if you activate recording.

        (1) Terminate secondary processes
        spp > bye sec

        (2) Terminate primary and secondary processes
        spp > bye all

        (3) Terminate SPP controller (not for primary and secondary)
        spp > bye
        """

        cmds = arg.split(' ')
        if cmds[0] == 'sec':
            self.close_all_secondary()
        elif cmds[0] == 'all':
            print('Closing secondary ...')
            self.close_all_secondary()
            print('Closing primary ...')
            self.command_primary('exit')
        elif cmds[0] == '':
            print('Thank you for using Soft Patch Panel')
            self.close()
            return True

    def complete_bye(self, text, line, begidx, endidx):
        """Completion for bye commands"""

        if not text:
            completions = self.BYE_CMDS[:]
        else:
            completions = [p
                           for p in self.BYE_CMDS
                           if p.startswith(text)
                           ]
        return completions

    def do_cat(self, arg):
        """View contents of a file.

        spp > cat file
        """
        if os.path.isfile(arg):
            c = 'cat %s' % arg
            subprocess.call(c, shell=True)
        else:
            print("No such a directory.")

    def do_redo(self, args):
        """Execute command of index of history."""

        idx = int(args)
        cmdline = None
        cnt = 1
        try:
            for line in open(self.hist_file):
                if cnt == idx:
                    cmdline = line.strip()
                    break
                cnt += 1

            if cmdline.find('pri;') > -1:
                cmdline = cmdline.replace(';', ' ;')
                print(cmdline)
            cmd_ary = cmdline.split(' ')
            cmd = cmd_ary.pop(0)
            cmd_options = ' '.join(cmd_ary)
            eval('self.do_%s(cmd_options)' % cmd)
        except IOError:
            print('Error: Cannot open history file "%s"' %
                    self.hist_file)

    def do_history(self, arg):
        """Show history.

        spp > history
          1  ls
          2  cat file.txt
          ...
        """

        # flush all of history to the hist_file.
        readline.write_history_file(self.hist_file)

        # remove commands defined in `self.HIST_EXCEPT`
        self.clean_hist_file()

        try:
            f = open(self.hist_file)

            # setup output format
            nof_lines = len(f.readlines())
            f.seek(0)
            lines_digit = len(str(nof_lines))
            hist_format = '  %' + str(lines_digit) + 'd  %s'

            cnt = 1
            for line in f:
                l = line.strip()
                print(hist_format % (cnt, l))
                cnt += 1
            f.close()
        except IOError:
            print('Error: Cannot open history file "%s"' %
                    self.hist_file)

    def complete_cat(self, text, line, begidx, endidx):
        return common.compl_common(text, line)

    def do_less(self, arg):
        """View contents of a file

        spp > less file
        """
        if os.path.isfile(arg):
            c = 'less %s' % arg
            subprocess.call(c, shell=True)
        else:
            print("No such a directory.")

    def complete_less(self, text, line, begidx, endidx):
        return common.compl_common(text, line)

    def do_exit(self, args):
        """Terminate SPP controller

        It is an alias for bye command and same as bye command.

        spp > exit
        """

        self.close()
        print('Thank you for using Soft Patch Panel')
        return True

    def do_inspect(self, args):
        from pprint import pprint
        if args == '':
            pprint(vars(self))

    def terms_topo_subgraph(self):
        """Define terms of topo_subgraph command"""

        return ['add', 'del']

    def do_topo_subgraph(self, args):
        """Subgarph manager for topo

        Subgraph is a group of object defined in dot language.
        For topo command, it is used for grouping resources of each
        of VM or container to topology be more understandable.

        Add subgraph labeled 'vm1'.
        spp > topo_subgraph add vm1 vhost:1;vhost:2

        Delete subgraph 'vm1'.
        spp > topo_subgraph del vm1

        To show subgraphs, run topo_subgraph without args.
        spp > topo_subgraph
        label: vm1	subgraph: "vhost:1;vhost:2"
        """

        args_cleaned = re.sub(r"\s+", ' ', args).strip()
        # Show subgraphs if given no argments
        if (args_cleaned == ''):
            if len(self.subgraphs) == 0:
                print("No subgraph.")
            else:
                for label, subg in self.subgraphs.items():
                    print('label: %s\tsubgraph: "%s"' % (label, subg))
        else:  # add or del
            tokens = args_cleaned.split(' ')
            # Add subgraph
            if tokens[0] == 'add':
                if len(tokens) == 3:
                    label = tokens[1]
                    subg = tokens[2]
                    if ',' in subg:
                        subg = re.sub(
                            r'%s' % spp_common.delim_node,
                            spp_common.delim_label, subg)
                        subg = re.sub(r",", ";", subg)

                    # TODO(yasufum) add validation for subgraph
                    self.subgraphs[label] = subg
                    print("Add subgraph '%s'" % label)
                else:
                    print("Invalid syntax '%s'!" % args_cleaned)
            # Delete subgraph
            elif ((tokens[0] == 'del') or
                    (tokens[0] == 'delete') or
                    (tokens[0] == 'remove')):
                del(self.subgraphs[tokens[1]])
                print("Delete subgraph '%s'" % tokens[1])

            else:
                print("Ivalid subcommand '%s'!" % tokens[0])

    def complete_topo_subgraph(self, text, line, begidx, endidx):
        terms = self.terms_topo_subgraph()

        tokens = re.sub(r"\s+", ' ', line).strip().split(' ')
        if text == '':
            if len(tokens) == 1:
                return terms
            elif len(tokens) == 2 and tokens[1] == 'del':
                return self.subgraphs.keys()
        elif text != '':
            completions = []
            if len(tokens) == 3 and tokens[1] == 'del':
                for t in self.subgraphs.keys():
                    if t.startswith(tokens[2]):
                        completions.append(t)
            elif len(tokens) == 2:
                for t in terms:
                    if t.startswith(text):
                        completions.append(t)
            return completions
        else:
            pass

    def do_topo_resize(self, args):
        if args == '':
            print(self.topo_size)
        else:
            if '%' in args:
                self.topo_size = args
                print(self.topo_size)
            elif '.' in args:
                ii = float(args) * 100
                self.topo_size = str(ii) + '%'
                print(self.topo_size)
            else:  # TODO(yasufum) add check for no number
                self.topo_size = str(float(args) * 100) + '%'
                print(self.topo_size)

    def do_topo(self, args):
        """Output network topology

        Support four types of output.
        * terminal (but very few terminals supporting to display images)
        * browser (websocket server is required)
        * image file (jpg, png, bmp)
        * text (dot, json, yaml)

        spp > topo term  # terminal
        spp > topo http  # browser
        spp > topo network_conf.jpg  # image
        spp > topo network_conf.dot  # text
        """

        if len(spp_common.SECONDARY_LIST) == 0:
            message = "secondary not exist"
            print(message)
            self.response(self.CMD_NOTREADY, message)
        else:
            tp = topo.Topo(
                spp_common.SECONDARY_LIST,
                spp_common.MAIN2SEC,
                spp_common.SEC2MAIN,
                self.subgraphs)
            args_ary = args.split()
            if len(args_ary) == 0:
                print("Usage: topo dst [ftype]")
                return False
            elif (args_ary[0] == "term") or (args_ary[0] == "http"):
                res_ary = tp.show(args_ary[0], self.topo_size)
            elif len(args_ary) == 1:
                ftype = args_ary[0].split(".")[-1]
                res_ary = tp.output(args_ary[0], ftype)
            elif len(args_ary) == 2:
                res_ary = tp.output(args_ary[0], args_ary[1])
            else:
                print("Usage: topo dst [ftype]")
                return False
            self.response(self.CMD_OK, json.dumps(res_ary))

    def complete_topo(self, text, line, begidx, endidx):
        """Complete topo command

        If no token given, return 'term' and 'http'.
        On the other hand, complete 'term' or 'http' if token starts
        from it, or complete file name if is one of supported formats.
        """

        terms = ['term', 'http']
        # Supported formats
        img_exts = ['jpg', 'png', 'bmp']
        txt_exts = ['dot', 'yml', 'js']

        # Number of given tokens is expected as two. First one is
        # 'topo'. If it is three or more, this methods returns nothing.
        tokens = re.sub(r"\s+", " ", line).split(' ')
        if (len(tokens) == 2):
            if (text == ''):
                completions = terms
            else:
                completions = []
                # Check if 2nd token is a part of terms.
                for t in terms:
                    if t.startswith(tokens[1]):
                        completions.append(t)
                # Not a part of terms, so check for completion for
                # output file name.
                if len(completions) == 0:
                    if tokens[1].endswith('.'):
                        completions = img_exts + txt_exts
                    elif ('.' in tokens[1]):
                        fname = tokens[1].split('.')[0]
                        token = tokens[1].split('.')[-1]
                        for ext in img_exts:
                            if ext.startswith(token):
                                completions.append(fname + '.' + ext)
                        for ext in txt_exts:
                            if ext.startswith(token):
                                completions.append(fname + '.' + ext)
            return completions
        else:  # do nothing for three or more tokens
            pass

    def do_load_cmd(self, args):
        """Load command plugin

        Path of plugin file is 'spp/src/controller/command'.

        spp > load hello
        """

        args = re.sub(',', ' ', args)
        args = re.sub(r'\s+', ' ', args)
        list_args = args.split(' ')

        libdir = 'command'
        mod_name = list_args[0]
        method_name = 'do_%s' % mod_name
        loaded = '%s.%s' % (libdir, mod_name)
        exec('import %s' % loaded)
        do_cmd = '%s.%s' % (loaded, method_name)
        exec('Shell.%s = %s' % (method_name, do_cmd))

        print("Module '%s' loaded." % loaded)

    def complete_load_cmd(self, text, line, begidx, endidx):
        """Complete command plugins

        Search under PLUGIN_DIR with compl_common() method.
        This method is intended to be used for searching current
        directory, but not in this case. If text is not '',
        compl_common() does not work correctly and do filtering
        for the result by self.
        """

        curdir = os.path.dirname(__file__)
        res = common.compl_common(
            '', '%s/%s' % (curdir, self.PLUGIN_DIR), 'py')

        completions = []
        for t in res:
            if text == '':
                if t[:2] != '__':
                    completions.append(t[:-3])
            else:
                if t[:len(text)] == text:
                    completions.append(t[:-3])
        return completions
