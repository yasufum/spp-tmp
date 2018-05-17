..  BSD LICENSE
    Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

.. _spp_vf_use_cases_usecase1:

Simple SSH Login
================

This section describes a usecase for simple SSH login through SPP VF.
Incoming packets are classified based on destination addresses defined
in ``classifier``.
Reterned packets are aggregated to ``merger`` to send it an outgoing
port.

.. _figure_simple_ssh_login:

.. figure:: ../images/usecase1_overview.*
    :height: 400 em
    :width: 400 em

    Simple SSH Login


Launch SPP VF
~~~~~~~~~~~~~

Change directory to spp and confirm that it is already compiled.

.. code-block:: console

    $ cd /path/to/spp

As spp, launch controller first. You notice that SPP VF has its own
controller ``spp_vf.py`` and do not use ``spp.py``.

.. code-block:: console

    # Launch spp_vf.py
    $ python ./src/spp_vf.py -p 5555 -s 6666

Then, run ``spp_primary``.

.. code-block:: console

    $ sudo ./src/primary/x86_64-native-linuxapp-gcc/spp_primary \
        -c 0x02 -n 4 \
        --socket-mem 512,512 \
        --huge-dir=/run/hugepages/kvm \
        --proc-type=primary \
        -- \
        -p 0x03 -n 8 -s 127.0.0.1:5555

After ``spp_primary`` is launched, run secondary process ``spp_vf``.

.. code-block:: console

    $ sudo ./src/vf/x86_64-native-linuxapp-gcc/spp_vf \
        -c 0x3ffd -n 4 --proc-type=secondary \
        -- \
        --client-id 1 \
        -s 127.0.0.1:6666 \
        --vhost-client


Network Configuration
~~~~~~~~~~~~~~~~~~~~~

Detailed configuration of :numref:`figure_simple_ssh_login` is
described below.
In this usecase, there are two NICs on host1 and host2 to duplicate
login path. Each of combination of classifier and merger responds
to each of pathes.

Incoming packets from NIC0 are classified based on destionation address.
For example, classifier1 sends packets to forwarder1 for vNIC0 and
to forwarder2 for vNIC2.
Outgoing packets from SSH server1 and 2 are aggregated to merger1 and
sent to SSH clinet via NIC0.

.. _figure_network_config:

.. figure:: ../images/usecase1_nwconfig.*
    :height: 720 em
    :width: 720 em

    Network Configuration

You need to input a little bit large amount of commands for the
configuration, or use ``playback`` command to load from config files.
You can find a series of config files for this use case in
``docs/samples/command/spp_vf/usecase1/``.

First, lanch threads of SPP VF called ``component`` with its core ID
and a directive for behaviour.
It is launched from ``component`` subcommand with options.

.. code-block:: console

    spp > sec [SEC_ID];component start [NAME] [CORE_ID] [BEHAVIOUR]

In this usecase, spp_vf is launched with ID=1. Let's start components
for the first login path.
Directive for classifier ``classifier_mac`` means to classify with MAC
address.
Core ID from 2 to 7 are assigned to each of components.

.. code-block:: console

    # Start component to spp_vf
    spp > sec 1;component start classifier1 2 classifier_mac
    spp > sec 1;component start forwarder1 3 forward
    spp > sec 1;component start forwarder2 4 forward
    spp > sec 1;component start forwarder3 5 forward
    spp > sec 1;component start forwarder4 6 forward
    spp > sec 1;component start merger1 7 merge

Each of components must have rx and tx ports for forwarding.
Add ports for each of components as following.
You might notice that classifier has two tx ports and
merger has two rx ports.

.. code-block:: console

    # classifier1
    spp > sec 1;port add phy:0 rx classifier1
    spp > sec 1;port add ring:0 tx classifier1
    spp > sec 1;port add ring:1 tx classifier1
    # forwarder1
    spp > sec 1;port add ring:0 rx forwarder1
    spp > sec 1;port add vhost:0 tx forwarder1
    # forwarder2
    spp > sec 1;port add ring:1 rx forwarder2
    spp > sec 1;port add vhost:2 tx forwarder2
    # forwarder3
    spp > sec 1;port add vhost:0 rx forwarder3
    spp > sec 1;port add ring:2 tx forwarder3
    # forwarder4
    spp > sec 1;port add vhost:2 rx forwarder4
    spp > sec 1;port add ring:3 tx forwarder4
    # merger1
    spp > sec 1;port add ring:2 rx merger1
    spp > sec 1;port add ring:3 rx merger1
    spp > sec 1;port add phy:0 tx merger1

As given ``classifier_mac``, classifier component decides
the destination with MAC address by referring ``classifier_table``.
MAC address and corresponging port is registered to the table with
``classifier_table add mac`` command.

.. code-block:: console

    spp > [SEC_ID];classifier_table add mac [MACADDRESS] [PORT]

In this usecase, you need to register two MAC addresses for merger1.

.. code-block:: console

    # Register MAC address to classifier
    spp > sec 1;classifier_table add mac 52:54:00:12:34:56 ring:0
    spp > sec 1;classifier_table add mac 52:54:00:12:34:58 ring:1

.. note::

    Please verify that MAC address of target VM is specified in
    [MACADDRESS] parameter.

Configuration for the second login path is almost similar to the first
path.

Start components with core ID 8-13 and directives.

.. code-block:: console

    spp > sec 1;component start classifier2 8 classifier_mac
    spp > sec 1;component start forwarder5 9 forward
    spp > sec 1;component start forwarder6 10 forward
    spp > sec 1;component start forwarder7 11 forward
    spp > sec 1;component start forwarder8 12 forward
    spp > sec 1;component start merger2 13 merge

Add ports to each of components.

.. code-block:: console

    # classifier2
    spp > sec 1;port add phy:1 rx classifier2
    spp > sec 1;port add ring:4 tx classifier2
    spp > sec 1;port add ring:5 tx classifier2
    # forwarder5
    spp > sec 1;port add ring:4 rx forwarder5
    spp > sec 1;port add vhost:1 tx forwarder5
    # forwarder6
    spp > sec 1;port add ring:5 rx forwarder6
    spp > sec 1;port add vhost:3 tx forwarder6
    # forwarder7
    spp > sec 1;port add vhost:1 rx forwarder7
    spp > sec 1;port add ring:6 tx forwarder7
    # forwarder8
    spp > sec 1;port add vhost:3 rx forwarder8
    spp > sec 1;port add ring:7 tx forwarder8
    # merger2
    spp > sec 1;port add ring:6 rx merger2
    spp > sec 1;port add ring:7 rx merger2
    spp > sec 1;port add phy:1 tx merger2

Register entries to classifier_table for classifier2.

.. code-block:: console

    # Register MAC address to classifier
    spp > sec 1;classifier_table add mac 52:54:00:12:34:57 ring:4
    spp > sec 1;classifier_table add mac 52:54:00:12:34:59 ring:5

.. note::

    Please verify that MAC address of target VM is specified in
    [MACADDRESS] parameter.

Finally, activate all of settings by doign `flush` subcommand.

.. code-block:: console

    spp > sec 1;flush

.. note::

    Commands for SPP VF Controller are accepted but not activated until
    user inputs ``flush`` subcommand.
    You can cancel all of commands before doing ``flush``.


Setup for VMs
~~~~~~~~~~~~~

Launch VM1 and VM2 with virsh command.
Setup for virsh is described in :ref:`spp_vf_gsg_build`.

.. code-block:: console

    $ virsh start spp-vm1  # VM1
    $ virsh start spp-vm2  # VM2

After launched, login to ``spp-vm1`` for configuration inside the VM.

.. note::

    To avoid asked for unknown keys while login VMs,
    use ``-oStrictHostKeyChecking=no`` option for ssh.

    .. code-block:: console

        $ ssh -oStrictHostKeyChecking=no sppuser at 192.168.122.31

Up interfaces for vhost inside ``spp-vm1``.
In addition, you have to disable TCP offload function, or ssh is failed
after configuration is done.

.. code-block:: console

    # up interfaces
    $ sudo ifconfig ens4 inet 192.168.140.21 netmask 255.255.255.0 up
    $ sudo ifconfig ens5 inet 192.168.150.22 netmask 255.255.255.0 up

    # disable TCP offload
    $ sudo ethtool -K ens4 tx off
    $ sudo ethtool -K ens5 tx off

Configurations also for ``spp-vm2`` as ``spp-vm1``.

.. code-block:: console

    # up interfaces
    $ sudo ifconfig ens4 inet 192.168.140.31 netmask 255.255.255.0 up
    $ sudo ifconfig ens5 inet 192.168.150.32 netmask 255.255.255.0 up

    # disable TCP offload
    $ sudo ethtool -K ens4 tx off
    $ sudo ethtool -K ens5 tx off


Login to VMs
~~~~~~~~~~~~

Now, you can login to VMs from the remote host1.

.. code-block:: console

    # spp-vm1 via NIC0
    $ ssh sppuser@192.168.140.21

    # spp-vm1 via NIC1
    $ ssh sppuser@192.168.150.22

    # spp-vm2 via NIC0
    $ ssh sppuser@192.168.140.31

    # spp-vm2 via NIC1
    $ ssh sppuser@192.168.150.32

Shutdown SPP VF Components
~~~~~~~~~~~~~~~~~~~~~~~~~~

Basically, you can shutdown all of SPP processes with ``bye all``
command.
This section describes graceful shutting down for SPP VF components.

First, delete entries of ``classifier_table`` and ports of components
for the first SSH login path.

.. code-block:: console

    # Delete MAC address from Classifier
    spp > sec 1;classifier_table del mac 51:54:00:12:34:56 ring:0
    spp > sec 1;classifier_table del mac 51:54:00:12:34:58 ring:1

.. code-block:: console

    # classifier1
    spp > sec 1;port del phy:0 rx classifier1
    spp > sec 1;port del ring:0 tx classifier1
    spp > sec 1;port del ring:1 tx classifier1
    # forwarder1
    spp > sec 1;port del ring:0 rx forwarder1
    spp > sec 1;port del vhost:0 tx forwarder1
    # forwarder2
    spp > sec 1;port del ring:1 rx forwarder2
    spp > sec 1;port del vhost:2 tx forwarder2
    # forwarder3
    spp > sec 1;port del vhost:0 rx forwarder3
    spp > sec 1;port del ring:2 tx forwarder3
    # forwarder4
    spp > sec 1;port del vhost:2 rx forwarder4
    spp > sec 1;port del ring:3 tx forwarder4
    # merger1
    spp > sec 1;port del ring:2 rx merger1
    spp > sec 1;port del ring:3 rx merger1
    spp > sec 1;port del phy:0 tx merger1

Then, stop components.

.. code-block:: console

    # Stop component to spp_vf
    spp > sec 1;component stop classifier1
    spp > sec 1;component stop forwarder1
    spp > sec 1;component stop forwarder2
    spp > sec 1;component stop forwarder3
    spp > sec 1;component stop forwarder4
    spp > sec 1;component stop merger1

Second, do termination for the second path.
Delete entries from ``classifier_table`` and ports from each of
components.

.. code-block:: console

    # Delete MAC address from Classifier
    spp > sec 1;classifier_table del mac 51:54:00:12:34:57 ring:4
    spp > sec 1;classifier_table del mac 51:54:00:12:34:59 ring:5

.. code-block:: console

    # classifier2
    spp > sec 1;port del phy:1 rx classifier2
    spp > sec 1;port del ring:4 tx classifier2
    spp > sec 1;port del ring:5 tx classifier2
    # forwarder5
    spp > sec 1;port del ring:4 rx forwarder5
    spp > sec 1;port del vhost:1 tx forwarder5
    # forwarder6
    spp > sec 1;port del ring:5 rx forwarder6
    spp > sec 1;port del vhost:3 tx forwarder6
    # forwarder7
    spp > sec 1;port del vhost:1 rx forwarder7
    spp > sec 1;port del ring:6 tx forwarder7
    # forwarder8
    spp > sec 1;port del vhost:3 tx forwarder8
    spp > sec 1;port del ring:7 rx forwarder8
    # merger2
    spp > sec 1;port del ring:6 rx merger2
    spp > sec 1;port del ring:7 rx merger2
    spp > sec 1;port del phy:1 tx merger2

Then, stop components.

.. code-block:: console

    # Stop component to spp_vf
    spp > sec 1;component stop classifier2
    spp > sec 1;component stop forwarder5
    spp > sec 1;component stop forwarder6
    spp > sec 1;component stop forwarder7
    spp > sec 1;component stop forwarder8
    spp > sec 1;component stop merger2

Finally, run ``flush`` subcommand.

.. code-block:: console

    spp > sec 1;flush
