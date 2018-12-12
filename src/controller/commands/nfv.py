# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

from .. import spp_common


class SppNfv(object):
    """Exec spp_nfv command.

    SppNfv lass is intended to be used in Shell class as a delegator for
    running 'nfv' command.

    'self.command()' is called from do_nfv() and 'self.complete()' is called
    from complete_nfv() of both of which is defined in Shell.
    """

    # All of commands and sub-commands used for validation and completion.
    NFV_CMDS = ['status', 'exit', 'forward', 'stop', 'add', 'patch', 'del']

    def __init__(self, spp_ctl_cli, sec_id, use_cache=False):
        self.spp_ctl_cli = spp_ctl_cli
        self.sec_id = sec_id
        self.ports = []  # registered ports
        self.patchs = []

        # Call REST API each time of completion if it is True.
        self.use_cache = use_cache

    def run(self, cmdline):
        """Called from do_nfv() to Send command to secondary process."""

        cmd = cmdline.split(' ')[0]
        params = cmdline.split(' ')[1:]

        if cmd == 'status':
            res = self.spp_ctl_cli.get('nfvs/%d' % self.sec_id)
            if res is not None:
                error_codes = self.spp_ctl_cli.rest_common_error_codes
                if res.status_code == 200:
                    self.print_nfv_status(res.json())
                elif res.status_code in error_codes:
                    pass
                else:
                    print('Error: unknown response.')

        elif cmd == 'add':
            if self.use_cache is True:
                self.ports.append(params[0])

            req_params = {'action': 'add', 'port': params[0]}

            res = self.spp_ctl_cli.put('nfvs/%d/ports' %
                                       self.sec_id, req_params)
            if res is not None:
                error_codes = self.spp_ctl_cli.rest_common_error_codes
                if res.status_code == 204:
                    print('Add %s.' % params[0])
                elif res.status_code in error_codes:
                    pass
                else:
                    print('Error: unknown response.')

        elif cmd == 'del':
            if self.use_cache is True:
                if params[0] in self.ports:
                    self.ports.remove(params[0])

            req_params = {'action': 'del', 'port': params[0]}
            res = self.spp_ctl_cli.put('nfvs/%d/ports' %
                                       self.sec_id, req_params)
            if res is not None:
                error_codes = self.spp_ctl_cli.rest_common_error_codes
                if res.status_code == 204:
                    print('Delete %s.' % params[0])
                elif res.status_code in error_codes:
                    pass
                else:
                    print('Error: unknown response.')

        elif cmd == 'forward' or cmd == 'stop':
            if cmd == 'forward':
                req_params = {'action': 'start'}
            elif cmd == 'stop':
                req_params = {'action': 'stop'}
            else:
                print('Unknown command. "forward" or "stop"?')

            res = self.spp_ctl_cli.put('nfvs/%d/forward' %
                                       self.sec_id, req_params)
            if res is not None:
                error_codes = self.spp_ctl_cli.rest_common_error_codes
                if res.status_code == 204:
                    if cmd == 'forward':
                        print('Start forwarding.')
                    else:
                        print('Stop forwarding.')
                elif res.status_code in error_codes:
                    pass
                else:
                    print('Error: unknown response.')

        elif cmd == 'patch':
            if params[0] == 'reset':
                res = self.spp_ctl_cli.delete('nfvs/%d/patches' % self.sec_id)
                if res is not None:
                    error_codes = self.spp_ctl_cli.rest_common_error_codes
                    if res.status_code == 204:
                        print('Clear all of patches.')
                    elif res.status_code in error_codes:
                        pass
                    else:
                        print('Error: unknown response.')
            else:
                req_params = {'src': params[0], 'dst': params[1]}
                res = self.spp_ctl_cli.put(
                        'nfvs/%d/patches' % self.sec_id, req_params)
                if res is not None:
                    error_codes = self.spp_ctl_cli.rest_common_error_codes
                    if res.status_code == 204:
                        print('Patch ports (%s -> %s).' % (
                            params[0], params[1]))
                    elif res.status_code in error_codes:
                        pass
                    else:
                        print('Error: unknown response.')

        elif cmd == 'exit':
            res = self.spp_ctl_cli.delete('nfvs/%d' % self.sec_id)
            if res is not None:
                error_codes = self.spp_ctl_cli.rest_common_error_codes
                if res.status_code == 204:
                    print('Exit nfv %d' % self.sec_id)
                elif res.status_code in error_codes:
                    pass
                else:
                    print('Error: unknown response.')

        else:
            print('Invalid command "%s".' % cmd)

    def print_nfv_status(self, json_obj):
        """Parse and print message from SPP secondary.

        Print status received from secondary.

          spp > nfv 1;status
          - status: idling
          - ports:
            - phy:0 -> ring:0
            - phy:1

        The format of the received message is JSON and ended with
        series of null character "\x00".

          {"client-id":1,...,"patches":[{"src":"phy:0"...},...]}'\x00..
        """

        nfv_attr = json_obj
        print('- status: %s' % nfv_attr['status'])
        print('- ports:')
        for port in nfv_attr['ports']:
            dst = None
            for patch in nfv_attr['patches']:
                if patch['src'] == port:
                    dst = patch['dst']

            if dst is None:
                print('  - %s' % port)
            else:
                print('  - %s -> %s' % (port, dst))

    def get_registered_ports(self):
        res = self.spp_ctl_cli.get('nfvs/%d' % self.sec_id)
        if res is not None:
            error_codes = self.spp_ctl_cli.rest_common_error_codes
            if res.status_code == 200:
                return res.json()['ports']
            elif res.status_code in error_codes:
                pass
            else:
                print('Error: unknown response.')

    def get_registered_patches(self):
        res = self.spp_ctl_cli.get('nfvs/%d' % self.sec_id)
        if res is not None:
            error_codes = self.spp_ctl_cli.rest_common_error_codes
            if res.status_code == 200:
                return res.json()['patches']
            elif res.status_code in error_codes:
                pass
            else:
                print('Error: unknown response.')

    def complete(self, sec_ids, text, line, begidx, endidx):
        """Completion for spp_nfv commands.

        Called from complete_nfv() to complete secondary command.
        """

        try:
            completions = []
            tokens = line.split(';')

            if len(tokens) == 2:
                sub_tokens = tokens[1].split(' ')

                if len(sub_tokens) == 1:
                    if not (sub_tokens[0] in self.NFV_CMDS):
                        completions = self._compl_first_tokens(sub_tokens[0])

                else:
                    if sub_tokens[0] in ['status', 'exit', 'forward', 'stop']:
                        if len(sub_tokens) < 2:
                            if sub_tokens[0].startswith(sub_tokens[1]):
                                completions = [sub_tokens[0]]

                    elif sub_tokens[0] == 'add':
                        completions = self._compl_add(sub_tokens)

                    elif sub_tokens[0] == 'del':
                        completions = self._compl_del(sub_tokens)

                    elif sub_tokens[0] == 'patch':
                        completions = self._compl_patch(sub_tokens)

            return completions

        except Exception as e:
            print(e)

    def _compl_first_tokens(self, token):
        res = []
        for kw in self.NFV_CMDS:
            if kw.startswith(token):
                res.append(kw)
        return res

    def _compl_add(self, sub_tokens):
        if len(sub_tokens) < 3:
            res = []

            port_types = spp_common.PORT_TYPES[:]
            port_types.remove('phy')

            for kw in port_types:
                if kw.startswith(sub_tokens[1]):
                    res.append(kw + ':')
            return res

    def _compl_del(self, sub_tokens):
        if len(sub_tokens) < 3:
            res = []

            if self.use_cache is False:
                self.ports = self.get_registered_ports()

            for kw in self.ports:
                if kw.startswith(sub_tokens[1]):
                    if ':' in sub_tokens[1]:  # exp, 'ring:' or 'ring:0'
                        res.append(kw.split(':')[1])
                    else:
                        res.append(kw)

            for p in res:
                if p.startswith('phy:'):
                    res.remove(p)

            return res

    def _compl_patch(self, sub_tokens):
        # Patch command consists of three tokens max, for instance,
        # `nfv 1; patch phy:0 ring:1`.
        if len(sub_tokens) < 4:
            res = []

            if self.use_cache is False:
                self.ports = self.get_registered_ports()
                self.patches = self.get_registered_patches()

            # Get patched ports of src and dst to be used for completion.
            src_ports = []
            dst_ports = []
            for pt in self.patches:
                src_ports.append(pt['src'])
                dst_ports.append(pt['dst'])

            # Remove patched ports from candidates.
            target_idx = len(sub_tokens) - 1  # target is src or dst
            tmp_ports = self.ports[:]  # candidates
            if target_idx == 1:  # find src port
                # If some of ports are patched, `reset` should be included.
                if self.patches != []:
                    tmp_ports.append('reset')
                for pt in src_ports:
                    tmp_ports.remove(pt)  # remove patched ports
            else:  # find dst port
                # If `reset` is given, no need to show dst ports.
                if sub_tokens[target_idx - 1] == 'reset':
                    tmp_ports = []
                else:
                    for pt in dst_ports:
                        tmp_ports.remove(pt)

            # Return candidates.
            for kw in tmp_ports:
                if kw.startswith(sub_tokens[target_idx]):
                    # Completion does not work correctly if `:` is included in
                    # tokens. Required to create keyword only after `:`.
                    if ':' in sub_tokens[target_idx]:  # 'ring:' or 'ring:0'
                        res.append(kw.split(':')[1])  # add only after `:`
                    else:
                        res.append(kw)

            return res
