..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017-2019 Nippon Telegraph and Telephone Corporation


.. _spp_usecases_vf:

spp_vf
======

``spp_vf`` is a secondary process for providing L2 classification as a simple
pusedo SR-IOV features.

.. _spp_usecases_vf_cls_icmp:

Classify ICMP Packets
---------------------

To confirm classifying packets, sends ICMP packet from remote node by using
ping and watch the response.
Incoming packets through ``NIC0`` are classified based on destination address.

.. _figure_spp_vf_use_cases_nw_config:

.. figure:: ../images/spp_vf/basic_usecase_vf_nwconfig.*
    :width: 90%

    Network Configuration


Setup
~~~~~

Launch ``spp-ctl`` and SPP CLI before primary and secondary processes.

.. code-block:: console

    # terminal 1
    $ python3 ./src/spp-ctl/spp-ctl -b 192.168.1.100

.. code-block:: console

    # terminal 2
    $ python ./src/spp.py -b 192.168.1.100

``spp_primary`` on the second lcore with ``-l 1`` and two ports ``-p 0x03``.

.. code-block:: console

    # terminal 3
    $ sudo ./src/primary/x86_64-native-linuxapp-gcc/spp_primary \
        -l 1 -n 4 \
        --socket-mem 512,512 \
        --huge-dir=/run/hugepages/kvm \
        --proc-type=primary \
        -- \
        -p 0x03 \
        -n 10 -s 127.0.0.1:5555

After ``spp_primary`` is launched, run secondary process ``spp_vf``.
In this case, lcore options is ``-l 2-6`` for one master thread and four
worker threads.

.. code-block:: console

     # terminal 4
     $ sudo ./src/vf/x86_64-native-linuxapp-gcc/spp_vf \
        -l 2-6 \
        -n 4 --proc-type=secondary \
        -- \
        --client-id 1 \
        -s 127.0.0.1:6666 \


Network Configuration
~~~~~~~~~~~~~~~~~~~~~

Configure network as described in :numref:`figure_spp_vf_use_cases_nw_config`
step by step.

First of all, setup worker threads from ``component`` command with lcore ID
and other options on local host ``host2``.

.. code-block:: console

    # terminal 2
    spp > vf 1; component start cls 3 classifier_mac
    spp > vf 1; component start fwd1 4 forward
    spp > vf 1; component start fwd2 5 forward
    spp > vf 1; component start mgr 6 merge

Add ports for each of components as following.
The number of rx and tx ports are different for each of component's role.

.. code-block:: console

    # terminal 2

    # classifier
    spp > vf 1; port add phy:0 rx cls
    spp > vf 1; port add ring:0 tx cls
    spp > vf 1; port add ring:1 tx cls

    # forwarders
    spp > vf 1; port add ring:0 rx fwd1
    spp > vf 1; port add ring:2 tx fwd1
    spp > vf 1; port add ring:1 rx fwd2
    spp > vf 1; port add ring:3 tx fwd2

    # merger
    spp > vf 1; port add ring:2 rx mgr
    spp > vf 1; port add ring:3 rx mgr
    spp > vf 1; port add phy:1 tx mgr

You also need to configure MAC address table for classifier. In this case,
you need to register two MAC addresses. Although any MAC can be used,
you use ``52:54:00:12:34:56`` and ``52:54:00:12:34:58``.

.. code-block:: console

    # terminal 2
    spp > vf 1; classifier_table add mac 52:54:00:12:34:56 ring:0
    spp > vf 1; classifier_table add mac 52:54:00:12:34:58 ring:1


Send Packet from Remote Host
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Ensure NICs, ``ens0`` and ``ens1`` in this case, are upped on remote host
``host1``. You can up by using ifconfig if the status is down.

.. code-block:: console

    # terminal 1 on remote host
    # Configure ip address of ens0
    $ sudo ifconfig ens0 192.168.140.1 255.255.255.0 up

Add arp entries of MAC addresses statically to be resolved.

.. code-block:: console

    # terminal 1 on remote host
    # set MAC address
    $ sudo arp -i ens0 -s 192.168.140.2 52:54:00:12:34:56
    $ sudo arp -i ens0 -s 192.168.140.3 52:54:00:12:34:58

Start tcpdump command for capturing ``ens1``.

.. code-block:: console

    # terminal 2 on remote host
    $ sudo tcpdump -i ens1

Then, start ping in other terminals.

.. code-block:: console

    # terminal 3 on remote host
    # ping via NIC0
    $ ping 192.168.140.2

.. code-block:: console

    # terminal 4 on remote host
    # ping via NIC0
    $ ping 192.168.140.3

You can see ICMP Echo requests are received from ping on terminal 2.


.. _spp_vf_use_cases_shutdown_comps:

Shutdown spp_vf Components
~~~~~~~~~~~~~~~~~~~~~~~~~~

First, delete entries of ``classifier_table`` and ports of components.

.. code-block:: console

    # terminal 2
    # Delete MAC address from Classifier
    spp > vf 1; classifier_table del mac 52:54:00:12:34:56 ring:0
    spp > vf 1; classifier_table del mac 52:54:00:12:34:58 ring:1

.. code-block:: console

    # terminal 2
    # classifier
    spp > vf 1; port del phy:0 rx cls
    spp > vf 1; port del ring:0 tx cls
    spp > vf 1; port del ring:1 tx cls

    # forwarders
    spp > vf 1; port del ring:0 rx fwd1
    spp > vf 1; port del vhost:0 tx fwd1
    spp > vf 1; port del ring:1 rx fwd2
    spp > vf 1; port del vhost:2 tx fwd2

    # mergers
    spp > vf 1; port del ring:2 rx mgr
    spp > vf 1; port del ring:3 rx mgr
    spp > vf 1; port del phy:0 tx mgr

Then, stop components.

.. code-block:: console

    # terminal 2
    spp > vf 1; component stop cls
    spp > vf 1; component stop fwd1
    spp > vf 1; component stop fwd2
    spp > vf 1; component stop mgr

You can confirm that worker threads are cleaned from ``status``.

.. code-block:: none

    spp > vf 1; status
    Basic Information:
      - client-id: 1
      - ports: [phy:0, phy:1]
    Classifier Table:
      No entries.
    Components:
      - core:3 '' (type: unuse)
      - core:4 '' (type: unuse)
      - core:5 '' (type: unuse)
      - core:6 '' (type: unuse)

Finally, terminate ``spp_vf`` by using ``exit`` or ``bye sec``.

.. code-block:: console

    spp > vf 0; exit


.. _spp_vf_use_cases_usecase1:

Simple SSH Login
----------------

This section describes a usecase for simple SSH login through SPP VF.
Incoming packets are classified based on destination addresses defined
in ``classifier``.
Reterned packets are aggregated to ``merger`` to send it an outgoing
port.

.. _figure_simple_ssh_login:

.. figure:: ../images/spp_vf/usecase1_overview.*
    :width: 55%

    Simple SSH Login


Launch SPP Processes
~~~~~~~~~~~~~~~~~~~~

Change directory to spp and confirm that it is already compiled.

.. code-block:: console

    $ cd /path/to/spp

Launch ``spp-ctl`` before launching SPP primary and secondary processes.
You also need to launch ``spp.py``  if you use ``spp_vf`` from CLI.
``-b`` option is for binding IP address to communicate other SPP processes,
but no need to give it explicitly if ``127.0.0.1`` or ``localhost`` although
doing explicitly in this example to be more understandable.

.. code-block:: console

    # Launch spp-ctl and spp.py
    $ python3 ./src/spp-ctl/spp-ctl -b 127.0.0.1
    $ python ./src/spp.py -b 127.0.0.1

Then, run ``spp_primary`` on the second core with ``-c 0x02``.

.. code-block:: console

    $ sudo ./src/primary/x86_64-native-linuxapp-gcc/spp_primary \
        -c 0x02 -n 4 \
        --socket-mem 512,512 \
        --huge-dir=/run/hugepages/kvm \
        --proc-type=primary \
        -- \
        -p 0x03 -n 8 -s 127.0.0.1:5555

After ``spp_primary`` is launched, run secondary process ``spp_vf``.
Core mask ``-c 0x3ffd`` indicates to use twelve cores except the second
core, and it equals to ``-l 0,2-12``.

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

.. figure:: ../images/spp_vf/usecase1_nwconfig.*
    :width: 100%

    Network Configuration

You need to input a little bit large amount of commands for the
configuration, or use ``playback`` command to load from config files.
You can find a series of config files for this use case in
``recipes/spp_vf/usecase1/``.

First, lanch threads of SPP VF called ``component`` with its core ID
and a directive for behaviour.
It is launched from ``component`` subcommand with options.

.. code-block:: console

    spp > sec SEC_ID; component start NAME CORE_ID BEHAVIOUR

In this usecase, spp_vf is launched with ID=1. Let's start components
for the first login path.
Directive for classifier ``classifier_mac`` means to classify with MAC
address.
Core ID from 2 to 7 are assigned to each of components.

.. code-block:: console

    # Start component to spp_vf
    spp > vf 1; component start classifier1 2 classifier_mac
    spp > vf 1; component start forwarder1 3 forward
    spp > vf 1; component start forwarder2 4 forward
    spp > vf 1; component start forwarder3 5 forward
    spp > vf 1; component start forwarder4 6 forward
    spp > vf 1; component start merger1 7 merge

Each of components must have rx and tx ports for forwarding.
Add ports for each of components as following.
You might notice that classifier has two tx ports and
merger has two rx ports.

.. code-block:: console

    # classifier1
    spp > vf 1; port add phy:0 rx classifier1
    spp > vf 1; port add ring:0 tx classifier1
    spp > vf 1; port add ring:1 tx classifier1

    # forwarder1
    spp > vf 1; port add ring:0 rx forwarder1
    spp > vf 1; port add vhost:0 tx forwarder1

    # forwarder2
    spp > vf 1; port add ring:1 rx forwarder2
    spp > vf 1; port add vhost:2 tx forwarder2

    # forwarder3
    spp > vf 1; port add vhost:0 rx forwarder3
    spp > vf 1; port add ring:2 tx forwarder3

    # forwarder4
    spp > vf 1; port add vhost:2 rx forwarder4
    spp > vf 1; port add ring:3 tx forwarder4

    # merger1
    spp > vf 1; port add ring:2 rx merger1
    spp > vf 1; port add ring:3 rx merger1
    spp > vf 1; port add phy:0 tx merger1

As given ``classifier_mac``, classifier component decides
the destination with MAC address by referring ``classifier_table``.
MAC address and corresponging port is registered to the table with
``classifier_table add mac`` command.

.. code-block:: console

    spp > vf SEC_ID; classifier_table add mac MAC_ADDR PORT

In this usecase, you need to register two MAC addresses of targetting VM
for merger1.

.. code-block:: console

    # Register MAC address to classifier
    spp > vf 1; classifier_table add mac 52:54:00:12:34:56 ring:0
    spp > vf 1; classifier_table add mac 52:54:00:12:34:58 ring:1


Configuration for the second login path is almost similar to the first
path.

Start components with core ID 8-13 and directives.

.. code-block:: console

    spp > vf 1; component start classifier2 8 classifier_mac
    spp > vf 1; component start forwarder5 9 forward
    spp > vf 1; component start forwarder6 10 forward
    spp > vf 1; component start forwarder7 11 forward
    spp > vf 1; component start forwarder8 12 forward
    spp > vf 1; component start merger2 13 merge

Add ports to each of components.

.. code-block:: console

    # classifier2
    spp > vf 1; port add phy:1 rx classifier2
    spp > vf 1; port add ring:4 tx classifier2
    spp > vf 1; port add ring:5 tx classifier2

    # forwarder5
    spp > vf 1; port add ring:4 rx forwarder5
    spp > vf 1; port add vhost:1 tx forwarder5

    # forwarder6
    spp > vf 1; port add ring:5 rx forwarder6
    spp > vf 1; port add vhost:3 tx forwarder6

    # forwarder7
    spp > vf 1; port add vhost:1 rx forwarder7
    spp > vf 1; port add ring:6 tx forwarder7

    # forwarder8
    spp > vf 1; port add vhost:3 rx forwarder8
    spp > vf 1; port add ring:7 tx forwarder8

    # merger2
    spp > vf 1; port add ring:6 rx merger2
    spp > vf 1; port add ring:7 rx merger2
    spp > vf 1; port add phy:1 tx merger2

Register entries to classifier_table for classifier2 with MAC address
of targetting VM..

.. code-block:: console

    # Register MAC address to classifier
    spp > vf 1; classifier_table add mac 52:54:00:12:34:57 ring:4
    spp > vf 1; classifier_table add mac 52:54:00:12:34:59 ring:5


.. _spp_vf_use_cases_usecase1_setup_vm:

Setup for VMs
~~~~~~~~~~~~~

Launch VM1 and VM2 with virsh command.
Setup for virsh is described in :ref:`spp_gsg_howto_virsh`.

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


.. _spp_vf_use_cases_usecase1_shutdown_spp_vf_components:

Shutdown spp_vf Components
~~~~~~~~~~~~~~~~~~~~~~~~~~

Basically, you can shutdown all of SPP processes with ``bye all``
command.
This section describes graceful shutting down for SPP VF components.

First, delete entries of ``classifier_table`` and ports of components
for the first SSH login path.

.. code-block:: console

    # Delete MAC address from Classifier
    spp > vf 1; classifier_table del mac 52:54:00:12:34:56 ring:0
    spp > vf 1; classifier_table del mac 52:54:00:12:34:58 ring:1

.. code-block:: console

    # classifier1
    spp > vf 1; port del phy:0 rx classifier1
    spp > vf 1; port del ring:0 tx classifier1
    spp > vf 1; port del ring:1 tx classifier1
    # forwarder1
    spp > vf 1; port del ring:0 rx forwarder1
    spp > vf 1; port del vhost:0 tx forwarder1
    # forwarder2
    spp > vf 1; port del ring:1 rx forwarder2
    spp > vf 1; port del vhost:2 tx forwarder2

    # forwarder3
    spp > vf 1; port del vhost:0 rx forwarder3
    spp > vf 1; port del ring:2 tx forwarder3

    # forwarder4
    spp > vf 1; port del vhost:2 rx forwarder4
    spp > vf 1; port del ring:3 tx forwarder4

    # merger1
    spp > vf 1; port del ring:2 rx merger1
    spp > vf 1; port del ring:3 rx merger1
    spp > vf 1; port del phy:0 tx merger1

Then, stop components.

.. code-block:: console

    # Stop component to spp_vf
    spp > vf 1; component stop classifier1
    spp > vf 1; component stop forwarder1
    spp > vf 1; component stop forwarder2
    spp > vf 1; component stop forwarder3
    spp > vf 1; component stop forwarder4
    spp > vf 1; component stop merger1

Second, do termination for the second path.
Delete entries from ``classifier_table`` and ports from each of
components.

.. code-block:: console

    # Delete MAC address from Classifier
    spp > vf 1; classifier_table del mac 52:54:00:12:34:57 ring:4
    spp > vf 1; classifier_table del mac 52:54:00:12:34:59 ring:5

.. code-block:: console

    # classifier2
    spp > vf 1; port del phy:1 rx classifier2
    spp > vf 1; port del ring:4 tx classifier2
    spp > vf 1; port del ring:5 tx classifier2

    # forwarder5
    spp > vf 1; port del ring:4 rx forwarder5
    spp > vf 1; port del vhost:1 tx forwarder5

    # forwarder6
    spp > vf 1; port del ring:5 rx forwarder6
    spp > vf 1; port del vhost:3 tx forwarder6

    # forwarder7
    spp > vf 1; port del vhost:1 rx forwarder7
    spp > vf 1; port del ring:6 tx forwarder7

    # forwarder8
    spp > vf 1; port del vhost:3 tx forwarder8
    spp > vf 1; port del ring:7 rx forwarder8

    # merger2
    spp > vf 1; port del ring:6 rx merger2
    spp > vf 1; port del ring:7 rx merger2
    spp > vf 1; port del phy:1 tx merger2

Then, stop components.

.. code-block:: console

    # Stop component to spp_vf
    spp > vf 1; component stop classifier2
    spp > vf 1; component stop forwarder5
    spp > vf 1; component stop forwarder6
    spp > vf 1; component stop forwarder7
    spp > vf 1; component stop forwarder8
    spp > vf 1; component stop merger2

Exit spp_vf
~~~~~~~~~~~

Terminate spp_vf.

.. code-block:: console

    spp > vf 1; exit
