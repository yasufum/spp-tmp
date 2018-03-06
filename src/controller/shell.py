import cmd
# import importlib
import json
import os
from Queue import Empty
import re
import spp_common
from spp_common import logger
import subprocess


class Shell(cmd.Cmd, object):
    """SPP command prompt"""

    intro = 'Welcome to the spp.   Type help or ? to list commands.\n'
    prompt = 'spp > '
    recorded_file = None

    CMD_OK = "OK"
    CMD_NG = "NG"
    CMD_NOTREADY = "NOTREADY"
    CMD_ERROR = "ERROR"

    PORT_TYPES = ['phy', 'ring', 'vhost']

    PRI_CMDS = ['status', 'exit', 'clear']
    SEC_CMDS = ['status', 'exit', 'forward', 'stop', 'add', 'patch', 'del']
    SEC_SUBCMDS = ['vhost', 'ring', 'pcap', 'nullpmd']
    BYE_CMDS = ['sec', 'all']

    def decorate_dir(self, curdir, filelist):
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

    def compl_common(self, text, line, ftype=None):
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
                res = self.decorate_dir(
                    '.', os.listdir(os.getcwd()))
            else:  # after '/'
                res = self.decorate_dir(
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
            res = self.decorate_dir(target_dir, matched)

        if ftype is not None:  # filtering by ftype
            completions = []
            if ftype == 'directory':
                for fn in res:
                    if fn[-1] == '/':
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

    def is_comment_line(self, line):
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

    def default(self, line):
        """Define defualt behaviour

        If user input is commend styled, controller simply echo
        as a comment.
        """

        if self.is_comment_line(line):
            print("%s" % line.strip())
        else:
            super(Shell, self).default(line)

    def emptyline(self):
        """Do nothin for empty input

        It override Cmd.emptyline() which runs previous input as default
        to do nothing.
        """
        pass

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

        print ("Soft Patch Panel Status :")
        print ("primary: %d" % spp_common.PRIMARY)  # it is 1 if PRIMA == True
        print ("secondary count: %d" % len(spp_common.SECONDARY_LIST))
        for i in spp_common.SECONDARY_LIST:
            print ("Connected secondary id: %d" % i)

    def command_primary(self, command):
        """Send command to primary process"""

        if spp_common.PRIMARY:
            spp_common.MAIN2PRIMARY.put(command)
            recv = spp_common.PRIMARY2MAIN.get(True)
            print (recv)
            return self.CMD_OK, recv
        else:
            recv = "primary not started"
            print (recv)
            return self.CMD_NOTREADY, recv

    def command_secondary(self, sec_id, command):
        """Send command to secondary process with sec_id"""

        if sec_id in spp_common.SECONDARY_LIST:
            spp_common.MAIN2SEC[sec_id].put(command)
            recv = spp_common.SEC2MAIN[sec_id].get(True)
            print (recv)
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

    def clean_sec_cmd(self, cmdstr):
        """remove unwanted spaces to avoid invalid command error"""

        tmparg = re.sub(r'\s+', " ", cmdstr)
        res = re.sub(r'\s?;\s?', ";", tmparg)
        return res

    def complete_sec(self, text, line, begidx, endidx):
        """Completion for secondary process commands"""

        try:
            cleaned_line = self.clean_sec_cmd(line)
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

    def response(self, result, message):
        """Enqueue message from other than CLI"""

        try:
            rcmd = spp_common.RCMD_EXECUTE_QUEUE.get(False)
        except Empty:
            return

        if (rcmd == spp_common.REMOTE_COMMAND):
            param = result + '\n' + message
            spp_common.RCMD_RESULT_QUEUE.put(param)
        else:
            if logger is not None:
                logger.debug("unknown remote command = %s" % rcmd)

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

        if command and command in self.PRI_CMDS:
            result, message = self.command_primary(command)
            self.response(result, message)
        else:
            message = "primary invalid command"
            print(message)
            self.response(self.CMD_ERROR, message)

    def do_sec(self, arg):
        """Send command to secondary process

        SPP secondary process is specified with secondary ID and takes
        sub commands.

        spp > sec 1;status
        spp > sec 1;add ring 0
        spp > sec 1;patch 0 2
        """

        # remove unwanted spaces to avoid invalid command error
        tmparg = self.clean_sec_cmd(arg)
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
            print (cmds[0])
            print ("first %s" % cmds[1])
            self.response(self.CMD_ERROR, "invalid format")

    def complete_record(self, text, line, begidx, endidx):
        return self.compl_common(text, line)

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

    def complete_playback(self, text, line, begidx, endidx):
        return self.compl_common(text, line)

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
                        if not self.is_comment_line(line):
                            lines.append("# %s" % line)
                        lines.append(line)
                    self.cmdqueue.extend(lines)
                    self.response(self.CMD_OK, "playback")
            except IOError:
                message = "Error: File does not exist."
                print(message)
                self.response(self.CMD_NG, message)

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

    def do_pwd(self, args):
        """Show corrent directory

        It behaves as UNIX's pwd command.

        spp > pwd
        """

        print(os.getcwd())

    def complete_ls(self, text, line, begidx, endidx):
        return self.compl_common(text, line)

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

    def complete_cd(self, text, line, begidx, endidx):
        return self.compl_common(text, line, 'directory')

    def do_cd(self, args):
        """Change current directory

        spp > cd path/to/dir
        """

        if os.path.isdir(args):
            os.chdir(args)
            print(os.getcwd())
        else:
            print("No such a directory.")

    def complete_mkdir(self, text, line, begidx, endidx):
        return self.compl_common(text, line)

    def do_mkdir(self, args):
        """Create a new directory

        It behaves as 'mkdir -p'.

        spp > mkdir path/to/dir
        """

        c = 'mkdir -p %s' % args
        subprocess.call(c, shell=True)

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
            self.close_all_secondary()
            self.command_primary('exit')
        elif cmds[0] == '':
            print('Thank you for using Soft Patch Panel')
            self.close()
            return True

    def do_exit(self, args):
        """Terminate SPP controller

        It is an alias for bye command and same as bye command.

        spp > exit
        """
        self.close()
        print('Thank you for using Soft Patch Panel')
        return True

    def do_load(self, args):
        """Load command plugin

        Path of plugin file is 'spp/src/controller/command'.

        spp > load hello
        """

        args = re.sub(',', ' ', args)
        args = re.sub(r'\s+', ' ', args)
        list_args = args.split(' ')

        libdir = 'command'
        loaded = '%s.%s' % (libdir, list_args[0])
        # importlib.import_module(loaded)
        exec('import %s' % loaded)
        do_cmd = '%s.do_%s' % (loaded, list_args[0])
        setattr(self, 'do_%s' % list_args[0], eval(do_cmd))
        print("Module '%s' loaded." % loaded)
