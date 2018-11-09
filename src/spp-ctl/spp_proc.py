# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

import bottle
import eventlet
import json
import logging

import spp_ctl


LOG = logging.getLogger(__name__)

ID_PRIMARY = 0
TYPE_PRIMARY = "primary"
TYPE_VF = "vf"
TYPE_NFV = "nfv"


def exec_command(func):
    """Decorator for Sending command and receiving reply.

    Define the common function for sending command and receiving reply
    as a decorator. Each of methods for executing command has only to
    return command string.

    exp)
    @exec_command
    def some_command(self, ...):
        return "command string of some_command"
    """
    def wrapper(self, *args, **kwargs):
        with self.sem:
            command = func(self, *args, **kwargs)
            LOG.info("%s(%d) command executed: %s", self.type, self.id,
                     command)
            data = spp_ctl.Controller._send_command(self.conn, command)
            if data is None:
                raise RuntimeError("%s(%d): %s: no-data returned" %
                                   (self.type, self.id, command))
            LOG.debug("reply: %s", data)
            return self._decode_reply(data)
    return wrapper


class SppProc(object):
    def __init__(self, proc_type, id, conn):
        self.id = id
        self.type = proc_type
        # NOTE: executing command is serialized by using a semaphore
        # for each process.
        self.sem = eventlet.semaphore.Semaphore(value=1)
        self.conn = conn


class VfProc(SppProc):

    def __init__(self, id, conn):
        super(VfProc, self).__init__(TYPE_VF, id, conn)

    @staticmethod
    def _decode_reply(data):
        data = json.loads(data)
        result = data["results"][0]
        if result["result"] == "error":
            msg = result["error_details"]["message"]
            raise bottle.HTTPError(400, "command error: %s" % msg)
        return data

    @staticmethod
    def _decode_client_id(data):
        try:
            data = VfProc._decode_reply(data)
            return data["client_id"]
        except:
            return None

    @exec_command
    def get_status(self):
        return "status"

    @exec_command
    def start_component(self, comp_name, core_id, comp_type):
        return ("component start {comp_name} {core_id} {comp_type}"
                .format(**locals()))

    @exec_command
    def stop_component(self, comp_name):
        return "component stop {comp_name}".format(**locals())

    @exec_command
    def port_add(self, port, direction, comp_name, op, vlan_id, pcp):
        command = "port add {port} {direction} {comp_name}".format(**locals())
        if op != "none":
            command += " %s" % op
            if op == "add_vlantag":
                command += " %d %d" % (vlan_id, pcp)
        return command

    @exec_command
    def port_del(self, port, direction, comp_name):
        return "port del {port} {direction} {comp_name}".format(**locals())

    @exec_command
    def set_classifier_table(self, mac_address, port):
        return ("classifier_table add mac {mac_address} {port}"
                .format(**locals()))

    @exec_command
    def clear_classifier_table(self, mac_address, port):
        return ("classifier_table del mac {mac_address} {port}"
                .format(**locals()))

    @exec_command
    def set_classifier_table_with_vlan(self, mac_address, port,
                                       vlan_id):
        return ("classifier_table add vlan {vlan_id} {mac_address} {port}"
                .format(**locals()))

    @exec_command
    def clear_classifier_table_with_vlan(self, mac_address, port,
                                         vlan_id):
        return ("classifier_table del vlan {vlan_id} {mac_address} {port}"
                .format(**locals()))


class NfvProc(SppProc):

    def __init__(self, id, conn):
        super(NfvProc, self).__init__(TYPE_NFV, id, conn)

    @staticmethod
    def _decode_reply(data):
        return data.strip('\0')

    @staticmethod
    def _decode_client_id(data):
        try:
            return int(NfvProc._decode_reply(data))
        except:
            return None

    @exec_command
    def get_status(self):
        return "status"

    @exec_command
    def port_add(self, port):
        return "add {port}".format(**locals())

    @exec_command
    def port_del(self, port):
        return "del {port}".format(**locals())

    @exec_command
    def patch_add(self, src_port, dst_port):
        return "patch {src_port} {dst_port}".format(**locals())

    @exec_command
    def patch_reset(self):
        return "patch reset"

    @exec_command
    def forward(self):
        return "forward"

    @exec_command
    def stop(self):
        return "stop"

    @exec_command
    def do_exit(self):
        return "exit"


class PrimaryProc(SppProc):

    def __init__(self, conn):
        super(PrimaryProc, self).__init__(TYPE_PRIMARY, ID_PRIMARY, conn)

    @staticmethod
    def _decode_reply(data):
        return data.strip('\0')

    @exec_command
    def status(self):
        return "status"

    @exec_command
    def clear(self):
        return "clear"

    @exec_command
    def do_exit(self):
        return "exit"
