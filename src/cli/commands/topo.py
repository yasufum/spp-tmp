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

    def __init__(self, spp_ctl_cli, subgraphs, cli_config):
        self.spp_ctl_cli = spp_ctl_cli
        self.subgraphs = subgraphs
        self.graph_size = None

        # Graphviz params
        topo_file = '{dir}/../config/topo.yml'.format(
                dir=os.path.dirname(__file__))
        topo_conf = yaml.load(open(topo_file), Loader=yaml.FullLoader)
        self.SEC_COLORS = topo_conf['topo_sec_colors']['val']
        self.PORT_COLORS = topo_conf['topo_port_colors']['val']
        self.LINE_STYLE = {"running": "solid", "idling": "dashed"}
        self.GRAPH_TYPE = "digraph"
        self.LINK_TYPE = "->"

        if self.resize(cli_config['topo_size']['val']) is not True:
            print('Config "topo_size" is invalid value.')
            exit()

    def run(self, args):
        args_ary = args.split()
        if len(args_ary) == 0:
            print("Usage: topo dst [ftype]")
            return False
        elif args_ary[0] == "term":
            self.to_term(self.graph_size)
        elif args_ary[0] == "http":
            self.to_http()
        elif len(args_ary) == 1:  # find ftype from filename
            ftype = args_ary[0].split(".")[-1]
            self.to_file(args_ary[0], ftype)
        elif len(args_ary) == 2:  # ftype is given as args_ary[1]
            self.to_file(args_ary[0], args_ary[1])
        else:
            print("Usage: topo dst [ftype]")

    def resize(self, size):
        """Parse given size and set to self.graph_size.

        The format of `size` is percentage or ratio. Return True if succeeded
        to parse, or False if invalid format.
        """

        size = str(size)
        matched = re.match(r'(\d+)%$', size)
        if matched:  # percentage
            i = int(matched.group(1))
            if i > 0 and i <= 100:
                self.graph_size = size
                return True
            else:
                return False
        elif re.match(r'0\.\d+$', size):  # ratio
            i = float(size) * 100
            self.graph_size = str(i) + '%'
            return True
        elif size == '1':
            self.graph_size = '100%'
            return True
        else:
            return False

    def to_file(self, fname, ftype="dot"):
        if ftype == "dot":
            self.to_dot(fname)
        elif ftype == "json" or ftype == "js":
            self.to_json(fname)
        elif ftype == "yaml" or ftype == "yml":
            self.to_yaml(fname)
        elif ftype == "jpg" or ftype == "png" or ftype == "bmp":
            self.to_img(fname)
        else:
            print("Invalid file type")
            return False
        print("Create topology: '{fname}'".format(fname=fname))
        return True

    def to_dot(self, output_fname):
        """Output dot script."""

        node_attrs = 'node[shape="rectangle", style="filled"];'

        node_template = '{}' + self.delim_node + '{}'

        phys = []
        rings = []
        vhosts = []
        links = []

        # parse status message from sec.
        for sec in self.spp_ctl_cli.get_sec_procs('nfv'):
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
                            "Invaid interface type: {rtype}".format(
                                rtype=r_type))

            for patch in sec['patches']:
                if sec['status'] == 'running':
                    l_style = self.LINE_STYLE["running"]
                else:
                    l_style = self.LINE_STYLE["idling"]
                attrs = '[label="%s", color="%s", style="%s"]' % (
                    "sec%d" % sec["client-id"],
                    self.SEC_COLORS[sec["client-id"]],
                    l_style
                )
                link_style = node_template + ' {} ' + node_template + '{};'

                if self._is_valid_port(patch['src']):
                    src_type, src_id = patch['src'].split(':')
                if self._is_valid_port(patch['dst']):
                    dst_type, dst_id = patch['dst'].split(':')

                tmp = link_style.format(src_type, src_id, self.LINK_TYPE,
                                        dst_type, dst_id, attrs)
                links.append(tmp)

        output = ["{} spp{{".format(self.GRAPH_TYPE)]
        output.append("newrank=true;")
        output.append(node_attrs)

        phy_nodes = []
        for node in phys:
            r_type, r_id = node.split(':')
            phy_nodes.append(
                node_template.format(r_type, r_id))
        phy_nodes = list(set(phy_nodes))
        for node in phy_nodes:
            label = re.sub(r'{}'.format(self.delim_node), ':', node)
            output.append(
                '{nd}[label="{lbl}", fillcolor="{col}"];'.format(
                    nd=node, lbl=label, col=self.PORT_COLORS["phy"]))

        ring_nodes = []
        for node in rings:
            r_type, r_id = node.split(':')
            ring_nodes.append(node_template.format(r_type, r_id))
        ring_nodes = list(set(ring_nodes))
        for node in ring_nodes:
            label = re.sub(r'{}'.format(self.delim_node), ':', node)
            output.append(
                '{nd}[label="{lbl}", fillcolor="{col}"];'.format(
                    nd=node, lbl=label, col=self.PORT_COLORS["ring"]))

        vhost_nodes = []
        for node in vhosts:
            r_type, r_id = node.split(':')
            vhost_nodes.append(node_template.format(r_type, r_id))
        vhost_nodes = list(set(vhost_nodes))
        for node in vhost_nodes:
            label = re.sub(r'{}'.format(self.delim_node), ':', node)
            output.append(
                '{nd}[label="{lbl}", fillcolor="{col}"];'.format(
                    nd=node, lbl=label, col=self.PORT_COLORS["vhost"]))

        # Align the same type of nodes with rank attribute
        output.append(
            '{{rank=same; {rn}}}'.format(rn="; ".join(ring_nodes)))
        output.append(
            '{{rank=same; {vn}}}'.format(vn="; ".join(vhost_nodes)))

        # Decide the bottom, phy or vhost
        rank_style = '{{rank=max; ' + node_template + '}}'
        if len(phys) > 0:
            r_type, r_id = phys[0].split(':')
        elif len(vhosts) > 0:
            r_type, r_id = vhosts[0].split(':')
        output.append(rank_style.format(r_type, r_id))

        # TODO(yasufum) check if it is needed, or is not needed for vhost_nodes
        if len(phy_nodes) > 0:
            output.append(
                '{{rank=same; {pn}}}'.format(pn="; ".join(phy_nodes)))

        # Add subgraph
        ssgs = []
        if len(self.subgraphs) > 0:
            cnt = 1
            for label, val in self.subgraphs.items():
                cluster_id = "cluster{}".format(cnt)
                ssg_label = label
                ssg_ports = re.sub(r'%s' % ':', self.delim_node, val)
                ssg = 'subgraph {cid} {{label="{ssgl}" {ssgp}}}'.format(
                    cid=cluster_id, ssgl=ssg_label, ssgp=ssg_ports)
                ssgs.append(ssg)
                cnt += 1

        cluster_id = "cluster0"
        sg_label = "Host"
        sg_ports = "; ".join(phy_nodes + ring_nodes)
        if len(ssgs) == 0:
            output.append(
                'subgraph {cid} {{label="{sgl}" {sgp}}}'.format(
                    cid=cluster_id, sgl=sg_label, sgp=sg_ports))
        else:
            tmp = 'label="{sgl}" {sgp}'.format(sgl=sg_label, sgp=sg_ports)
            contents = [tmp] + ssgs
            output.append(
                'subgraph {cid} {{{cont}}}'.format(
                    cid=cluster_id, cont='; '.join(contents)))

        # Add links
        for link in links:
            output.append(link)

        output.append("}")

        f = open(output_fname, "w+")
        f.write("\n".join(output))
        f.close()

    def to_json(self, output_fname):
        import json
        f = open(output_fname, "w+")
        sec_list = self.spp_ctl_cli.get_sec_procs('nfv')
        f.write(json.dumps(sec_list))
        f.close()

    def to_yaml(self, output_fname):
        import yaml
        f = open(output_fname, "w+")
        sec_list = self.spp_ctl_cli.get_sec_procs('nfv')
        f.write(yaml.dump(sec_list))
        f.close()

    def to_img(self, output_fname):
        tmpfile = "{fn}.dot".format(fn=uuid.uuid4().hex)
        self.to_dot(tmpfile)
        fmt = output_fname.split(".")[-1]
        cmd = "dot -T{fmt} {dotf} -o {of}".format(
                fmt=fmt, dotf=tmpfile, of=output_fname)
        subprocess.call(cmd, shell=True)
        subprocess.call("rm -f {tmpf}".format(tmpf=tmpfile), shell=True)

    def to_http(self):
        import websocket
        tmpfile = "{fn}.dot".format(fn=uuid.uuid4().hex)
        self.to_dot(tmpfile)
        msg = open(tmpfile).read()
        subprocess.call("rm -f {tmpf}".format(tmpf=tmpfile), shell=True)
        # TODO(yasufum) change to be able to use other than `localhost`.
        ws_url = "ws://localhost:8989/spp_ws"
        try:
            ws = websocket.create_connection(ws_url)
            ws.send(msg)
            ws.close()
        except socket.error:
            print('Error: Connection refused! Is running websocket server?')

    def to_term(self, size):
        tmpfile = "{fn}.jpg".format(fn=uuid.uuid4().hex)
        self.to_img(tmpfile)
        from distutils import spawn

        # TODO(yasufum) add check for using only supported terminal
        if spawn.find_executable("img2sixel") is not None:
            img_cmd = "img2sixel"
        else:
            imgcat = "{pdir}/{sdir}/imgcat".format(
                pdir=os.path.dirname(__file__), sdir='3rd_party')
            if os.path.exists(imgcat) is True:
                img_cmd = imgcat
            else:
                img_cmd = None

        if img_cmd is not None:
            # Resize image to fit the terminal
            img_size = size
            cmd = "convert -resize {size} {fn1} {fn2}".format(
                    size=img_size, fn1=tmpfile, fn2=tmpfile)
            subprocess.call(cmd, shell=True)
            subprocess.call("{cmd} {fn}".format(cmd=img_cmd, fn=tmpfile),
                            shell=True)
            subprocess.call(["rm", "-f", tmpfile])
        else:
            print("img2sixel (or imgcat.sh for MacOS) not found!")
            topo_doc = "https://spp.readthedocs.io/en/latest/"
            topo_doc += "commands/experimental.html"
            print("See '{url}' for required packages.".format(url=topo_doc))

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
