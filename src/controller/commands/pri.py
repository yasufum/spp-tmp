# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation


class SppPrimary(object):
    """Exec SPP primary command.

    SppPrimary class is intended to be used in Shell class as a delegator
    for running 'pri' command.

    'self.command()' is called from do_pri() and 'self.complete()' is called
    from complete_pri() of both of which is defined in Shell.
    """

    # All of primary commands used for validation and completion.
    PRI_CMDS = ['status', 'exit', 'clear']

    def __init__(self, spp_ctl_cli):
        self.spp_ctl_cli = spp_ctl_cli

    def run(self, cmd):
        """Called from do_pri() to Send command to primary process."""

        if not (cmd in self.PRI_CMDS):
            print("Invalid pri command: '%s'" % cmd)
            return None

        if cmd == 'status':
            res = self.spp_ctl_cli.get('primary/status')
            if res is not None:
                if res.status_code == 200:
                    self.print_status(res.json())
                elif res.status_code in self.rest_common_error_codes:
                    # Print default error message
                    pass
                else:
                    print('Error: unknown response.')

        elif cmd == 'clear':
            res = self.spp_ctl_cli.delete('primary/status')
            if res is not None:
                if res.status_code == 204:
                    print('Clear port statistics.')
                elif res.status_code in self.rest_common_error_codes:
                    pass
                else:
                    print('Error: unknown response.')

        elif cmd == 'exit':
            print('"pri; exit" is deprecated.')

        else:
            print('Invalid pri command!')

    def print_status(self, json_obj):
        """Parse SPP primary's status and print.

        Primary returns the status as JSON format, but it is just a little
        long.

            {
                "phy_ports": [
                    {
                        "eth": "56:48:4f:12:34:00",
                        "id": 0,
                        "rx": 78932932,
                        "tx": 78932931,
                        "tx_drop": 1,
                    }
                    ...
                ],
                "ring_ports": [
                    {
                        "id": 0,
                        "rx": 89283,
                        "rx_drop": 0,
                        "tx": 89283,
                        "tx_drop": 0
                    },
                    ...
                ]
            }

        It is formatted to be simple and more understandable.

            Physical Ports:
              ID          rx          tx     tx_drop  mac_addr
               0    78932932    78932931           1  56:48:4f:53:54:00
            Ring Ports:
              ID          rx          tx     rx_drop     rx_drop
               0       89283       89283           0           0
               ...
        """

        if 'phy_ports' in json_obj:
            print('Physical Ports:')
            print('  ID          rx          tx     tx_drop  mac_addr')
            for pports in json_obj['phy_ports']:
                print('  %2d  %10d  %10d  %10d  %s' % (
                    pports['id'], pports['rx'],  pports['tx'],
                    pports['tx_drop'], pports['eth']))

        if 'ring_ports' in json_obj:
            print('Ring Ports:')
            print('  ID          rx          tx     rx_drop     rx_drop')
            for rports in json_obj['ring_ports']:
                print('  %2d  %10d  %10d  %10d  %10d' % (
                    rports['id'], rports['rx'],  rports['tx'],
                    rports['rx_drop'], rports['tx_drop']))

    def complete(self, text, line, begidx, endidx):
        """Completion for primary process commands.

        Called from complete_pri() to complete primary command.
        """

        if not text:
            completions = self.PRI_CMDS[:]
        else:
            completions = [p for p in self.PRI_CMDS
                           if p.startswith(text)
                           ]
        return completions
