# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation


class SppBye(object):
    """Exec SPP bye command.

    SppBye class is intended to be used in Shell class as a delegator
    for running 'bye' command.

    'self.command()' is called from do_bye() and 'self.complete()' is called
    from complete_bye() of both of which is defined in Shell.
    """

    BYE_CMDS = ['sec', 'all']

    def __init__(self, spp_ctl_cli, spp_primary, spp_nfvs):
        self.spp_ctl_cli = spp_ctl_cli
        self.spp_primary = spp_primary
        self.spp_nfvs = spp_nfvs

    def run(self, args, sec_ids):

        cmds = args.split(' ')
        if cmds[0] == 'sec':
            self.close_all_secondary(sec_ids)
        elif cmds[0] == 'all':
            print('Closing secondary ...')
            self.close_all_secondary(sec_ids)
            print('Closing primary ...')
            self.spp_primary.do_exit()

    def complete(self, text, line, begidx, endidx):

        if not text:
            completions = self.BYE_CMDS[:]
        else:
            completions = [p
                           for p in self.BYE_CMDS
                           if p.startswith(text)
                           ]
        return completions

    def close_all_secondary(self, sec_ids):
        """Terminate all secondary processes."""

        for i, nfv in self.spp_nfvs.items():
            nfv.run(i, 'exit')
