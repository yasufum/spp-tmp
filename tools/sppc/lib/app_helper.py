# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

from . import common
import os
import secrets
import subprocess
import sys


# Supported vdev types of SPP Container.
VDEV_TYPES = ['vhost', 'memif', 'tap']

# Prefix of tap interface which is named as 'spp_tap0', 'spp_tap1' or so.
TAP_PREFIX = 'spp_tap'


def add_eal_args(parser, mem_size=1024, mem_channel=4):
    """Add EAL args to app."""

    parser.add_argument(
        '-l', '--core-list',
        type=str,
        help='Core list')
    parser.add_argument(
        '-c', '--core-mask',
        type=str,
        help='Core mask')
    parser.add_argument(
        '-m', '--mem',
        type=int,
        default=mem_size,
        help='Memory size (default is %s)' % mem_size)
    parser.add_argument(
        '--vdev',
        nargs='*', type=str,
        help='Virtual device in the format of DPDK')
    parser.add_argument(
        '--socket-mem',
        type=str,
        help='Memory size')
    parser.add_argument(
        '-b', '--pci-blacklist',
        nargs='*', type=str,
        help='PCI blacklist for excluding devices')
    parser.add_argument(
        '-w', '--pci-whitelist',
        nargs='*', type=str,
        help='PCI whitelist for including devices')
    parser.add_argument(
        '--single-file-segments',
        action='store_true',
        help='Create fewer files in hugetlbfs (non-legacy mode only).')
    parser.add_argument(
        '--nof-memchan',
        type=int,
        default=mem_channel,
        help='Number of memory channels (default is %s)' % mem_channel)
    return parser


def add_sppc_args(parser):
    """Add args of SPP Container to app."""

    parser.add_argument(
        '--dist-name',
        type=str,
        default='ubuntu',
        help="Name of Linux distribution")
    parser.add_argument(
        '--dist-ver',
        type=str,
        default='latest',
        help="Version of Linux distribution")
    parser.add_argument(
        '--workdir',
        type=str,
        help="Path of directory in which the command is launched")
    parser.add_argument(
        '--name',
        type=str,
        help='Name of container')
    parser.add_argument(
        '-ci', '--container-image',
        type=str,
        help="Name of container image")
    parser.add_argument(
        '-fg', '--foreground',
        action='store_true',
        help="Run container as foreground mode")
    parser.add_argument(
        '--dry-run',
        action='store_true',
        help="Only print matrix, do not run, and exit")
    return parser


def add_appc_args(parser):
    """Add docker options and other common args."""

    parser.add_argument(
        '-d', '--dev-uids',
        type=str,
        help='Virtual devices of SPP in resource UID format')
    parser.add_argument(
        '-v', '--volume',
        nargs='*', type=str,
        help='Bind mount a volume (for docker)')
    parser.add_argument(
        '-nq', '--nof-queues',
        type=int,
        default=1,
        help="Number of queues of virtio (default is 1)")
    parser.add_argument(
        '--no-privileged',
        action='store_true',
        help="Disable docker's privileged mode if it's needed")
    return parser


def is_valid_dev_uids(dev_uids):
    """Return True if value of --dev-uids is valid.

    dev_uids should be a list of resource UIDs separated with ',', for example
    'vhost:0,vhost:1'.

    If given port type is not supported in SPP Container, it returns False.
    """

    if dev_uids is None:
        return False

    for dev_uid in dev_uids.split(','):
        dtype = dev_uid.split(':')[0]
        if dtype not in VDEV_TYPES:
            print('Error: `{}` is not supported.'.format(dtype))
            return False

    return True


def get_core_opt(args):
    # Check core_mask or core_list is defined.
    if args.core_mask is not None:
        core_opt = {'attr': '-c', 'val': args.core_mask}
    elif args.core_list is not None:
        core_opt = {'attr': '-l', 'val': args.core_list}
    else:
        common.error_exit('core_mask or core_list')
    return core_opt


def get_mem_opt(args):
    # Check memory option is defined.
    if args.socket_mem is not None:
        mem_opt = {'attr': '--socket-mem', 'val': args.socket_mem}
    else:
        mem_opt = {'attr': '-m', 'val': str(args.mem)}
    return mem_opt


def setup_eal_opts(args, app_name=None, proc_type='auto', is_spp_pri=False,
                   hugedir=None):
    core_opt = get_core_opt(args)
    mem_opt = get_mem_opt(args)

    eal_opts = [
        core_opt['attr'], core_opt['val'], '\\',
        '-n', str(args.nof_memchan), '\\',
        mem_opt['attr'], mem_opt['val'], '\\',
        '--proc-type', proc_type, '\\']

    if args.dev_uids is not None:
        dev_uids_list = args.dev_uids.split(',')

        socks = sock_files(dev_uids_list, is_spp_pri)

        # Configure '--vdev' options
        for i in range(len(dev_uids_list)):
            dev_uid = dev_uids_list[i].split(':')
            if dev_uid[0] == 'vhost':
                if not is_spp_pri:
                    eal_opts += [
                            '--vdev',
                            'virtio_user{0:s},queues={1:d},path={2:s}'.
                            format(dev_uid[1], args.nof_queues,
                                   socks[i]['guest']),
                            '\\']
                else:
                    # TODO(yasufum) Support `queues` option.
                    eal_opts += [
                            '--vdev',
                            'eth_vhost{0:s},iface={1:s}'.
                            format(dev_uid[1], socks[i]['guest']),
                            '\\']
            elif dev_uid[0] == 'memif':
                if not is_spp_pri:
                    eal_opts += [
                            '--vdev',
                            'net_memif{0:s},id={0:s},socket={1:s}'.
                            format(dev_uid[1], socks[0]['guest']),
                            '\\']
                else:
                    eal_opts += [
                            '--vdev',
                            'net_memif{0:s},id={0:s},role={1:s},socket={2:s}'.
                            format(dev_uid[1], 'master', socks[0]['guest']),
                            '\\']
            elif dev_uid[0] == 'tap':
                eal_opts += [
                        '--vdev',
                        'net_tap{0:s},iface={1:s}{0:s}'.
                        format(dev_uid[1], TAP_PREFIX), '\\']

    if (args.pci_blacklist is not None) and (args.pci_whitelist is not None):
        common.error_exit("Cannot use both of '-b' and '-w' at once")
    elif args.pci_blacklist is not None:
        for bd in args.pci_blacklist:
            eal_opts += ['-b', bd, '\\']
    elif args.pci_whitelist is not None:
        for wd in args.pci_whitelist:
            eal_opts += ['-w', wd, '\\']

    if args.single_file_segments is True:
        eal_opts += ['--single-file-segments', '\\']

    # Generate unique --file-prefix value for app container, or use common
    # value for spp_primary and secondary.
    if args.name is not None:
        file_prefix = _gen_sppc_file_prefix(args.name)
    elif app_name is not None and app_name.__class__ is str:
        file_prefix = _gen_sppc_file_prefix(app_name)
    else:
        file_prefix = common.SPPC_FILE_PREFIX
    eal_opts += [
        '--file-prefix', file_prefix, '\\',
        '--', '\\']

    return eal_opts


def setup_docker_opts(args, socks=None, app_opts=None, workdir=None):
    """Return docker options as a list.

    socks must be None if process behaves as master role, such as
    spp_primary, or failed to initialize the process.

    :param args: Parsed args with argparse
    :param socks: Socket files, it must be None in spp-primary
    :param app_opts: Application specific option
    :returns: A list of docker options
    """

    docker_opts = []

    if args.foreground is True:
        docker_opts = ['-it', '\\']
    else:
        docker_opts = ['-d', '\\']

    if args.no_privileged is not True:
        docker_opts += ['--privileged', '\\']

    docker_opts += [
        '-v', '/dev/hugepages:/dev/hugepages', '\\']

    if app_opts is not None:
        docker_opts += app_opts

    if args.workdir is not None:
        docker_opts += ['--workdir', args.workdir, '\\']

    if args.name is not None:
        docker_opts += ['--name', args.name, '\\']

    if socks is not None:
        for sock in socks:
            docker_opts += [
                '-v', '%s:%s' % (sock['host'], sock['guest']), '\\']

    return docker_opts


def is_sufficient_ports(args):
    """Check if ports can be reserved.

    Return True if the number of vdevs is enogh for given ports.
    """

    # TODO(yasufum): It doesn't check if no given portmask and dev_uids, so
    # add this additional check.
    if (args.port_mask is None) or (args.dev_uids is None):
        return False
    elif not ('0x' in args.port_mask):  # invalid port mask
        return False

    dev_uids_list = args.dev_uids.split(',')

    ports_in_binary = format(int(args.port_mask, 16), 'b')
    if len(dev_uids_list) >= len(ports_in_binary):
        return True
    else:
        return False


def sock_files(dev_uids_list, is_spp_pri=False):
    """Return list of socket files on host and containers.

    The name of socket files is defined with a conventional ones described
    in DPDK doc, though you can use any name actually.

    For spp_primary, path of sock file is just bit different because it is
    shared among other SPP processes.

    Here is an example of two vhost devices.
        [vhost:0, vhost:1]
        => [
              {'host': '/tmp/sock0, 'guest': '/var/run/usvhost0'},
              {'host': '/tmp/sock1, 'guest': '/var/run/usvhost1'}
            ]
    """

    socks = {
            'vhost': {
                'host': '/tmp/sock{:s}',
                'guest': '/var/run/usvhost{:s}'},
            'memif': {
                'host': '/tmp/spp-memif.sock',
                'guest': '/var/run/spp-memif.sock'}}

    res = []
    is_memif_added = False
    for dev_uid in dev_uids_list:
        dev_uid = dev_uid.split(':')

        if (dev_uid[0] == 'memif') and (not is_memif_added):
            # Single sock file is enough for memif because it is just used for
            # negotiation between master and slaves processes.
            if is_spp_pri:
                res.append({
                    'host': socks['memif']['host'],
                    'guest': socks['memif']['host']})
            else:
                res.append({
                    'host': socks['memif']['host'],
                    'guest': socks['memif']['guest']})
            is_memif_added = True

        elif dev_uid[0] == 'vhost':
            if is_spp_pri:
                res.append({
                    'host': socks['vhost']['host'].format(dev_uid[1]),
                    'guest': socks['vhost']['host'].format(dev_uid[1])})
            else:
                res.append({
                    'host': socks['vhost']['host'].format(dev_uid[1]),
                    'guest': socks['vhost']['guest'].format(dev_uid[1])})

    return res


def count_ports(port_mask):
    """Return the number of ports of given portmask"""

    ports_in_binary = format(int(port_mask, 16), 'b')
    nof_ports = ports_in_binary.count('1')
    return nof_ports


def cores_to_list(core_opt):
    """Expand DPDK core option to ranged list.

    Core option must be a hash of attritute and its value.
    Attribute is -c(core mask) or -l(core list).
    For example, '-c 0x03' is described as:
      core_opt = {'attr': '-c', 'val': '0x03'}
    or '-l 0-1' is as
      core_opt = {'attr': '-l', 'val': '0-1'}

    Returned value is a list, such as:
      '0x17' is converted to [1,2,3,5].
    or
      '-l 1-3,5' is converted to [1,2,3,5],
    """

    res = []
    if core_opt['attr'] == '-c':
        bin_list = list(
            format(
                int(core_opt['val'], 16), 'b'))
        cnt = 1
        bin_list.reverse()
        for i in bin_list:
            if i == '1':
                res.append(cnt)
            cnt += 1
    elif core_opt['attr'] == '-l':
        for core_part in core_opt['val'].split(','):
            if '-' in core_part:
                cl = core_part.split('-')
                res = res + list(range(int(cl[0]), int(cl[1])+1))
            else:
                res.append(int(core_part))
    else:
        pass
    res = _uniq(res)
    res.sort()
    return res


def _gen_sppc_file_prefix(app_name):
    """Generate a unique file prefix of DPDK for SPP Container app."""

    return 'sppc-{:s}-{:s}'.format(app_name, secrets.token_hex(8))


def get_dpdk_ver_in_container(rte_sdk, c_image):
    """Get DPDK version on a container.

    The version is retrieved by reading `${RTE_SDK/VERION` file.
    """

    cmd = ['cat', '{:s}/VERSION'.format(rte_sdk)]
    cmd = ['docker', 'run', '-it', c_image] + cmd
    # Decode the result of byte type to utf-8.
    return subprocess.check_output(cmd).decode('utf-8').strip()


def compare_version(expected, target):
    """Compare given versions.

    If two versions are equal, return 0. On the other hand, return -1 if
    expected ver is less than target, or return 1.
    """

    from distutils.version import LooseVersion
    if LooseVersion(expected) == LooseVersion(target):
        return 0
    elif LooseVersion(expected) < LooseVersion(target):
        return 1
    else:
        return -1


def _uniq(dup_list):
    """Remove duplicated elements in a list and return a unique list.

    Example: [1,1,2,2,3,3] #=> [1,2,3]
    """

    return list(set(dup_list))
