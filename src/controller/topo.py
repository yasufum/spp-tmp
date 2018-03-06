#!/usr/bin/env python
# coding: utf-8

import os
import re
from spp_common import logger
import subprocess
import traceback
import uuid
import websocket


class Topo(object):
    """Setup and output network topology for topo command

    Topo command supports four types of output.
    * terminal (but very few terminals supporting to display images)
    * browser (websocket server is required)
    * image file (jpg, png, bmp)
    * text (dot, json, yaml)
    """

    def __init__(self, sec_ids, m2s_queues, s2m_queues):
        logger.info("Topo initialized with sec IDs %s" % sec_ids)
        self.sec_ids = sec_ids
        self.m2s_queues = m2s_queues
        self.s2m_queues = s2m_queues

    def show(self, dtype):
        res_ary = []
        for sec_id in self.sec_ids:
            self.m2s_queues[sec_id].put("status")
            res = self.format_sec_status(self.s2m_queues[sec_id].get(True))
            res_ary.append(res)
        if dtype == "http":
            self.to_http(res_ary)
        elif dtype == "term":
            self.to_term(res_ary)
        else:
            print("Invalid file type")
            return res_ary

    def output(self, fname, ftype="dot"):
        res_ary = []
        for sec_id in self.sec_ids:
            self.m2s_queues[sec_id].put("status")
            res = self.format_sec_status(self.s2m_queues[sec_id].get(True))
            res_ary.append(res)

        if ftype == "dot":
            self.to_dot(res_ary, fname)
        elif ftype == "json" or ftype == "js":
            self.to_json(res_ary, fname)
        elif ftype == "yaml" or ftype == "yml":
            self.to_yaml(res_ary, fname)
        elif ftype == "jpg" or ftype == "png" or ftype == "bmp":
            self.to_img(res_ary, fname)
        else:
            print("Invalid file type")
            return res_ary
        print("Create topology: '%s'" % fname)
        return res_ary

    def to_dot(self, sec_list, output_fname):
        # Label given if outport is "none"
        NO_PORT = "none"

        # Graphviz params
        SEC_COLORS = [
            "blue", "green", "orange", "chocolate", "black",
            "cyan3", "green3", "indianred", "lawngreen", "limegreen"]
        PORT_COLORS = {
            "PHY": "white",
            "RING": "yellow",
            "VHOST": "limegreen"}
        LINE_STYLE = {
            "RUNNING": "solid",
            "IDLING": "dashed"}
        GRAPH_TYPE = "digraph"
        LINK_TYPE = "->"

        node_attrs = 'node[shape="rectangle", style="filled"];'

        phys = []
        rings = []
        vhosts = []
        links = []

        for sec in sec_list:
            for port in sec["ports"]:
                if port["iface"]["type"] == "PHY":
                    phys.append(port)
                elif port["iface"]["type"] == "RING":
                    rings.append(port)
                elif port["iface"]["type"] == "VHOST":
                    vhosts.append(port)
                else:
                    raise ValueError(
                        "Invaid interface type: %s" % port["iface"]["type"])

                if port["out"] != NO_PORT:
                    out_id = int(port["out"])
                    if sec["forward"] is True:
                        l_style = LINE_STYLE["RUNNING"]
                    else:
                        l_style = LINE_STYLE["IDLING"]
                    attrs = '[label="%s", color="%s", style="%s"]' % (
                        "sec" + sec["sec_id"],
                        SEC_COLORS[int(sec["sec_id"])],
                        l_style
                    )
                    tmp = "%s%s %s %s%s%s;" % (
                        port["iface"]["type"],
                        port["iface"]["id"],
                        LINK_TYPE,
                        sec["ports"][out_id]["iface"]["type"],
                        sec["ports"][out_id]["iface"]["id"],
                        attrs
                    )
                    links.append(tmp)

        output = ["%s spp{" % GRAPH_TYPE]
        output.append("newrank=true;")
        output.append(node_attrs)

        phy_labels = []
        for p in phys:
            phy_labels.append(p["iface"]["type"] + p["iface"]["id"])
        phy_labels = list(set(phy_labels))
        for l in phy_labels:
            output.append(
                    '%s[label="%s", fillcolor="%s"];' % (
                        l, l, PORT_COLORS["PHY"]))

        ring_labels = []
        for p in rings:
            ring_labels.append(p["iface"]["type"] + p["iface"]["id"])
        ring_labels = list(set(ring_labels))
        for l in ring_labels:
            output.append(
                '%s[label="%s", fillcolor="%s"];' % (
                    l, l, PORT_COLORS["RING"]))

        vhost_labels = []
        for p in vhosts:
            vhost_labels.append(p["iface"]["type"] + p["iface"]["id"])
        vhost_labels = list(set(vhost_labels))
        for l in vhost_labels:
            output.append(
                '%s[label="%s", fillcolor="%s"];' % (
                    l, l, PORT_COLORS["VHOST"]))

        # rank
        output.append(
            '{rank=same; %s}' % ("; ".join(ring_labels)))
        if len(phys) > 0:
            output.append(
                '{rank=max; %s}' % (
                    phys[0]["iface"]["type"] + phys[0]["iface"]["id"]))
        output.append(
            '{rank=same; %s}' % ("; ".join(phy_labels)))

        # subgraph
        cluster_id = "cluster0"
        sg_label = "Host"
        sg_ports = "; ".join(phy_labels + ring_labels)
        output.append(
            'subgraph %s {label="%s" %s}' % (cluster_id, sg_label, sg_ports))

        for link in links:
            output.append(link)

        output.append("}")

        # remove duplicated entries
        f = open(output_fname, "w+")
        f.write("\n".join(output))
        f.close()

    def to_json(self, sec_list, output_fname):
        import json
        f = open(output_fname, "w+")
        f.write(json.dumps(sec_list))
        f.close()

    def to_yaml(self, sec_list, output_fname):
        import yaml
        f = open(output_fname, "w+")
        f.write(yaml.dump(sec_list))
        f.close()

    def to_img(self, sec_list, output_fname):
        tmpfile = "%s.dot" % uuid.uuid4().hex
        self.to_dot(sec_list, tmpfile)
        fmt = output_fname.split(".")[-1]
        cmd = "dot -T%s %s -o %s" % (fmt, tmpfile, output_fname)
        subprocess.call(cmd, shell=True)
        subprocess.call("rm -f %s" % tmpfile, shell=True)

    def to_http(self, sec_list):
        tmpfile = "%s.dot" % uuid.uuid4().hex
        self.to_dot(sec_list, tmpfile)
        msg = open(tmpfile).read()
        subprocess.call("rm -f %s" % tmpfile, shell=True)
        ws_url = "ws://localhost:8989/spp_ws"
        ws = websocket.create_connection(ws_url)
        ws.send(msg)
        ws.close()

    def to_term(self, sec_list):
        tmpfile = "%s.jpg" % uuid.uuid4().hex
        self.to_img(sec_list, tmpfile)
        from distutils import spawn

        # TODO(yasufum) Add check for using only supported terminal

        if spawn.find_executable("img2sixel") is not None:
            img_cmd = "img2sixel"
        else:
            img_cmd = "%s/%s/imgcat.sh" % (
                os.path.dirname(__file__), '3rd_party')
        # Resize image to fit the terminal
        img_size = "60%"
        cmd = "convert -resize %s %s %s" % (img_size, tmpfile, tmpfile)
        subprocess.call(cmd, shell=True)
        subprocess.call("%s %s" % (img_cmd, tmpfile), shell=True)
        subprocess.call(["rm", "-f", tmpfile])

    def format_sec_status(self, stat):
        """Return formatted secondary status as a hash

        By running status command on controller, status is sent from
        secondary process and receiving message is displayed.

        This is an example of receiving status message.
            recv:8:{Client ID 1 Idling
            client_id:1
            port_id:0,on,PHY,outport:2
            port_id:1,on,PHY,outport:none
            port_id:2,on,RING(0),outport:3
            port_id:3,on,VHOST(1),outport:none
            }

        This method returns as following.
            {
            'forward': False,
            'ports': [
                {
                    'out': 'none',
                    'id': '0',
                    'iface': {'type': 'PHY', 'id': '0'}
                },
                {
                    'out': 'none',
                    'id': '1',
                    'iface': {'type': 'PHY', 'id': '1'}
                }
            ],
            'sec_id': '2'
            }
        """

        stat_ary = stat.split("\n")
        res = {}

        try:
            # Check running status
            if "Idling" in stat_ary[0]:
                res["forward"] = False
            elif "Running" in stat_ary[0]:
                res["forward"] = True
            else:
                print("Invalid forwarding status:", stat_ary[0])

            ptn = re.compile(r"clinet_id:(\d+)")
            m = ptn.match(stat_ary[1])
            if m is not None:
                res["sec_id"] = m.group(1)
            else:
                raise Exception("No client ID matched!")

            ports = []
            # match PHY, for exp. 'port_id:0,on,PHY,outport:none'
            ptn_p = re.compile(r"port_id:(\d+),on,(\w+),outport:(\w+)")

            # match RING for exp. 'port_id:2,on,RING(0),outport:3'
            # or VHOST for exp. 'port_id:3,on,VHOST(1),outport:none'
            ptn_v = re.compile(
                r"port_id:(\d+),on,(\w+)\((\d+)\),outport:(\w+)")

            for i in range(2, len(stat_ary)-1):
                m = ptn_p.match(stat_ary[i])
                if m is not None:
                    ports.append({
                        "id": m.group(1),
                        "iface": {"type": m.group(2), "id": m.group(1)},
                        "out": m.group(3)})
                    continue

                m = ptn_v.match(stat_ary[i])
                if m is not None:
                    ports.append({
                        "id": m.group(1),
                        "iface": {"type": m.group(2), "id": m.group(3)},
                        "out": m.group(4)})

            res["ports"] = ports
            return res

        except Exception:
            traceback.print_exc()
            return None
