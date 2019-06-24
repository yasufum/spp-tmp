# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation

import os
import re
import socket
import subprocess
import traceback
import uuid
import yaml


class SppTopo(object):
    """Setup and output network topology for topo command

    Topo command supports four types of output.
    * terminal (but very few terminals supporting to display images)
    * browser (websocket server is required)
    * image file (jpg, png, bmp)
    * text (dot, json, yaml)
    """

    delim_node = '_'

    def __init__(self, spp_ctl_cli, subgraphs, size):
        self.spp_ctl_cli = spp_ctl_cli
        self.subgraphs = subgraphs
        self.graph_size = size

    def run(self, args, sec_ids):
        args_ary = args.split()
        if len(args_ary) == 0:
            print("Usage: topo dst [ftype]")
            return False
        elif (args_ary[0] == "term") or (args_ary[0] == "http"):
            self.show(args_ary[0], sec_ids, self.graph_size)
        elif len(args_ary) == 1:
            ftype = args_ary[0].split(".")[-1]
            self.output(args_ary[0], sec_ids, ftype)
        elif len(args_ary) == 2:
            self.output(args_ary[0], sec_ids, args_ary[1])
        else:
            print("Usage: topo dst [ftype]")

    def show(self, dtype, sec_ids, size):
        res_ary = []
        error_codes = self.spp_ctl_cli.rest_common_error_codes

        for sec_id in sec_ids:
            res = self.spp_ctl_cli.get('nfvs/%d' % sec_id)
            if res.status_code == 200:
                res_ary.append(res.json())
            elif res.status_code in error_codes:
                # Print default error message
                pass
            else:
                # Ignore unknown response because no problem for drawing graph
                pass

        if dtype == "http":
            self.to_http(res_ary)
        elif dtype == "term":
            self.to_term(res_ary, size)
        else:
            print("Invalid file type")

    def output(self, fname, sec_ids, ftype="dot"):
        res_ary = []
        error_codes = self.spp_ctl_cli.rest_common_error_codes

        for sec_id in sec_ids:
            res = self.spp_ctl_cli.get('nfvs/%d' % sec_id)
            if res.status_code == 200:
                res_ary.append(res.json())
            elif res.status_code in error_codes:
                # Print default error message
                pass
            else:
                # Ignore unknown response because no problem for drawing graph
                pass

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

        node_template = '%s' + self.delim_node + '%s'

        phys = []
        rings = []
        vhosts = []
        links = []

        # parse status message from sec.
        for sec in sec_list:
            if sec is None:
                continue
            for port in sec['ports']:
                if self._is_valid_port(port):
                    r_type = port.split(':')[0]
                    # TODO(yasufum) change decision of r_type smarter
                    if r_type == 'phy':
                        phys.append(port)
                    elif r_type == 'ring':
                        rings.append(port)
                    elif r_type == 'vhost':
                        vhosts.append(port)
                    # TODO(yasufum) add drawing pcap and nullpmd
                    elif r_type == 'pcap':
                        pass
                    elif r_type == 'nullpmd':
                        pass
                    else:
                        raise ValueError(
                            "Invaid interface type: %s" % r_type)

            for patch in sec['patches']:
                if sec['status'] == 'running':
                    l_style = LINE_STYLE["running"]
                else:
                    l_style = LINE_STYLE["idling"]
                attrs = '[label="%s", color="%s", style="%s"]' % (
                    "sec%d" % sec["client-id"],
                    SEC_COLORS[sec["client-id"]],
                    l_style
                )
                link_style = node_template + ' %s ' + node_template + '%s;'

                if self._is_valid_port(patch['src']):
                    src_type, src_id = patch['src'].split(':')
                if self._is_valid_port(patch['dst']):
                    dst_type, dst_id = patch['dst'].split(':')

                tmp = link_style % (src_type, src_id, LINK_TYPE,
                                    dst_type, dst_id, attrs)
                links.append(tmp)

        output = ["%s spp{" % GRAPH_TYPE]
        output.append("newrank=true;")
        output.append(node_attrs)

        phy_nodes = []
        for node in phys:
            r_type, r_id = node.split(':')
            phy_nodes.append(
                node_template % (r_type, r_id))
        phy_nodes = list(set(phy_nodes))
        for node in phy_nodes:
            label = re.sub(r'%s' % self.delim_node, ':', node)
            output.append(
                '%s[label="%s", fillcolor="%s"];' % (
                    node, label, PORT_COLORS["phy"]))

        ring_nodes = []
        for node in rings:
            r_type, r_id = node.split(':')
            ring_nodes.append(node_template % (r_type, r_id))
        ring_nodes = list(set(ring_nodes))
        for node in ring_nodes:
            label = re.sub(r'%s' % self.delim_node, ':', node)
            output.append(
                '%s[label="%s", fillcolor="%s"];' % (
                    node, label, PORT_COLORS["ring"]))

        vhost_nodes = []
        for node in vhosts:
            r_type, r_id = node.split(':')
            vhost_nodes.append(node_template % (r_type, r_id))
        vhost_nodes = list(set(vhost_nodes))
        for node in vhost_nodes:
            label = re.sub(r'%s' % self.delim_node, ':', node)
            output.append(
                '%s[label="%s", fillcolor="%s"];' % (
                    node, label, PORT_COLORS["vhost"]))

        # Align the same type of nodes with rank attribute
        output.append(
            '{rank=same; %s}' % ("; ".join(ring_nodes)))
        output.append(
            '{rank=same; %s}' % ("; ".join(vhost_nodes)))

        # Decide the bottom, phy or vhost
        rank_style = '{rank=max; %s}' % node_template
        if len(phys) > 0:
            r_type, r_id = phys[0].split(':')
        elif len(vhosts) > 0:
            r_type, r_id = vhosts[0].split(':')
        output.append(rank_style % (r_type, r_id))

        # TODO(yasufum) check if it is needed, or is not needed for vhost_nodes
        if len(phy_nodes) > 0:
            output.append(
                '{rank=same; %s}' % ("; ".join(phy_nodes)))

        # Add subgraph
        ssgs = []
        if len(self.subgraphs) > 0:
            cnt = 1
            for label, val in self.subgraphs.items():
                cluster_id = "cluster%d" % cnt
                ssg_label = label
                ssg_ports = re.sub(r'%s' % ':', self.delim_node, val)
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
        try:
            ws = websocket.create_connection(ws_url)
            ws.send(msg)
            ws.close()
        except socket.error:
            print('Error: Connection refused! Is running websocket server?')

    def to_term(self, sec_list, size):
        tmpfile = "%s.jpg" % uuid.uuid4().hex
        self.to_img(sec_list, tmpfile)
        from distutils import spawn

        # TODO(yasufum) add check for using only supported terminal
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

    def resize_graph(self, args):
        if args == '':
            print(self.graph_size)
        else:
            if '%' in args:
                self.graph_size = args
                print(self.graph_size)
            elif '.' in args:
                ii = float(args) * 100
                self.graph_size = str(ii) + '%'
                print(self.graph_size)
            else:  # TODO(yasufum) add check for no number
                self.graph_size = str(float(args) * 100) + '%'
                print(self.graph_size)

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

        stat_obj = yaml.load(stat, Loader=yaml.FullLoader)
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

    def complete(self, text, line, begidx, endidx):
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

    def _is_valid_port(self, port):
        """Check if port's format is valid.

        Return True if the format is 'r_type:r_id', for example, 'phy:0'.
        """

        if (':' in port) and (len(port.split(':')) == 2):
            return True
        else:
            return False

    @classmethod
    def help(cls):
        msg = """Output network topology.

        Support four types of output.
        * terminal (but very few terminals supporting to display images)
        * browser (websocket server is required)
        * image file (jpg, png, bmp)
        * text (dot, js or json, yml or yaml)

        spp > topo term  # terminal
        spp > topo http  # browser
        spp > topo network_conf.jpg  # image
        spp > topo network_conf.dot  # text
        spp > topo network_conf.js# text
        """

        print(msg)

    @classmethod
    def help_resize(cls):
        msg = """Change the size of the image of topo command.

        You can specify the size by percentage or ratio.

        spp > topo resize 60%  # percentage
        spp > topo resize 0.6  # ratio
        """

        print(msg)

    @classmethod
    def help_subgraph(cls):
        msg = """Edit subgarph for topo command.

        Subgraph is a group of object defined in dot language. For topo
        command, it is used for grouping resources of each of VM or
        container to topology be more understandable.

        (1) Add subgraph labeled 'vm1'.
            spp > topo_subgraph add vm1 vhost:1;vhost:2

        (2) Delete subgraph 'vm1'.
            spp > topo_subgraph del vm1

        (3) Show subgraphs by running topo_subgraph without args.
            spp > topo_subgraph
            label: vm1	subgraph: "vhost:1;vhost:2"
        """

        print(msg)
