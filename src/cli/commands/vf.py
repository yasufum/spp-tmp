# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation


class SppVf(object):
    """Exec SPP VF command.

    SppVf class is intended to be used in Shell class as a delegator
    for running 'vf' command.

    'self.command()' is called from do_vf() and 'self.complete()' is called
    from complete_vf() of both of which is defined in Shell.
    """

    # All of commands and sub-commands used for validation and completion.
    VF_CMDS = {
            'status': None,
            'exit': None,
            'component': ['start', 'stop'],
            'port': ['add', 'del'],
            'classifier_table': ['add', 'del']}

    WORKER_TYPES = ['forward', 'merge', 'classifier']

    def __init__(self, spp_ctl_cli, sec_id, use_cache=False):
        self.spp_ctl_cli = spp_ctl_cli
        self.sec_id = sec_id

        # Update 'self.worker_names' and 'self.unused_core_ids' each time
        # 'self.run()' is called if it is 'False'.
        # True to 'True' if you do not wait for spp_vf's response.
        self.use_cache = use_cache

        # Names and core IDs of worker threads
        vf_status = self._get_status(self.sec_id)

        core_ids = vf_status['core_ids']
        for wk in vf_status['workers']:
            if wk['core_id'] in core_ids:
                core_ids.remove(wk['core_id'])
        self.unused_core_ids = core_ids  # used while completion to exclude

        self.workers = vf_status['workers']
        self.worker_names = [attr['name'] for attr in vf_status['workers']]

    def run(self, cmdline):
        """Called from do_sec() to Send command to secondary process."""

        # update status each time if configured not to use cache
        if self.use_cache is False:
            vf_status = self._get_status(self.sec_id)

            core_ids = vf_status['core_ids']
            for wk in vf_status['workers']:
                if wk['core_id'] in core_ids:
                    core_ids.remove(wk['core_id'])
            self.unused_core_ids = core_ids  # used while completion to exclude

            self.workers = vf_status['workers']
            self.worker_names = [attr['name'] for attr in vf_status['workers']]

        cmd = cmdline.split(' ')[0]
        params = cmdline.split(' ')[1:]

        if cmd == 'status':
            self._run_status()

        elif cmd == 'component':
            self._run_component(params)

        elif cmd == 'port':
            self._run_port(params)

        elif cmd == 'classifier_table':
            self._run_cls_table(params)

        elif cmd == 'exit':
            self._run_exit()

        else:
            print('Invalid command "%s".' % cmd)

    def print_status(self, json_obj):
        """Parse and print message from SPP VF.

        Print status received from spp_vf.

          spp > vf; status
          Basic Information:
            - client-id: 3
            - ports: [phy:0, phy:1]
            - lcore_ids:
              - master: 1
              - slaves: [2, 3]
          Classifier Table:
            - "FA:16:3E:7D:CC:35", ring:0
            - "FA:17:3E:7D:CC:55", ring:1
          Components:
            - core:1, "fwdr1" (type: forwarder)
              - rx: ring:0
              - tx: vhost:0
            - core:2, "mgr11" (type: merger)
              - rx: ring:1, vlan (operation: add, id: 101, pcp: 0)
              - tx: ring:2, vlan (operation: del)
            ...

        """

        # Extract slave lcore IDs first
        slave_lcore_ids = []
        for worker in json_obj['components']:
            slave_lcore_ids.append(str(worker['core']))

        # Basic Information
        print('Basic Information:')
        print('  - client-id: {}'.format(json_obj['client-id']))
        print('  - ports: [{}]'.format(', '.join(json_obj['ports'])))
        print('  - lcore_ids:')
        print('    - master: {}'.format(json_obj['master-lcore']))
        print('    - slaves: [{}]'.format(', '.join(slave_lcore_ids)))

        # Classifier Table
        print('Classifier Table:')
        if len(json_obj['classifier_table']) == 0:
            print('  No entries.')
        for ct in json_obj['classifier_table']:
            print('  - %s, %s' % (ct['value'], ct['port']))

        # Componennts
        print('Components:')
        for worker in json_obj['components']:
            if 'name' in worker.keys():
                print("  - core:%d '%s' (type: %s)" % (
                      worker['core'], worker['name'], worker['type']))
                for pt_dir in ['rx', 'tx']:
                    pt = '%s_port' % pt_dir
                    for attr in worker[pt]:
                        if attr['vlan']['operation'] == 'add':
                            msg = '    - %s: %s ' + \
                                  '(vlan operation: %s, id: %d, pcp: %d)'
                            print(msg % (pt_dir, attr['port'],
                                         attr['vlan']['operation'],
                                         attr['vlan']['id'],
                                         attr['vlan']['pcp']))
                        elif attr['vlan']['operation'] == 'del':
                            msg = '    - %s: %s (vlan operation: %s)'
                            print(msg % (pt_dir, attr['port'],
                                  attr['vlan']['operation']))
                        else:
                            msg = '    - %s: %s'
                            print(msg % (pt_dir, attr['port']))

            else:
                # TODO(yasufum) should change 'unuse' to 'unused'
                print("  - core:%d '' (type: unuse)" % worker['core'])

    def complete(self, sec_ids, text, line, begidx, endidx):
        """Completion for spp_vf commands.

        Called from complete_vf() to complete.
        """

        try:
            completions = []
            tokens = line.split(';')

            if len(tokens) == 2:
                sub_tokens = tokens[1].split(' ')

                # VF_CMDS = {
                #         'status': None,
                #         'component': ['start', 'stop'],
                #         'port': ['add', 'del'],
                #         'classifier_table': ['add', 'del']}

                if len(sub_tokens) == 1:
                    if not (sub_tokens[0] in self.VF_CMDS.keys()):
                        completions = self._compl_first_tokens(sub_tokens[0])
                else:
                    if sub_tokens[0] == 'status':
                        if len(sub_tokens) < 2:
                            if 'status'.startswith(sub_tokens[1]):
                                completions = ['status']

                    elif sub_tokens[0] == 'component':
                        completions = self._compl_component(sub_tokens)

                    elif sub_tokens[0] == 'port':
                        completions = self._compl_port(sub_tokens)

                    elif sub_tokens[0] == 'classifier_table':
                        completions = self._compl_cls_table(sub_tokens)
            return completions
        except Exception as e:
            print(e)

    def _compl_first_tokens(self, token):
        res = []
        for kw in self.VF_CMDS.keys():
            if kw.startswith(token):
                res.append(kw)
        return res

    def _get_status(self, sec_id):
        """Get status of spp_vf.

        To update status of the instance of SppVf, get the status from
        spp-ctl. This method returns the result as a dict. For considering
        behaviour of spp_vf, it is enough to return worker's name, core IDs and
        ports as the status, but might need to be update for future updates.

        # return worker's name and used core IDs, and all of core IDs.
        {
          'workers': [
            {'name': 'fw1', 'core_id': 5},
            {'name': 'mg1', 'core_id': 6},
            ...
          ],
          'core_ids': [5, 6, 7, ...],
          'ports': ['phy:0', 'phy:1', ...]
        }

        """

        status = {'workers': [], 'core_ids': [], 'ports': []}
        res = self.spp_ctl_cli.get('vfs/%d' % self.sec_id)
        if res is not None:
            if res.status_code == 200:
                json_obj = res.json()

                if 'components' in json_obj.keys():
                    for wk in json_obj['components']:
                        if 'core' in wk.keys():
                            if 'name' in wk.keys():
                                status['workers'].append(
                                        {'name': wk['name'],
                                            'core_id': wk['core']})
                            status['core_ids'].append(wk['core'])

                if 'ports' in json_obj:
                    status['ports'] = json_obj.get('ports')

        return status

    def _run_status(self):
        res = self.spp_ctl_cli.get('vfs/%d' % self.sec_id)
        if res is not None:
            error_codes = self.spp_ctl_cli.rest_common_error_codes
            if res.status_code == 200:
                self.print_status(res.json())
            elif res.status_code in error_codes:
                pass
            else:
                print('Error: unknown response.')

    def _run_component(self, params):
        if params[0] == 'start':
            req_params = {'name': params[1], 'core': int(params[2]),
                          'type': params[3]}
            res = self.spp_ctl_cli.post('vfs/%d/components' % self.sec_id,
                                        req_params)
            if res is not None:
                error_codes = self.spp_ctl_cli.rest_common_error_codes
                if res.status_code == 204:
                    print("Succeeded to start component '%s' on core:%d"
                          % (req_params['name'], req_params['core']))
                    self.worker_names.append(req_params['name'])
                    if req_params['core'] in self.unused_core_ids:
                        self.unused_core_ids.remove(req_params['core'])
                elif res.status_code in error_codes:
                    pass
                else:
                    print('Error: unknown response.')

        elif params[0] == 'stop':
            res = self.spp_ctl_cli.delete('vfs/%d/components/%s' % (
                                          self.sec_id, params[1]))
            if res is not None:
                error_codes = self.spp_ctl_cli.rest_common_error_codes
                if res.status_code == 204:
                    print("Succeeded to delete component '%s'" % params[1])

                    # update workers and core IDs
                    if params[1] in self.worker_names:
                        self.worker_names.remove(params[1])
                    for wk in self.workers:
                        if wk['name'] == params[1]:
                            self.unused_core_ids.append(wk['core_id'])
                            self.workers.remove(wk)
                            break
                elif res.status_code in error_codes:
                    pass
                else:
                    print('Error: unknown response.')

    def _run_port(self, params):
        params_index = 0
        req_params = {}
        vlan_params = {}
        name = None
        flg_mq = False

        while params_index < len(params):
            if params_index == 0:
                if params[params_index] == 'add':
                    req_params["action"] = 'attach'
                elif params[params_index] == 'del':
                    req_params["action"] = 'detach'
                else:
                    print('Error: Invalid action.')
                    return None

            elif params_index == 1:
                req_params["port"] = params[params_index]

                # Check Multi queue
                if params_index + 2 < len(params):
                    if params[params_index + 1] == "nq":
                        params_index += 2
                        req_params["port"] += "nq" + params[params_index]
                        flg_mq = True
                else:
                    print("Error: Not enough parameters.")
                    return None

            elif ((params_index == 2 and flg_mq is False) or
                    (params_index == 4 and flg_mq is True)):
                req_params["dir"] = params[params_index]

            elif ((params_index == 3 and flg_mq is False) or
                    (params_index == 5 and flg_mq is True)):
                name = params[params_index]

            elif ((params_index == 4 and flg_mq is False) or
                    (params_index == 6 and flg_mq is True)):
                if params[params_index] == "add_vlantag":
                    vlan_params["operation"] = "add"
                elif params[params_index] == "del_vlantag":
                    vlan_params["operation"] = "del"
                else:
                    print('Error: vlantag is Only add_vlantag or del_vlantag.')
                    return None

            elif ((params_index == 5 and flg_mq is False) or
                    (params_index == 7 and flg_mq is True)):
                try:
                    vlan_params["id"] = int(params[params_index])
                except Exception as _:
                    print('Error: vid is not a number.')
                    return None

            elif ((params_index == 6 and flg_mq is False) or
                    (params_index == 8 and flg_mq is True)):
                try:
                    vlan_params["pcp"] = int(params[params_index])
                except Exception as _:
                    print('Error: pcp is not a number.')
                    return None

            params_index += 1

        req_params["vlan"] = vlan_params
        res = self.spp_ctl_cli.put('vfs/%d/components/%s/ports'
                                   % (self.sec_id, name), req_params)
        if res is not None:
            error_codes = self.spp_ctl_cli.rest_common_error_codes
            if res.status_code == 204:
                print("Succeeded to %s port" % params[0])
            elif res.status_code in error_codes:
                pass
            else:
                print('Error: unknown response.')

    def _run_cls_table(self, params):
        params_index = 0
        req_params = {}
        flg_vlan = False

        # The list elements are:
        #       action, type, vlan, mac,  port
        values = [None, None, None, None, None]
        values_index = 0

        while params_index < len(params):

            if params_index == 0:
                req_params["action"] = params[params_index]

            elif params_index == 1:
                req_params["type"] = params[params_index]
                if (req_params["type"] != "vlan" and
                        req_params["type"] != "mac"):
                    print("Error: Type is only vlan or mac")
                    return None

            elif params_index == 2 and req_params["type"] == "vlan":
                req_params["vlan"] = params[params_index]
                flg_vlan = True

            elif ((params_index == 2 and flg_vlan is False) or
                    (params_index == 3 and flg_vlan is True)):
                req_params["mac_address"] = params[params_index]

            elif ((params_index == 3 and flg_vlan is False) or
                    (params_index == 4 and flg_vlan is True)):
                req_params["port"] = params[params_index]

                # Check Multi queue
                if params_index + 2 < len(params):
                    if params[params_index + 1] == "nq":
                        params_index += 2
                        req_params["port"] += "nq" + params[params_index]

            params_index += 1

        req = 'vfs/%d/classifier_table' % self.sec_id
        res = self.spp_ctl_cli.put(req, req_params)

        if res is not None:
            error_codes = self.spp_ctl_cli.rest_common_error_codes
            if res.status_code == 204:
                print("Succeeded to %s" % params[0])
            elif res.status_code in error_codes:
                pass
            else:
                print('Error: unknown response.')

    def _run_exit(self):
        """Run `exit` command."""

        res = self.spp_ctl_cli.delete('vfs/%d' % self.sec_id)
        if res is not None:
            error_codes = self.spp_ctl_cli.rest_common_error_codes
            if res.status_code == 204:
                print('Exit vf %d.' % self.sec_id)
            elif res.status_code in error_codes:
                pass
            else:
                print('Error: unknown response.')

    def _compl_component(self, sub_tokens):
        if len(sub_tokens) < 6:
            subsub_cmds = ['start', 'stop']
            res = []
            if len(sub_tokens) == 2:
                for kw in subsub_cmds:
                    if kw.startswith(sub_tokens[1]):
                        res.append(kw)
            elif len(sub_tokens) == 3:
                # 'start' takes any of names and no need
                #  check, required only for 'stop'.
                if sub_tokens[1] == 'start':
                    if 'NAME'.startswith(sub_tokens[2]):
                        res.append('NAME')
                if sub_tokens[1] == 'stop':
                    for kw in self.worker_names:
                        if kw.startswith(sub_tokens[2]):
                            res.append(kw)
            elif len(sub_tokens) == 4:
                if sub_tokens[1] == 'start':
                    for cid in [str(i) for i in self.unused_core_ids]:
                        if cid.startswith(sub_tokens[3]):
                            res.append(cid)
            elif len(sub_tokens) == 5:
                if sub_tokens[1] == 'start':
                    for wk_type in self.WORKER_TYPES:
                        if wk_type.startswith(sub_tokens[4]):
                            res.append(wk_type)
            return res

    def _compl_port(self, sub_tokens):
        res = []
        index = 0

        # compl_phase "add_del"  : candidate is add or del
        # compl_phase "res_uid"  : candidate is RES_UID
        # compl_phase "nq"       : candidate is nq
        # compl_phase "queue_no" : candidate is queue no
        # compl_phase "dir"      : candidate is DIR
        # compl_phase "name"     : candidate is NAME
        # compl_phase "vlan_tag" : candidate is vlan tag
        # compl_phase "vid"      : candidate is vid
        # compl_phase "pcp"      : candidate is pcp
        # compl_phase None       : candidate is None
        compl_phase = "add_del"
        add_or_del = None

        while index < len(sub_tokens):
            if compl_phase == "nq":
                queue_no_list = self._get_candidate_phy_queue_no(
                    sub_tokens[index - 1])

                if queue_no_list is None:
                    compl_phase = "dir"

            if ((compl_phase == "add_del") and
                    (sub_tokens[index - 1] == "port")):
                res = ["add", "del"]
                compl_phase = "res_uid"

            elif ((compl_phase == "res_uid") and
                    (sub_tokens[index - 1] == "add" or
                     sub_tokens[index - 1] == "del")):
                res = ["RES_UID"]
                compl_phase = "nq"
                add_or_del = sub_tokens[index - 1]

            elif compl_phase == "nq":
                res = ["nq"]
                compl_phase = "queue_no"

            elif compl_phase == "queue_no":
                res = queue_no_list
                compl_phase = "dir"

            elif compl_phase == "dir":
                res = ["rx", "tx"]
                compl_phase = "name"

            elif compl_phase == "name":
                res = ["NAME"]
                if add_or_del == "add":
                    compl_phase = "vlan_tag"
                else:
                    compl_phase = None

            elif compl_phase == "vlan_tag":
                res = ["add_vlantag", "del_vlantag"]
                compl_phase = "vid"

            elif (compl_phase == "vid" and
                  sub_tokens[index - 1] == "add_vlantag"):
                res = ["VID"]
                compl_phase = "pcp"

            elif compl_phase == "pcp":
                res = ["PCP"]
                compl_phase = None

            else:
                res = []

            index += 1

        res = [p for p in res
               if p.startswith(sub_tokens[len(sub_tokens) - 1])]

        return res

    def _get_candidate_phy_queue_no(self, res_uid):
        """Get phy queue_no candidate.
        If res_uid is phy and multi-queue, return queue_no in list type.
        Otherwise returns None.
        """

        try:
            port, _ = res_uid.split(":")
        except Exception as _:
            return None

        if port != "phy":
            return None

        status = self._get_status(self.sec_id)

        queue_no_list = []
        for port in status["ports"]:
            if not port.startswith(res_uid):
                continue

            try:
                _, queue_no = port.split("nq")
            except Exception as _:
                continue

            queue_no_list.append(queue_no.strip(" "))

        if len(queue_no_list) == 0:
            return None

        return queue_no_list

    def _compl_cls_table(self, sub_tokens):
        res = []
        index = 0

        # compl_phase "add_del"  : candidate is add or del
        # compl_phase "vlan_mac" : candidate is vlan or mac
        # compl_phase "vid"      : candidate is VID
        # compl_phase "mac_addr" : candidate is MAC_ADDR or default
        # compl_phase "res_uid"  : candidate is RES_UID
        # compl_phase "nq"       : candidate is nq
        # compl_phase "queue_no" : candidate is queue_no
        # compl_phase None : candidate is None
        compl_phase = "add_del"

        while index < len(sub_tokens):

            if compl_phase == "vid" and sub_tokens[index - 1] == "mac":
                compl_phase = "mac_addr"

            if compl_phase == "nq":
                queue_no_list = self._get_candidate_phy_queue_no(
                    sub_tokens[index - 1])
                if queue_no_list is None:
                    compl_phase = None

            if ((compl_phase == "add_del") and
                    (sub_tokens[index - 1] == "classifier_table")):
                res = ["add", "del"]
                compl_phase = "vlan_mac"

            elif compl_phase == "vlan_mac":
                res = ["vlan", "mac"]
                compl_phase = "vid"

            elif compl_phase == "vid" and sub_tokens[index - 1] == "vlan":
                res = ["VID"]
                compl_phase = "mac_addr"

            elif compl_phase == "mac_addr":
                res = ["MAC_ADDR", "default"]
                compl_phase = "res_uid"

            elif compl_phase == "res_uid":
                res = ["RES_UID"]
                compl_phase = "nq"

            elif compl_phase == "nq":
                res = ["nq"]
                compl_phase = "queue_no"

            elif compl_phase == "queue_no":
                res = queue_no_list
                compl_phase = None

            else:
                res = []

            index += 1

        res = [p for p in res
               if p.startswith(sub_tokens[len(sub_tokens) - 1])]

        return res

    @classmethod
    def help(cls):
        msg = """Send a command to spp_vf.

        SPP VF is a secondary process for pseudo SR-IOV features. This
        command has four sub commands.
          * status
          * component
          * port
          * classifier_table

        Each of sub commands other than 'status' takes several parameters
        for detailed operations. Notice that 'start' for launching a worker
        is replaced with 'stop' for terminating. 'add' is also replaced with
        'del' for deleting.

        Examples:

        # (1) show status of worker threads and resources
        spp > vf 1; status

        # (2) launch or terminate a worker thread with arbitrary name
        #   NAME: arbitrary name used as identifier
        #   CORE_ID: one of unused cores referred from status
        #   ROLE: role of workers, 'forward', 'merge' or 'classifier'
        spp > vf 1; component start NAME CORE_ID ROLE
        spp > vf 1; component stop NAME CORE_ID ROLE

        # (3) add or delete a port to worker of NAME
        #   RES_UID: resource UID such as 'ring:0' or 'vhost:1'
        #   DIR: 'rx' or 'tx'
        spp > vf 1; port add RES_UID DIR NAME
        spp > vf 1; port del RES_UID DIR NAME

        # (4) add or delete a port with vlan ID to worker of NAME
        #   VID: vlan ID
        #   PCP: priority code point defined in IEEE 802.1p
        spp > vf 1; port add RES_UID DIR NAME add_vlantag VID PCP
        spp > vf 1; port del RES_UID DIR NAME add_vlantag VID PCP

        # (5) add a port of deleting vlan tag
        spp > vf 1; port add RES_UID DIR NAME del_vlantag

        # (6) add or delete an entry of MAC address and resource to classify
        spp > vf 1; classifier_table add mac MAC_ADDR RES_UID
        spp > vf 1; classifier_table del mac MAC_ADDR RES_UID

        # (7) add or delete an entry of MAC address and resource with vlan ID
        spp > vf 1; classifier_table add vlan VID MAC_ADDR RES_UID
        spp > vf 1; classifier_table del vlan VID MAC_ADDR RES_UID
        """

        print(msg)
