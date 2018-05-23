#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015-2016 Intel Corporation

from __future__ import absolute_import

from queue import Queue
import select
import socket
from . import spp_common
from .spp_common import logger
import threading
import traceback


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
                cmd_str = spp_common.MAIN2SEC[self.client_id].get(True)
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
                    msg = "%s" % data.decode('utf-8')
                    spp_common.SEC2MAIN[self.client_id].put(msg)
                else:
                    spp_common.SEC2MAIN[self.client_id].put(
                        "closing:" + str(self.conn))
                    break
            except Exception as excep:
                print(
                    excep, ",Error while receiving msg in connectionthread()!")
                break

        spp_common.SECONDARY_LIST.remove(self.client_id)
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
        self.sock.listen(spp_common.MAX_SECONDARY)

        self.stop_event = threading.Event()
        self.sock_opened = False

    def getclientid(self, conn):
        """Get client_id from client"""

        try:
            conn.send(b'_get_client_id')
        except KeyError:
            return -1

        data = conn.recv(1024)
        if data is None:
            return -1

        if logger is not None:
            logger.debug("data: %s" % data)
        client_id = int(data.decode('utf-8').strip('\0'))

        if client_id < 0 or client_id > spp_common.MAX_SECONDARY:
            logger.debug("Failed to get client_id: %d" % client_id)
            return -1

        found = 0
        for i in spp_common.SECONDARY_LIST:
            if client_id == i:
                found = 1
                break

        if found == 0:
            return client_id

        # client_id in use, find a free one
        free_client_id = -1
        for i in range(spp_common.MAX_SECONDARY):
            found = -1
            for j in spp_common.SECONDARY_LIST:
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

        msg = "_set_client_id %u" % free_client_id
        conn.send(msg.encode('utf-8'))
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
                spp_common.SECONDARY_LIST.append(client_id)
                spp_common.MAIN2SEC[client_id] = Queue()
                spp_common.SEC2MAIN[client_id] = Queue()
                connection_thread = ConnectionThread(client_id, conn)
                connection_thread.daemon = True
                connection_thread.start()

                spp_common.SECONDARY_COUNT += 1
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
        cmd_str = ''

        while True:
            # waiting for connection
            spp_common.PRIMARY = False
            conn, addr = self.sock.accept()
            spp_common.PRIMARY = True

            while conn:
                try:
                    _, _, _ = select.select([conn, ], [conn, ], [], 5)
                except select.error:
                    break

                self.sock_opened = True
                # Sending message to connected primary
                try:
                    cmd_str = spp_common.MAIN2PRIMARY.get(True)
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
                        spp_common.PRIMARY2MAIN.put(
                            "recv:%s:{%s}" % (str(addr), data.decode('utf-8')))
                    else:
                        spp_common.PRIMARY2MAIN.put("closing:" + str(addr))
                        conn.close()
                        self.sock_opened = False
                        break
                except Exception as excep:
                    print(
                        excep,
                        ", Error while receiving msg in primarythread()!")
                    break
