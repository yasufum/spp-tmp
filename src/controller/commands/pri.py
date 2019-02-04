# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

from __future__ import absolute_import

from .. import spp_common
from ..shell_lib import common
from ..spp_common import logger
#from .. import spp_common


class SppPrimary(object):
    """Exec SPP primary command.

    SppPrimary class is intended to be used in Shell class as a delegator
    for running 'pri' command.

    'self.run()' is called from do_pri() and 'self.complete()' is called
    from complete_pri() of both of which is defined in Shell.
    """

    # All of primary commands used for validation and completion.
    PRI_CMDS = ['status', 'launch', 'clear']

    def __init__(self, spp_ctl_cli):
        self.spp_ctl_cli = spp_ctl_cli

    def run(self, cmd):
        """Called from do_pri() to Send command to primary process."""

        tmpary = cmd.split(' ')
        subcmd = tmpary[0]
        params = tmpary[1:]

        if not (subcmd in self.PRI_CMDS):
            print("Invalid pri command: '%s'" % subcmd)
            return None

        # use short name
        common_err_codes = self.spp_ctl_cli.rest_common_error_codes

        if subcmd == 'status':
            res = self.spp_ctl_cli.get('primary/status')
            if res is not None:
                if res.status_code == 200:
                    self.print_status(res.json())
                elif res.status_code in common_err_codes:
                    # Print default error message
                    pass
                else:
                    print('Error: unknown response.')

        elif subcmd == 'launch':
            self._run_launch(params)

        elif subcmd == 'clear':
            res = self.spp_ctl_cli.delete('primary/status')
            if res is not None:
                if res.status_code == 204:
                    print('Clear port statistics.')
                elif res.status_code in common_err_codes:
                    pass
                else:
                    print('Error: unknown response.')

        else:
            print('Invalid pri command!')

    def do_exit(self):
        res = self.spp_ctl_cli.delete('primary')

        if res is not None:
            error_codes = self.spp_ctl_cli.rest_common_error_codes
            if res.status_code == 204:
                print('Exit primary')
            elif res.status_code in error_codes:
                pass
            else:
                print('Error: unknown response.')

    def print_status(self, json_obj):
        """Parse SPP primary's status and print.

        Primary returns the status as JSON format, but it is just a little
        long.

            {
                "lcores": [0, 3],
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

            - lcores:
                [0, 3]
            - physical ports:
                ID          rx          tx     tx_drop  mac_addr
                 0    78932932    78932931           1  56:48:4f:53:54:00
            - ring ports:
                ID          rx          tx     rx_drop     rx_drop
                 0       89283       89283           0           0
                 ...
        """

        if 'lcores' in json_obj:
            print('- lcores:')
            print('  - {}'.format(json_obj['lcores']))

        if 'phy_ports' in json_obj:
            print('- physical ports:')
            print('    ID          rx          tx     tx_drop  mac_addr')
            for pports in json_obj['phy_ports']:
                print('    %2d  %10d  %10d  %10d  %s' % (
                    pports['id'], pports['rx'],  pports['tx'],
                    pports['tx_drop'], pports['eth']))

        if 'ring_ports' in json_obj:
            print('- ring Ports:')
            print('    ID          rx          tx     rx_drop     rx_drop')
            for rports in json_obj['ring_ports']:
                print('    %2d  %10d  %10d  %10d  %10d' % (
                    rports['id'], rports['rx'],  rports['tx'],
                    rports['rx_drop'], rports['tx_drop']))

    def complete(self, text, line, begidx, endidx):
        """Completion for primary process commands.

        Called from complete_pri() to complete primary command.
        """

        candidates = []
        tokens = line.split(' ')

        base_core = 1  # shared among secondaries
        mytemplate = "-l {},{} -m 512 -- {} {} -s {}"

        if tokens[0].endswith(';'):

            # Show sub commands
            if len(tokens) == 2:
                # Add sub commands
                candidates = candidates + self.PRI_CMDS[:]

            # Show args of `launch` sub command.
            elif len(tokens) == 3 and tokens[1] == 'launch':
                for pt in spp_common.SEC_TYPES:
                    candidates.append('{}'.format(pt))

            elif len(tokens) == 4 and tokens[1] == 'launch':
                if tokens[2] in spp_common.SEC_TYPES:
                    candidates = [
                            str(i+1) for i in range(spp_common.MAX_SECONDARY)]

            elif len(tokens) == 5 and tokens[1] == 'launch':
                if (tokens[2] in spp_common.SEC_TYPES) and \
                        (int(tokens[3])-1 in range(spp_common.MAX_SECONDARY)):
                    ptype = tokens[2]
                    sid = tokens[3]

                    if ptype == 'nfv':
                        opt_sid = '-n'
                    else:
                        opt_sid = '--client-id'

                    server_addr = common.current_server_addr()
                    server_addr = server_addr.replace('7777', '6666')

                    # Define rest of cores dynamically.
                    # TODO(yasufum) decide rest of cores considering used cores
                    if ptype == 'nfv':  # one core is enough
                        rest_core = sid
                    elif ptype == 'vf':  # at least three cores
                        rest_core = '{}-{}'.format(int(sid), int(sid)+2)
                    elif ptype == 'mirror':  # two cores
                        rest_core = sid
                    elif ptype == 'pcap':  # at least two cores
                        rest_core = '{}-{}'.format(int(sid), int(sid)+1)

                    candidates = [mytemplate.format(
                        base_core, rest_core, opt_sid, sid, server_addr)]

        if not text:
            completions = candidates
        else:
            completions = [p for p in candidates
                           if p.startswith(text)
                           ]

        return completions

    def _get_sec_ids(self):
        sec_ids = []
        res = self.spp_ctl_cli.get('processes')
        if res is not None:
            if res.status_code == 200:
                for proc in res.json():
                    if proc['type'] != 'primary':
                        sec_ids.append(proc['client-id'])
            elif res.status_code in self.spp_ctl_cli.rest_common_error_codes:
                # Print default error message
                pass
            else:
                print('Error: unknown response.')
        return sec_ids

    def _setup_opts_dict(self, opts_list):
        """Setup options for sending to spp-ctl as a request body.

        Options is setup from given list. If option has no value, None is
        assgined for the value. For example,
          ['-l', '1-2', --no-pci, '-m', '512', ...]
          => {'-l':'1-2', '--no-pci':None, '-m':'512', ...}
        """
        prekey = None
        opts_dict = {}
        for opt in opts_list:
            if opt.startswith('-'):
                opts_dict[opt] = None
                prekey = opt
            else:
                if prekey is not None:
                    opts_dict[prekey] = opt
                    prekey = None
        return opts_dict

    def _run_launch(self, params):
        """Launch secondary process.

        Parse `launch` command and send request to spp-ctl. Params of the
        consists of proc type, sec ID and arguments. It allows to skip some
        params which are completed. All of examples here are same.

        spp > lanuch nfv -l 1-2 ... -- -n 1 ...  # sec ID '1' is skipped
        spp > lanuch spp_nfv -l 1-2 ... -- -n 1 ...  # use 'spp_nfv' insteads
        """

        # Check params
        if len(params) < 2:
            print('Invalid syntax! Proc type, ID and options are required.')
            print('E.g. "nfv 1 -l 1-2 -m 512 -- -n 1 -s 192.168.1.100:6666"')
            return None

        proc_type = params[0]
        if params[1].startswith('-'):
            sec_id = None  # should be found later, or failed
            args = params[1:]
        else:
            sec_id = params[1]
            args = params[2:]

        if proc_type.startswith('spp_') is not True:
            proc_name = 'spp_' + proc_type
        else:
            proc_name = proc_type
            proc_type = proc_name[len('spp_'):]

        if proc_type not in spp_common.SEC_TYPES:
            print("'{}' is not supported in launch cmd.".format(proc_type))
            return None

        if '--' not in args:
            print('Arguments should include separator "--".')
            return None

        # Setup options of JSON sent to spp-ctl. Here is an example for
        # launching spp_nfv.
        #   {
        #      'client_id': '1',
        #      'proc_name': 'spp_nfv',
        #      'eal': {'-l': '1-2', '-m': '1024', ...},
        #      'app': {'-n': '1', '-s': '192.168.1.100:6666'}
        #   }
        idx_separator = args.index('--')
        eal_opts = args[:idx_separator]
        app_opts = args[(idx_separator+1):]

        if '--proc-type' not in args:
            eal_opts.append('--proc-type')
            eal_opts.append('secondary')

        opts = {'proc_name': proc_name}
        opts['eal'] = self._setup_opts_dict(eal_opts)
        opts['app'] = self._setup_opts_dict(app_opts)

        # Try to find sec_id from app options.
        if sec_id is None:
            if (proc_type == 'nfv') and ('-n' in opts['app']):
                sec_id = opts['app']['-n']
            elif ('--client-id' in opts['app']):  # vf, mirror or pcap
                sec_id = opts['app']['--client-id']
            else:
                print('Secondary ID is required!')
                return None

        if sec_id in self._get_sec_ids():
            print("Cannot add '{}' already used.".format(sec_id))
            return None

        opts['client_id'] = sec_id

        # Complete or correct sec_id.
        if proc_name == 'spp_nfv':
            if '-n' in opts['app'].keys():
                if (opts['app']['-n'] != sec_id):
                    opts['app']['-n'] = sec_id
            else:
                opts['app']['-n'] = sec_id
        else:  # vf, mirror or pcap
            if '--client-id' in opts['app'].keys():
                if (opts['app']['--client-id'] != sec_id):
                    opts['app']['--client-id'] = sec_id
            else:
                opts['app']['--client-id'] = sec_id

        # Send request for launch secondary.
        res = self.spp_ctl_cli.put('primary/launch', opts)
        if res is not None:
            error_codes = self.spp_ctl_cli.rest_common_error_codes
            if res.status_code == 204:
                print('Send request to launch {}:{}.'.format(
                    proc_type, sec_id))
            elif res.status_code in error_codes:
                pass
            else:
                print('Error: unknown response.')
