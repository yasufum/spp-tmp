# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation


class SppSecondary(object):
    """Exec SPP secondary command.

    SppSecondaryclass is intended to be used in Shell class as a delegator
    for running 'sec' command.

    'self.command()' is called from do_sec() and 'self.complete()' is called
    from complete_sec() of both of which is defined in Shell.
    """

    # All of commands and sub-commands used for validation and completion.
    SEC_CMDS = ['status', 'exit', 'forward', 'stop', 'add', 'patch', 'del']
    SEC_SUBCMDS = ['vhost', 'ring', 'pcap', 'nullpmd']

    def __init__(self, spp_ctl_cli):
        self.spp_ctl_cli = spp_ctl_cli

    def run(self, sec_id, cmdline):
        """Called from do_sec() to Send command to secondary process."""

        cmd = cmdline.split(' ')[0]
        params = cmdline.split(' ')[1:]

        if cmd == 'status':
            res = self.spp_ctl_cli.get('nfvs/%d' % sec_id)
            if res is not None:
                error_codes = self.spp_ctl_cli.rest_common_error_codes
                if res.status_code == 200:
                    self.print_sec_status(res.json())
                elif res.status_code in error_codes:
                    pass
                else:
                    print('Error: unknown response.')

        elif cmd == 'add':
            req_params = {'action': 'add', 'port': params[0]}
            res = self.spp_ctl_cli.put('nfvs/%d/ports' % sec_id, req_params)
            if res is not None:
                error_codes = self.spp_ctl_cli.rest_common_error_codes
                if res.status_code == 204:
                    print('Add %s.' % params[0])
                elif res.status_code in error_codes:
                    pass
                else:
                    print('Error: unknown response.')

        elif cmd == 'del':
            req_params = {'action': 'del', 'port': params[0]}
            res = self.spp_ctl_cli.put('nfvs/%d/ports' % sec_id, req_params)
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

            res = self.spp_ctl_cli.put('nfvs/%d/forward' % sec_id, req_params)
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
                res = self.spp_ctl_cli.delete('nfvs/%d/patches' % sec_id)
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
                        'nfvs/%d/patches' % sec_id, req_params)
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
            res = self.spp_ctl_cli.delete('nfvs/%d' % sec_id)
            if res is not None:
                error_codes = self.spp_ctl_cli.rest_common_error_codes
                if res.status_code == 204:
                    print('Exit sec %d' % sec_id)
                elif res.status_code in error_codes:
                    pass
                else:
                    print('Error: unknown response.')

        else:
            print('Invalid command "%s".' % cmd)

    def print_sec_status(self, json_obj):
        """Parse and print message from SPP secondary.

        Print status received from secondary.

          spp > sec 1;status
          - status: idling
          - ports:
            - phy:0 -> ring:0
            - phy:1

        The format of the received message is JSON and ended with
        series of null character "\x00".

          {"client-id":1,...,"patches":[{"src":"phy:0"...},...]}'\x00..
        """

        sec_attr = json_obj
        print('- status: %s' % sec_attr['status'])
        print('- ports:')
        for port in sec_attr['ports']:
            dst = None
            for patch in sec_attr['patches']:
                if patch['src'] == port:
                    dst = patch['dst']

            if dst is None:
                print('  - %s' % port)
            else:
                print('  - %s -> %s' % (port, dst))

    def complete(self, sec_ids, text, line, begidx, endidx):
        """Completion for secondary process commands.

        Called from complete_sec() to complete secondary command.
        """

        try:
            cleaned_line = line

            if len(cleaned_line.split()) == 1:
                completions = [str(i)+";" for i in sec_ids]
            elif len(cleaned_line.split()) == 2:
                if not (";" in cleaned_line):
                    tmplist = [str(i) for i in sec_ids]
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
