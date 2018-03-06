#!/usr/bin/python
"""Soft Patch Panel"""

from __future__ import print_function

import argparse
import cmd
import json
import os
from Queue import Empty
from Queue import Queue
import re
import select
import socket
import SocketServer
import subprocess
import sys
import threading
import traceback

# Turn true if activate logger to debug remote command.
logger = None

# Maximum num of sock queues for secondaries
MAX_SECONDARY = 16

PRIMARY = ''
SECONDARY_LIST = []
SECONDARY_COUNT = 0

# Initialize primary comm channel
MAIN2PRIMARY = Queue()
PRIMARY2MAIN = Queue()

REMOTE_COMMAND = "RCMD"
RCMD_EXECUTE_QUEUE = Queue()
RCMD_RESULT_QUEUE = Queue()


class GrowingList(list):
    """Growing List

    Custom list type for appending index over the range which is
    similar to ruby's Array. Empty index is filled with 'None'.
    It is used to contain queues for secondaries with any sec ID.

    >>> gl = GrowingList()
    >>> gl.[3] = 0
    >>> gl
    [None, None, None, 0]
    """

    def __setitem__(self, index, value):
        if index >= len(self):
            self.extend([None]*(index + 1 - len(self)))
        list.__setitem__(self, index, value)


# init secondary comm channel list
MAIN2SEC = GrowingList()
SEC2MAIN = GrowingList()


class CmdRequestHandler(SocketServer.BaseRequestHandler):
    """Request handler for getting message from remote entities"""

    CMD = None  # contains a instance of Shell class

    def handle(self):
        self.data = self.request.recv(1024).strip()
        cur_thread = threading.currentThread()
        print(cur_thread.getName())
        print(self.client_address[0])
        print(self.data)
        if CmdRequestHandler.CMD is not None:
            RCMD_EXECUTE_QUEUE.put(REMOTE_COMMAND)
            CmdRequestHandler.CMD.onecmd(self.data)
            ret = RCMD_RESULT_QUEUE.get()
            if (ret is not None):
                if logger is not None:
                    logger.debug("ret:%s" % ret)
                self.request.send(ret)
            else:
                if logger is not None:
                    logger.debug("ret is none")
                self.request.send("")
        else:
            if logger is not None:
                logger.debug("CMD is None")
            self.request.send("")


class ConnectionThread(threading.Thread):
    """Manage connection between controller and secondary"""

    def __init__(self, client_id, conn):
        super(ConnectionThread, self).__init__()
        self.daemon = True

        self.client_id = client_id
        self.conn = conn
        self.stop_event = threading.Event()
        self.conn_opened = False

    def stop(self):
        self.stop_event.set()

    def run(self):
        global SECONDARY_LIST
        global MAIN2SEC
        global SEC2MAIN

        cmd_str = 'hello'

        # infinite loop so that function do not terminate and thread do not
        # end.
        while True:
            try:
                _, _, _ = select.select(
                    [self.conn, ], [self.conn, ], [], 5)
            except select.error:
                break

            # Sending message to connected secondary
            try:
                cmd_str = MAIN2SEC[self.client_id].get(True)
                self.conn.send(cmd_str)  # send only takes string
            except KeyError:
                break
            except Exception as excep:
                print(excep, ",Error while sending msg in connectionthread()!")
                break

            # Receiving from secondary
            try:
                # 1024 stands for bytes of data to be received
                data = self.conn.recv(1024)
                if data:
                    SEC2MAIN[self.client_id].put(
                        "recv:%s:{%s}" % (str(self.conn.fileno()), data))
                else:
                    SEC2MAIN[self.client_id].put("closing:" + str(self.conn))
                    break
            except Exception as excep:
                print(
                    excep, ",Error while receiving msg in connectionthread()!")
                break

        SECONDARY_LIST.remove(self.client_id)
        self.conn.close()


class AcceptThread(threading.Thread):
    """Manage connection"""

    def __init__(self, host, port):
        super(AcceptThread, self).__init__()
        self.daemon = True

        # Creating secondary socket object
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        # Binding secondary socket to a address. bind() takes tuple of host
        # and port.
        self.sock.bind((host, port))

        # Listening secondary at the address
        self.sock.listen(MAX_SECONDARY)

        self.stop_event = threading.Event()
        self.sock_opened = False

    def getclientid(self, conn):
        """Get client_id from client"""

        global SECONDARY_LIST

        try:
            conn.send("_get_client_id")
        except KeyError:
            return -1

        data = conn.recv(1024)
        if data is None:
            return -1

        if logger is not None:
            logger.debug("data: %s" % data)
        client_id = int(data.strip('\0'))

        if client_id < 0 or client_id > MAX_SECONDARY:
            logger.debug("Failed to get client_id: %d" % client_id)
            return -1

        found = 0
        for i in SECONDARY_LIST:
            if client_id == i:
                found = 1
                break

        if found == 0:
            return client_id

        # client_id in use, find a free one
        free_client_id = -1
        for i in range(MAX_SECONDARY):
            found = -1
            for j in SECONDARY_LIST:
                if i == j:
                    found = i
                    break
            if found == -1:
                free_client_id = i
                break

        if logger is not None:
            logger.debug("Found free_client_id: %d" % free_client_id)

        if free_client_id < 0:
            return -1

        conn.send("_set_client_id %u" % free_client_id)
        data = conn.recv(1024)

        return free_client_id

    def stop(self):
        if self.sock_opened is True:
            try:
                self.sock.shutdown(socket.SHUT_RDWR)
            except socket.error as excep:
                print(excep, ", Error while closing sock in AcceptThread!")
                traceback.print_exc()
        self.sock.close()
        self.stop_event.set()

    def run(self):
        global SECONDARY_COUNT
        global SECONDARY_LIST
        global MAIN2SEC
        global SEC2MAIN

        try:
            while True:
                # Accepting incoming connections
                conn, _ = self.sock.accept()

                client_id = self.getclientid(conn)
                if client_id < 0:
                    break

                # Creating new thread.
                # Calling secondarythread function for this function and
                # passing conn as argument.
                SECONDARY_LIST.append(client_id)
                MAIN2SEC[client_id] = Queue()
                SEC2MAIN[client_id] = Queue()
                connection_thread = ConnectionThread(client_id, conn)
                connection_thread.daemon = True
                connection_thread.start()

                SECONDARY_COUNT += 1
        except Exception as excep:
            print(excep, ", Error in AcceptThread!")
            traceback.print_exc()
            self.sock_opened = False
            self.sock.close()


class PrimaryThread(threading.Thread):

    def __init__(self, host, port):
        super(PrimaryThread, self).__init__()
        self.daemon = True

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        # Binding primary socket to a address. bind() takes tuple of host
        # and port.
        self.sock.bind((host, port))

        # Listening primary at the address
        self.sock.listen(1)  # 5 denotes the number of clients can queue

        self.stop_event = threading.Event()
        self.sock_opened = False

    def stop(self):
        if self.sock_opened is True:
            self.sock.shutdown(socket.SHUT_RDWR)
        self.sock.close()
        self.stop_event.set()

    def run(self):
        global PRIMARY
        cmd_str = ''

        while True:
            # waiting for connection
            PRIMARY = False
            conn, addr = self.sock.accept()
            PRIMARY = True

            while conn:
                try:
                    _, _, _ = select.select([conn, ], [conn, ], [], 5)
                except select.error:
                    break

                self.sock_opened = True
                # Sending message to connected primary
                try:
                    cmd_str = MAIN2PRIMARY.get(True)
                    conn.send(cmd_str)  # send only takes string
                except KeyError:
                    break
                except Exception as excep:
                    print(
                        excep,
                        ", Error while sending msg in primarythread()!")
                    break

                # Receiving from primary
                try:
                    # 1024 stands for bytes of data to be received
                    data = conn.recv(1024)
                    if data:
                        PRIMARY2MAIN.put(
                            "recv:%s:{%s}" % (str(addr), data))
                    else:
                        PRIMARY2MAIN.put("closing:" + str(addr))
                        conn.close()
                        self.sock_opened = False
                        break
                except Exception as excep:
                    print(
                        excep,
                        ", Error while receiving msg in primarythread()!")
                    break


def clean_sec_cmd(cmdstr):
    """remove unwanted spaces to avoid invalid command error"""

    tmparg = re.sub(r'\s+', " ", cmdstr)
    res = re.sub(r'\s?;\s?', ";", tmparg)
    return res


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

    def is_comment_line(self, line):
        input_line = line.strip()
        if len(input_line) > 0:
            if (input_line[0] == '#') or (input_line[0:2] == '//'):
                return True
            else:
                return False

    def default(self, line):
        """Define defualt behaviour

        If user input is commend styled, controller simply echo as a comment.
        Supported styles are
          - python ('#')
          - C ('//')
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
        """Exit all secondary processes"""

        global SECONDARY_COUNT
        global SECONDARY_LIST

        tmp_list = []
        for i in SECONDARY_LIST:
            tmp_list.append(i)
        for i in tmp_list:
            self.command_secondary(i, 'exit')
        SECONDARY_COUNT = 0

    def get_status(self):
        global SECONDARY_LIST

        secondary = []
        for i in SECONDARY_LIST:
            secondary.append("%d" % i)
        stat = {
            "primary": "%d" % PRIMARY,
            "secondary": secondary
            }
        return stat

    def print_status(self):
        """Display information about connected clients"""

        global SECONDARY_LIST

        print ("Soft Patch Panel Status :")
        print ("primary: %d" % PRIMARY)  # "primary: 1" if PRIMA == True
        print ("secondary count: %d" % len(SECONDARY_LIST))
        for i in SECONDARY_LIST:
            print ("Connected secondary id: %d" % i)

    def command_primary(self, command):
        """Send command to primary process"""

        if PRIMARY:
            MAIN2PRIMARY.put(command)
            recv = PRIMARY2MAIN.get(True)
            print (recv)
            return self.CMD_OK, recv
        else:
            recv = "primary not started"
            print (recv)
            return self.CMD_NOTREADY, recv

    def command_secondary(self, sec_id, command):
        """Send command to secondary process with sec_id"""

        global SECONDARY_LIST

        if sec_id in SECONDARY_LIST:
            MAIN2SEC[sec_id].put(command)
            recv = SEC2MAIN[sec_id].get(True)
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

    def complete_sec(self, text, line, begidx, endidx):
        """Completion for secondary process commands"""

        global SECONDARY_LIST

        try:
            cleaned_line = clean_sec_cmd(line)
            if len(cleaned_line.split()) == 1:
                completions = [str(i)+";" for i in SECONDARY_LIST]
            elif len(cleaned_line.split()) == 2:
                if not (";" in cleaned_line):
                    tmplist = [str(i) for i in SECONDARY_LIST]
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
            rcmd = RCMD_EXECUTE_QUEUE.get(False)
        except Empty:
            return

        if (rcmd == REMOTE_COMMAND):
            param = result + '\n' + message
            RCMD_RESULT_QUEUE.put(param)
        else:
            if logger is not None:
                logger.debug("unknown remote command = %s" % rcmd)

    def do_status(self, _):
        """Display Soft Patch Panel Status"""

        self.print_status()
        stat = self.get_status()
        self.response(self.CMD_OK, json.dumps(stat))

    def do_pri(self, command):
        """Send command to primary process"""

        if command and command in self.PRI_CMDS:
            result, message = self.command_primary(command)
            self.response(result, message)
        else:
            message = "primary invalid command"
            print(message)
            self.response(self.CMD_ERROR, message)

    def do_sec(self, arg):
        """Send command to secondary process"""

        # remove unwanted spaces to avoid invalid command error
        tmparg = clean_sec_cmd(arg)
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

    def do_record(self, fname):
        """Save future commands to filename:  RECORD filename.cmd"""

        if fname == '':
            print("Record file is required!")
        else:
            self.recorded_file = open(fname, 'w')
            self.response(self.CMD_OK, "record")

    def do_playback(self, fname):
        """Playback commands from a file:  PLAYBACK filename.cmd"""

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
        if self.recorded_file:
            if not (('playback' in line) or ('bye' in line)):
                print(line, file=self.recorded_file)
        return line

    def close(self):
        """Close record file"""

        if self.recorded_file:
            print("closing file")
            self.recorded_file.close()
            self.recorded_file = None

    def do_pwd(self, args):
        print(os.getcwd())

    def do_cd(self, args):
        if os.path.isdir(args):
            os.chdir(args)
            print(os.getcwd())
        else:
            print("No such a directory.")

    def ls_decorate_dir(self, filelist):
        res = []
        for f in filelist:
            if os.path.isdir(f):
                res.append('%s/' % f)
            else:
                res.append(f)
        return res

    def complete_ls(self, text, line, begidx, endidx):
        if text == '':
            tokens = line.split(' ')
            target = tokens[-1]
            if target == '':
                completions = self.ls_decorate_dir(
                    os.listdir(os.getcwd()))
            else:
                completions = self.ls_decorate_dir(
                    os.listdir(target))
        else:
            tokens = line.split(' ')
            target = tokens[-1]

            if '/' in target:
                seg = target.split('/')[-1]
                target_dir = '/'.join(target.split('/')[0:-1])
            else:
                seg = text
                target_dir = os.getcwd()

            matched = []
            for t in os.listdir(target_dir):
                if seg in t:
                    matched.append(t)
            completions = self.ls_decorate_dir(matched)
        return completions

    def do_ls(self, args):
        if args == '' or os.path.isdir(args):
            c = 'ls -F %s' % args
            subprocess.call(c, shell=True)
        else:
            print("No such a directory.")

    def do_bye(self, arg):
        """Stop recording, close SPP, and exit: BYE"""

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
        print('Thank you for using Soft Patch Panel')
        self.close()
        return True


def main(argv):
    """main"""

    parser = argparse.ArgumentParser(description="SPP Controller")

    parser.add_argument(
        "-p", "--pri-port",
        type=int, default=5555,
        help="primary port number")
    parser.add_argument(
        "-s", "--sec-port",
        type=int, default=6666,
        help="secondary port number")
    parser.add_argument(
        "-m", "--mng-port",
        type=int, default=7777,
        help="management port number")
    parser.add_argument(
        "-ip", "--ipaddr",
        type=str, default='',  # 'localhost' or '127.0.0.1' or '' are all same
        help="IP address")
    args = parser.parse_args()

    host = args.ipaddr
    primary_port = args.pri_port
    secondary_port = args.sec_port
    management_port = args.mng_port

    print("primary port : %d" % primary_port)
    print('secondary port : %d' % secondary_port)
    print('management port : %d' % management_port)

    primary_thread = PrimaryThread(host, primary_port)
    primary_thread.start()

    accept_thread = AcceptThread(host, secondary_port)
    accept_thread.start()

    shell = Shell()

    # Run request handler as a TCP server thread
    SocketServer.ThreadingTCPServer.allow_reuse_address = True
    CmdRequestHandler.CMD = shell
    command_server = SocketServer.ThreadingTCPServer(
        (host, management_port), CmdRequestHandler)

    t = threading.Thread(target=command_server.serve_forever)
    t.setDaemon(True)
    t.start()

    shell.cmdloop()
    shell = None

    try:
        primary_thread.stop()
        accept_thread.stop()
    except socket.error as excep:
        print(excep, ", Error while terminating threads in main()!")
        traceback.print_exc()


if __name__ == "__main__":
    if logger is True:
        from logging import DEBUG
        from logging import Formatter
        from logging import getLogger
        from logging import StreamHandler
        logger = getLogger(__name__)
        handler = StreamHandler()
        handler.setLevel(DEBUG)
        formatter = Formatter(
            '%(asctime)s,[%(filename)s][%(name)s][%(levelname)s]%(message)s')
        handler.setFormatter(formatter)
        logger.setLevel(DEBUG)
        logger.addHandler(handler)

    main(sys.argv[1:])
