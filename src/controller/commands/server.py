# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation

import re
from ..spp_common import logger


class SppCtlServer(object):
    """Execute server command for switching spp-ctl.

    SppCtlServer class is intended to be used in Shell class as a delegator
    for running 'server' command.

    'self.run()' is called from do_pri() and 'self.complete()' is called
    from complete_pri() of both of which is defined in Shell.
    """

    SERVER_CMDS = ['list']

    def __init__(self, spp_cli_objs):
        self.spp_cli_objs = spp_cli_objs
        self.current_idx = 0

    def run(self, commands):
        args = re.sub(r'\s+', ' ', commands).split(' ')
        if '' in args:
            args.remove('')

        if len(args) == 0 or args[0] == 'list':
            self._show_list()
        else:
            idx = int(args[0]) - 1
            self._switch_to(idx)

    def complete(self, text, line, begidx, endidx):
        """Completion for server command.

        Called from complete_server() to complete server command.
        """

        candidates = []
        for i in range(len(self.spp_cli_objs)):
            candidates.append(str(i))
        candidates = candidates + self.SERVER_CMDS[:]

        if not text:
            completions = candidates
        else:
            completions = [p for p in candidates
                           if p.startswith(text)
                           ]

        return completions

    def get_current_server(self):
        return self.spp_cli_objs[self.current_idx]

    def _show_list(self):
        cnt = 1
        for cli_obj in self.spp_cli_objs:
            # Put a mark to current server.
            if cnt == self.current_idx + 1:
                current = '*'
            else:
                current = ''

            print('  %d: %s:%s %s' % (
                  cnt, cli_obj.ip_addr, cli_obj.port, current))
            cnt += 1

    def _switch_to(self, idx):
        if len(self.spp_cli_objs) > idx:
            self.current_idx = idx
            cli_obj = self.spp_cli_objs[self.current_idx]
            print('Switch spp-ctl to "%d: %s:%d"' %
                    (idx+1, cli_obj.ip_addr, cli_obj.port))
        else:
            print('Index should be less than %d!' %
                    (len(self.spp_cli_objs) + 1))
