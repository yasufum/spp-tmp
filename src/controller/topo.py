#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation

import os
import re
from . import spp_common
from .spp_common import logger
import subprocess
import traceback
import uuid
import yaml


class Topo(object):
    """Setup and output network topology for topo command

    Topo command supports four types of output.
    * terminal (but very few terminals supporting to display images)
    * browser (websocket server is required)
    * image file (jpg, png, bmp)
    * text (dot, json, yaml)
    """

    def __init__(self, sec_ids, m2s_queues, s2m_queues, sub_graphs):
        logger.info("Topo initialized with sec IDs %s" % sec_ids)
        self.sec_ids = sec_ids
        self.m2s_queues = m2s_queues
        self.s2m_queues = s2m_queues
        self.sub_graphs = sub_graphs

    def show(self, dtype, size):
        res_ary = []
        for sec_id in self.sec_ids:
            self.m2s_queues[sec_id].put("status")
            res = self.format_sec_status(
                sec_id, self.s2m_queues[sec_id].get(True))
            res_ary.append(res)
        if dtype == "http":
            self.to_http(res_ary)
        elif dtype == "term":
            self.to_term(res_ary, size)
        else:
            print("Invalid file type")
            return res_ary

    def output(self, fname, ftype="dot"):
        res_ary = []
        for sec_id in self.sec_ids:
            self.m2s_queues[sec_id].put("status")
            res = self.format_sec_status(
                sec_id, self.s2m_queues[sec_id].get(True))
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
        NO_PORT = None

        # Graphviz params
        # TODO(yasufum) consider to move gviz params to config file.
        SEC_COLORS = [
            "blue", "green", "orange", "chocolate", "black",
            "cyan3", "green3", "indianred", "lawngreen", "limegreen"]
        PORT_COLORS = {
            "phy": "white",
            "ring": "yellow",
            "vhost": "limegreen"}
        LINE_STYLE = {
            "running": "solid",
            "idling": "dashed"}
        GRAPH_TYPE = "digraph"
        LINK_TYPE = "->"

        node_attrs = 'node[shape="rectangle", style="filled"];'

        node_template = '%s' + spp_common.delim_node + '%s'

        phys = []
        rings = []
        vhosts = []
        links = []

        # parse status message from sec.
        for sec in sec_list:
            if sec is None:
                continue
            for port in sec["ports"]:
                if port["iface"]["type"] == "phy":
                    phys.append(port)
                elif port["iface"]["type"] == "ring":
                    rings.append(port)
                elif port["iface"]["type"] == "vhost":
                    vhosts.append(port)
                else:
                    raise ValueError(
                        "Invaid interface type: %s" % port["iface"]["type"])

                if port['out'] != NO_PORT:
                    out_type, out_id = port['out'].split(':')
                    if sec['status'] == 'running':
                        l_style = LINE_STYLE["running"]
                    else:
                        l_style = LINE_STYLE["idling"]
                    attrs = '[label="%s", color="%s", style="%s"]' % (
                        "sec%d" % sec["sec_id"],
                        SEC_COLORS[sec["sec_id"]],
                        l_style
                    )
                    link_style = node_template + ' %s ' + node_template + '%s;'
                    tmp = link_style % (
                        port["iface"]["type"],
                        port["iface"]["id"],
                        LINK_TYPE,
                        out_type,
                        out_id,
                        attrs
                    )
                    links.append(tmp)

        output = ["%s spp{" % GRAPH_TYPE]
        output.append("newrank=true;")
        output.append(node_attrs)

        phy_nodes = []
        for node in phys:
            phy_nodes.append(
                node_template % (node['iface']['type'], node['iface']['id']))
        phy_nodes = list(set(phy_nodes))
        for node in phy_nodes:
            label = re.sub(
                r'%s' % spp_common.delim_node, spp_common.delim_label, node)
            output.append(
                '%s[label="%s", fillcolor="%s"];' % (
                    node, label, PORT_COLORS["phy"]))

        ring_nodes = []
        for p in rings:
            ring_nodes.append(
                node_template % (p['iface']['type'], p['iface']['id']))
        ring_nodes = list(set(ring_nodes))
        for node in ring_nodes:
            label = re.sub(
                r'%s' % spp_common.delim_node, spp_common.delim_label, node)
            output.append(
                '%s[label="%s", fillcolor="%s"];' % (
                    node, label, PORT_COLORS["ring"]))

        vhost_nodes = []
        for p in vhosts:
            vhost_nodes.append(
                node_template % (p["iface"]["type"], p["iface"]["id"]))
        vhost_nodes = list(set(vhost_nodes))
        for node in vhost_nodes:
            label = re.sub(
                r'%s' % spp_common.delim_node, spp_common.delim_label, node)
            output.append(
                '%s[label="%s", fillcolor="%s"];' % (
                    node, label, PORT_COLORS["vhost"]))

        # rank
        output.append(
            '{rank=same; %s}' % ("; ".join(ring_nodes)))
        output.append(
            '{rank=same; %s}' % ("; ".join(vhost_nodes)))

        rank_style = '{rank=max; %s}' % node_template
        if len(phys) > 0:
            output.append(
                rank_style % (
                    phys[0]["iface"]["type"], phys[0]["iface"]["id"]))
        elif len(vhosts) > 0:
            output.append(
                rank_style % (
                    vhosts[0]["iface"]["type"], vhosts[0]["iface"]["id"]))

        if len(phy_nodes) > 0:
            output.append(
                '{rank=same; %s}' % ("; ".join(phy_nodes)))

        # Add subgraph
        ssgs = []
        if len(self.sub_graphs) > 0:
            cnt = 1
            for label, val in self.sub_graphs.items():
                cluster_id = "cluster%d" % cnt
                ssg_label = label
                ssg_ports = re.sub(
                    r'%s' % spp_common.delim_label,
                    spp_common.delim_node, val)
                ssg = 'subgraph %s {label="%s" %s}' % (
                    cluster_id, ssg_label, ssg_ports)
                ssgs.append(ssg)
                cnt += 1

        cluster_id = "cluster0"
        sg_label = "Host"
        sg_ports = "; ".join(phy_nodes + ring_nodes)
        if len(ssgs) == 0:
            output.append(
                'subgraph %s {label="%s" %s}' % (
                    cluster_id, sg_label, sg_ports))
        else:
            tmp = 'label="%s" %s' % (sg_label, sg_ports)
            contents = [tmp] + ssgs
            output.append(
                'subgraph %s {%s}' % (cluster_id, '; '.join(contents)))

        # Add links
        for link in links:
            output.append(link)

        output.append("}")

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
        import websocket
        tmpfile = "%s.dot" % uuid.uuid4().hex
        self.to_dot(sec_list, tmpfile)
        msg = open(tmpfile).read()
        subprocess.call("rm -f %s" % tmpfile, shell=True)
        ws_url = "ws://localhost:8989/spp_ws"
        ws = websocket.create_connection(ws_url)
        ws.send(msg)
        ws.close()

    def to_term(self, sec_list, size):
        tmpfile = "%s.jpg" % uuid.uuid4().hex
        self.to_img(sec_list, tmpfile)
        from distutils import spawn

        # TODO(yasufum) Add check for using only supported terminal

        if spawn.find_executable("img2sixel") is not None:
            img_cmd = "img2sixel"
        else:
            imgcat = "%s/%s/imgcat" % (
                os.path.dirname(__file__), '3rd_party')
            if os.path.exists(imgcat) is True:
                img_cmd = imgcat
            else:
                img_cmd = None

        if img_cmd is not None:
            # Resize image to fit the terminal
            img_size = size
            cmd = "convert -resize %s %s %s" % (img_size, tmpfile, tmpfile)
            subprocess.call(cmd, shell=True)
            subprocess.call("%s %s" % (img_cmd, tmpfile), shell=True)
            subprocess.call(["rm", "-f", tmpfile])
        else:
            print("img2sixel (or imgcat.sh for MacOS) not found!")
            topo_doc = "https://spp.readthedocs.io/en/latest/"
            topo_doc += "commands/experimental.html"
            print("See '%s' for required packages." % topo_doc)

    def format_sec_status(self, sec_id, stat):
        """Return formatted secondary status as a hash

        By running status command on controller, status is sent from
        secondary process and receiving message is displayed.

        This is an example of receiving status message.

        spp > sec 1;status
        status: idling
        ports:
          - 'phy:0 -> vhost:1'
          - 'phy:1'
          - 'ring:0'
          - 'vhost:1 -> ring:0'

        This method returns a result as following.
        {
            'sec_id': '1',
            'status': 'idling',
            'ports': [
                 {
                     'rid': 'phy:0',
                     'out': 'vhost:0',
                     'iface': {'type': 'phy', 'id': '0'}
                 },
                 {
                     'rid': 'phy:1',
                     'out': None,
                     'iface': {'type': 'phy', 'id': '1'}
                 }
            ],
            'sec_id': '2',
            ...
        }
        """

        # Clean sec stat message
        stat = stat.replace("\x00", "")
        stat = stat.replace("'", "")

        stat_obj = yaml.load(stat)
        res = {}
        res['sec_id'] = sec_id
        res['status'] = stat_obj['status']

        port_list = []
        try:
            if stat_obj['ports'] is None:
                return None

            ports = stat_obj['ports'].split(',')
            for port_info in ports:
                rid, outport = port_info.split('-')
                if outport == 'null':
                    outport = None

                itype, pid = rid.split(':')
                iface = {'type': itype, 'id': pid}
                port_list.append({
                    'rid': rid,
                    'out': outport,
                    'iface': iface
                })

            res["ports"] = port_list
            return res

        except Exception:
            traceback.print_exc()
            return None
