#!/usr/bin/python
"""Soft Patch Panel"""

from __future__ import print_function

import argparse
from conn_thread import AcceptThread
from conn_thread import PrimaryThread
from shell import Shell
import socket
import SocketServer
import spp_common
from spp_common import logger
import sys
import threading
import traceback


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
            spp_common.RCMD_EXECUTE_QUEUE.put(spp_common.REMOTE_COMMAND)
            CmdRequestHandler.CMD.onecmd(self.data)
            ret = spp_common.RCMD_RESULT_QUEUE.get()
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

    main(sys.argv[1:])
