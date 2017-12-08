#!/usr/bin/python
"""Soft Patch Panel"""

from __future__ import print_function

import argparse
import cmd
import json
from Queue import Empty
from Queue import Queue
import re
import select
import socket
import SocketServer
import sys
import threading
from threading import Thread

# Turn true if activate logger to debug remote command.
logger = None

if logger is not None:
    from logging import DEBUG
    from logging import Formatter
    from logging import getLogger
    from logging import StreamHandler
    logger = getLogger(__name__)
    handler = StreamHandler()
    handler.setLevel(DEBUG)
    formatter = Formatter(
        '%(asctime)s - [%(name)s] - [%(levelname)s] - %(message)s')
    handler.setFormatter(formatter)
    logger.setLevel(DEBUG)
    logger.addHandler(handler)


CMD_OK = "OK"
CMD_NG = "NG"
CMD_NOTREADY = "NOTREADY"
CMD_ERROR = "ERROR"

RCMD_EXECUTE_QUEUE = Queue()
RCMD_RESULT_QUEUE = Queue()
REMOTE_COMMAND = "RCMD"


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


class GrowingList(list):
    """GrowingList"""

    def __setitem__(self, index, value):
        if index >= len(self):
            self.extend([None]*(index + 1 - len(self)))
        list.__setitem__(self, index, value)

# Maximum num of sock queues for secondaries
MAX_SECONDARY = 16

# init
PRIMARY = ''
SECONDARY_LIST = []
SECONDARY_COUNT = 0

# init primary comm channel
MAIN2PRIMARY = Queue()
PRIMARY2MAIN = Queue()

# init secondary comm channel list
MAIN2SEC = GrowingList()
SEC2MAIN = GrowingList()


def connectionthread(name, client_id, conn, m2s, s2m):
    """Manage secondary process connections"""

    cmd_str = 'hello'

    # infinite loop so that function do not terminate and thread do not end.
    while True:
        try:
            _, _, _ = select.select([conn, ], [conn, ], [], 5)
        except select.error:
            break

        # Sending message to connected secondary
        try:
            cmd_str = m2s.get(True)
            conn.send(cmd_str)  # send only takes string
        except KeyError:
            break
        except Exception as excep:
            print(excep, ",Error while sending msg in connectionthread()!")
            break

        # Receiving from secondary
        try:
            # 1024 stands for bytes of data to be received
            data = conn.recv(1024)
            if data:
                s2m.put("recv:%s:{%s}" % (str(conn.fileno()), data))
            else:
                s2m.put("closing:" + str(conn))
                break
        except Exception as excep:
            print(excep, ",Error while receiving msg in connectionthread()!")
            break

    SECONDARY_LIST.remove(client_id)
    conn.close()


def getclientid(conn):
    """Get client_id from client"""

    try:
        conn.send("_get_client_id")
    except KeyError:
        return -1

    data = conn.recv(1024)
    if data is None:
        return -1

    client_id = int(data.strip('\0'))

    if client_id < 0 or client_id > MAX_SECONDARY:
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

    if free_client_id < 0:
        return -1

    conn.send("_set_client_id %u" % free_client_id)
    data = conn.recv(1024)

    return free_client_id


def acceptthread(sock, main2sec, sec2main):
    """Listen for secondary processes"""

    global SECONDARY_COUNT

    try:
        while True:
            # Accepting incoming connections
            conn, _ = sock.accept()

            client_id = getclientid(conn)
            if client_id < 0:
                break

            # Creating new thread.
            # Calling secondarythread function for this function and passing
            # conn as argument.
            SECONDARY_LIST.append(client_id)
            main2sec[client_id] = Queue()
            sec2main[client_id] = Queue()
            connection_thread = Thread(target=connectionthread,
                                       args=('secondary', client_id, conn,
                                             main2sec[client_id],
                                             sec2main[client_id]))
            connection_thread.daemon = True
            connection_thread.start()

            SECONDARY_COUNT += 1
    except Exception as excep:
        print(excep, ", Error in acceptthread()!")
        sock.close()


def command_primary(command):
    """Send command to primary process"""

    if PRIMARY:
        MAIN2PRIMARY.put(command)
        recv = PRIMARY2MAIN.get(True)
        print (recv)
        return CMD_OK, recv
    else:
        recv = "primary not started"
        print (recv)
        return CMD_NOTREADY, recv


def command_secondary(sec_id, command):
    """Send command to secondary process with sec_id"""

    if sec_id in SECONDARY_LIST:
        MAIN2SEC[sec_id].put(command)
        recv = SEC2MAIN[sec_id].get(True)
        print (recv)
        return CMD_OK, recv
    else:
        message = "secondary id %d not exist" % sec_id
        print(message)
        return CMD_NOTREADY, message


def get_status():
    secondary = []
    for i in SECONDARY_LIST:
        secondary.append("%d" % i)
    stat = {
        "primary": "%d" % PRIMARY,
        "secondary": secondary
        }
    return stat


def print_status():
    """Display information about connected clients"""

    print ("Soft Patch Panel Status :")
    print ("primary: %d" % PRIMARY)  # "primary: 1" if PRIMA == True
    print ("secondary count: %d" % len(SECONDARY_LIST))
    for i in SECONDARY_LIST:
        print ("Connected secondary id: %d" % i)


def primarythread(sock, main2primary, primary2main):
    """Manage primary process connection"""

    global PRIMARY
    cmd_str = ''

    while True:
        # waiting for connection
        PRIMARY = False
        conn, addr = sock.accept()
        PRIMARY = True

        while conn:
            try:
                _, _, _ = select.select([conn, ], [conn, ], [], 5)
            except select.error:
                break

            # Sending message to connected primary
            try:
                cmd_str = main2primary.get(True)
                conn.send(cmd_str)  # send only takes string
            except KeyError:
                break
            except Exception as excep:
                print(excep, ", Error while sending msg in primarythread()!")
                break

            # Receiving from primary
            try:
                # 1024 stands for bytes of data to be received
                data = conn.recv(1024)
                if data:
                    primary2main.put("recv:%s:{%s}" % (str(addr), data))
                else:
                    primary2main.put("closing:" + str(addr))
                    conn.close()
                    break
            except Exception as excep:
                print(excep, ", Error while receiving msg in primarythread()!")
                break

    print ("primary communication thread end")


def close_all_secondary():
    """Exit all secondary processes"""

    global SECONDARY_COUNT

    tmp_list = []
    for i in SECONDARY_LIST:
        tmp_list.append(i)
    for i in tmp_list:
        command_secondary(i, 'exit')
    SECONDARY_COUNT = 0


def check_sec_cmds(cmds):
    """Validate secondary commands before sending"""

    level1 = ['status', 'exit', 'forward', 'stop']
    level2 = ['add', 'patch', 'del']
    patch_args = ['reset']
    add_del_args = ['ring', 'vhost']
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
                if str.isdigit(cmdlist[1]) and str.isdigit(cmdlist[2]):
                    valid = 1

    return valid


def clean_sec_cmd(cmdstr):
    """remove unwanted spaces to avoid invalid command error"""

    tmparg = re.sub(r'\s+', " ", cmdstr)
    res = re.sub(r'\s?;\s?', ";", tmparg)
    return res


class Shell(cmd.Cmd):
    """SPP command prompt"""

    intro = 'Welcome to the spp.   Type help or ? to list commands.\n'
    prompt = 'spp > '
    recorded_file = None

    PRI_CMDS = ['status', 'exit', 'clear']
    SEC_CMDS = ['status', 'exit', 'forward', 'stop', 'add', 'patch', 'del']
    SEC_SUBCMDS = ['vhost', 'ring']
    BYE_CMDS = ['sec', 'all']

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

        print_status()
        stat = get_status()
        self.response(CMD_OK, json.dumps(stat))

    def do_pri(self, command):
        """Send command to primary process"""

        if command and command in self.PRI_CMDS:
            result, message = command_primary(command)
            self.response(result, message)
        else:
            message = "primary invalid command"
            print(message)
            self.response(CMD_ERROR, message)

    def do_sec(self, arg):
        """Send command to secondary process"""

        # remove unwanted spaces to avoid invalid command error
        tmparg = clean_sec_cmd(arg)
        cmds = tmparg.split(';')
        if len(cmds) < 2:
            message = "error"
            print(message)
            self.response(CMD_ERROR, message)
        elif str.isdigit(cmds[0]):
            sec_id = int(cmds[0])
            if check_sec_cmds(cmds[1]):
                result, message = command_secondary(sec_id, cmds[1])
                self.response(result, message)
            else:
                message = "invalid cmd"
                print(message)
                self.response(CMD_ERROR, message)
        else:
            print (cmds[0])
            print ("first %s" % cmds[1])
            self.response(CMD_ERROR, "invalid format")

    def do_record(self, fname):
        """Save future commands to filename:  RECORD filename.cmd"""

        if fname == '':
            print("Record file is required!")
        else:
            self.recorded_file = open(fname, 'w')
            self.response(CMD_OK, "record")

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
                        if line.strip().startswith("#"):
                            continue
                        lines.append(line)
                    self.cmdqueue.extend(lines)
                    self.response(CMD_OK, "playback")
            except IOError:
                message = "Error: File does not exist."
                print(message)
                self.response(CMD_NG, message)

    def precmd(self, line):
        line = line.lower()
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

    def do_bye(self, arg):
        """Stop recording, close SPP, and exit: BYE"""

        cmds = arg.split(' ')
        if cmds[0] == 'sec':
            close_all_secondary()
        elif cmds[0] == 'all':
            close_all_secondary()
            command_primary('exit')
        elif cmds[0] == '':
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

    # Creating primary socket object
    primary_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    primary_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    # Binding primary socket to a address. bind() takes tuple of host and port.
    primary_sock.bind((host, primary_port))

    # Listening primary at the address
    primary_sock.listen(1)  # 5 denotes the number of clients can queue

    primary_thread = Thread(target=primarythread,
                            args=(primary_sock, MAIN2PRIMARY, PRIMARY2MAIN))
    primary_thread.daemon = True
    primary_thread.start()

    # Creating secondary socket object
    secondary_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    secondary_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    # Binding secondary socket to a address. bind() takes tuple of host
    # and port.
    secondary_sock.bind((host, secondary_port))

    # Listening secondary at the address
    secondary_sock.listen(MAX_SECONDARY)

    # secondary process handling thread
    accept_thread = Thread(target=acceptthread,
                           args=(secondary_sock, MAIN2SEC, SEC2MAIN))
    accept_thread.daemon = True
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
        primary_sock.shutdown(socket.SHUT_RDWR)
        primary_sock.close()
    except socket.error as excep:
        print(excep, ", Error while closing primary_sock in main()!")

    try:
        secondary_sock.shutdown(socket.SHUT_RDWR)
        secondary_sock.close()
    except socket.error as excep:
        print(excep, ", Error while closing secondary_sock in main()!")


if __name__ == "__main__":
    main(sys.argv[1:])
