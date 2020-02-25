..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation

.. _sppc_howto_define_appc:

How to Define Your App Launcher
===============================

SPP container is a set of python script for launching DPDK application
on a container with docker command. You can launch your own application
by preparing a container image and install your application in
the container.
In this chapter, you will understand how to define application container
for your application.


.. _sppc_howto_build_img:

Build Image
-----------

SPP container provides a build tool with version specific Dockerfiles.
You should read the Dockerfiles to understand environmental variable
or command path are defined.
Build tool refer ``conf/env.py`` for the definitions before running
docker build.

Dockerfiles of pktgen or SPP can help your understanding for building
app container in which your application is placed outside of DPDK's
directory.
On the other hand, if you build an app container of DPDK sample
application, you do not need to prepare your Dockerfile because all of
examples are compiled while building DPDK's image.


.. _sppc_howto_create_appc:

Create App Container Script
---------------------------

As explained in :ref:`spp_container_app_launcher`, app container script
shold be prepared for each of applications.
Application of SPP container is roughly categorized as DPDK sample apps
or not. The former case is like that you change an existing DPDK sample
application and run as a app container.

For DPDK sample apps, it is easy to build image and create app container
script.
On the other hand, it is a bit complex because you should you should
define environmental variables, command path and compilation process by
your own.

This section describes how to define app container script,
first for DPDK sample applications,
and then second for other than them.

.. _sppc_howto_dpdk_sample_appc:

DPDK Sample App Container
-------------------------

Procedure of App container script is defined in main() and
consists of three steps of
(1) parsing options, (2) setup docker command and
(3) application command run inside the container.

Here is a sample code of :ref:`sppc_appl_l2fwd`.
``parse_args()`` is defined in each
of app container scripts to parse all of EAL, docker and application
specific options.
It returns a result of ``parse_args()`` method of
``argparse.ArgumentParser`` class.
App container script uses standard library module ``argparse``
for parsing the arguments.

.. code-block:: python

    def main():
        args = parse_args()

        # Container image name such as 'sppc/dpdk-ubuntu:18.04'
        if args.container_image is not None:
            container_image = args.container_image
        else:
            container_image = common.container_img_name(
                common.IMG_BASE_NAMES['dpdk'],
                args.dist_name, args.dist_ver)

        # Check for other mandatory opitons.
        if args.port_mask is None:
            common.error_exit('--port-mask')

If the name of container is given via ``args.container_image``, it is
decided as a combination of basename, distribution and its version.
Basenames are defined as ``IMG_BASE_NAMES`` in ``lib/common.py``.
In general, You do not need to change for using DPDK sample apps.

.. code-block:: python

    # defined in lib/common.py
    IMG_BASE_NAMES = {
        'dpdk': 'sppc/dpdk',
        'pktgen': 'sppc/pktgen',
        'spp': 'sppc/spp',
        'suricata': 'sppc/suricata',
        }

Options can be referred via ``args``. For example, the name of container
image can be referred via ``args.container_image``.

Before go to step (2) and (3), you had better to check given option,
expecially mandatory options.
``common.error_exit()`` is a helper method to print an error message
for given option and do ``exit()``. In this case, ``--port-mask`` must
be given, or exit with an error message.

Setup of ``sock_files`` is required for creating network interfaces
for the container. ``sock_files()`` defined in ``lib/app_helper.py`` is
provided for creating socket files from given device UIDs.

Then, setup docker command and its options as step (2).
Docker options are added by using helper method
``setup_docker_opts()`` which generates commonly used options for app
containers.
This methods returns a list of a part of options to give it to
``subprocess.call()``.

.. code-block:: python

    # Setup docker command.
    docker_cmd = ['sudo', 'docker', 'run', '\\']
    docker_opts = app_helper.setup_docker_opts(args, sock_files)

You also notice that ``docker_cmd`` has a backslash ``\\`` at the end of
the list.
It is only used to format the printed command on the terminal.
If you do no care about formatting, you do not need to add this character.

Next step is (3), to setup the application command.
You should change ``cmd_path`` to specify your application.
In ``app/l2fwd.py``, the application compiled under ``RTE_SDK`` in DPDK's
directory, but your application might be different.

.. code-block:: python

    # Setup l2fwd command run on container.
    cmd_path = '{0:s}/examples/{2:s}/{1:s}/{2:s}'.format(
        env.RTE_SDK, env.RTE_TARGET, APP_NAME)

    l2fwd_cmd = [cmd_path, '\\']

    # Setup EAL options.
    eal_opts = app_helper.setup_eal_opts(args, APP_NAME)

    # Setup l2fwd options.
    l2fwd_opts = ['-p', args.port_mask, '\\']

While setting up EAL option in ``setup_eal_opts()``, ``--file-prefix`` is
generated by using the name of application and a random number. It should
be unique on the system because it is used as the name of hugepage file.

Finally, combine command and all of options before launching from
``subprocess.call()``.

.. code-block:: python

    cmds = docker_cmd + docker_opts + [container_image, '\\'] + \
        l2fwd_cmd + eal_opts + l2fwd_opts
    if cmds[-1] == '\\':
        cmds.pop()
    common.print_pretty_commands(cmds)

    if args.dry_run is True:
        exit()

    # Remove delimiters for print_pretty_commands().
    while '\\' in cmds:
        cmds.remove('\\')
    subprocess.call(cmds)

There are some optional behaviors in the final step.
``common.print_pretty_commands()`` replaces ``\\`` with a newline character
and prints command line in pretty format.
If you give ``--dry-run`` option, this launcher script prints command line
and exits without launching container.


.. _sppc_howto_none_dpdk_sample_apps:

None DPDK Sample Applications in Container
------------------------------------------

There are several application using DPDK but not included in
`sample applications
<https://dpdk.org/doc/guides/sample_app_ug/index.html>`_.
``pktgen.py`` is an example of this type of app container.
As described in :ref:`sppc_howto_dpdk_sample_appc`,
app container consists of three steps and it is the same for
this case.

First of all, you define parsing option for EAL, docker and
your application.

.. code-block:: python

    def parse_args():
        parser = argparse.ArgumentParser(
            description="Launcher for pktgen-dpdk application container")

        parser = app_helper.add_eal_args(parser)
        parser = app_helper.add_appc_args(parser)

        parser.add_argument(
            '-s', '--pcap-file',
            type=str,
            help="PCAP packet flow file of port, defined as 'N:filename'")
        parser.add_argument(
            '-f', '--script-file',
            type=str,
            help="Pktgen script (.pkt) to or a Lua script (.lua)")
        ...

        parser = app_helper.add_sppc_args(parser)
        return parser.parse_args()

It is almost the same as :ref:`sppc_howto_dpdk_sample_appc`,
but it has options for ``pktgen`` itself.
For your application, you can simply add options to ``parser`` object.

.. code-block:: python

    def main():
        args = parse_args()

Setup of socket files for network interfaces is the same as DPDK sample apps.
However, you might need to change paht of command  which is run in the
container. In ``app/pktgen.py``, directory of ``pktgen`` is defined as
``wd``, and the name of application s defined as ``APP_NAME``.
This directory can be changed with ``--workdir`` option.

.. code-block:: python

    # Setup docker command.
    if args.workdir is not None:
        wd = args.workdir
    else:
        wd = '/root/pktgen-dpdk'
    docker_cmd = ['sudo', 'docker', 'run', '\\']
    docker_opts = app_helper.setup_docker_opts(args, sock_files, None, wd)

    # Setup pktgen command
    pktgen_cmd = [APP_NAME, '\\']

    # Setup EAL options.
    eal_opts = app_helper.setup_eal_opts(args, APP_NAME)


Finally, combine all of commands and its options and launch
from ``subprocess.call()``.

.. code-block:: python

    cmds = docker_cmd + docker_opts + [container_image, '\\'] + \
        pktgen_cmd + eal_opts + pktgen_opts
    if cmds[-1] == '\\':
        cmds.pop()
    common.print_pretty_commands(cmds)

    if args.dry_run is True:
        exit()

    # Remove delimiters for print_pretty_commands().
    while '\\' in cmds:
        cmds.remove('\\')
    subprocess.call(cmds)

As you can see, it is almost the same as DPDK sample app container
without application path and options of application specific.
