..
   SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2018 Nippon Telegraph and Telephone Corporation


.. _spp_mirror_use_cases_usecase:

Mirroring packet from a VM
==========================

This section describes a usage for mirroring from a VM to other VM through
spp_mirror.  Traffic from host2 is forwarded to each VM inside host1 thorough
spp_vf. spp_vf is required to forward traffic from host NIC to each VM.

In this usecase, spp-ctl should be started first. And then primary process
should be started with -n 16 like following because for giving enough number
of rings.

Move to spp directory.

.. code-block:: console

   $cd /path/to/spp

Start spp-ctl using python3.

.. code-block:: console

   $ python3 ./src/spp-ctl/spp-ctl

Start spp_primary with core id 1.

.. code-block:: console

   # Type the following in different terminal
   $ sudo ./src/primary/x86_64-native-linuxapp-gcc/spp_primary \
       -l 1 -n 4 \
       --socket-mem 512,512 \
       --huge-dir=/run/hugepages/kvm \
       --proc-type=primary \
       -- \
       -p 0x03 -n 16 -s 127.0.0.1:5555

.. _figure_simple_mirroring:

.. figure:: ../../images/spp_vf/spp_mirror_usecase_overview.*
   :width: 60%

   Mirroring from a VM

VM for spp_vf
-------------

The first step is creating VM1 for running ``spp_vf``.
A process of ``spp_vf`` is started with core list ``0,2-14`` in this usecase.

Start spp_vf with core list 0,2-14.

.. code-block:: console

   $ sudo ./src/vf/x86_64-native-linuxapp-gcc/spp_vf \
       -l 0,2-14 -n 4 --proc-type=secondary \
       -- \
       --client-id 1 \
       -s 127.0.0.1:6666 \
       --vhost-client

Start components for spp_vf.

.. code-block:: console

   # start components
   spp > vf 1; component start classifier 2 classifier_mac
   spp > vf 1; component start merger 3 merge
   spp > vf 1; component start forwarder1 4 forward
   spp > vf 1; component start forwarder2 5 forward
   spp > vf 1; component start forwarder3 6 forward
   spp > vf 1; component start forwarder4 7 forward

Add ports for started components.

.. code-block:: console

   # add ports
   spp > vf 1; port add phy:0 rx classifier
   spp > vf 1; port add phy:0 tx merger
   spp > vf 1; port add ring:0 tx classifier
   spp > vf 1; port add ring:1 tx classifier
   spp > vf 1; port add ring:0 rx forwarder1
   spp > vf 1; port add ring:1 rx forwarder2
   spp > vf 1; port add ring:2 rx merger
   spp > vf 1; port add ring:3 rx merger
   spp > vf 1; port add ring:2 tx forwarder3
   spp > vf 1; port add ring:3 tx forwarder4
   spp > vf 1; port add vhost:0 tx forwarder1
   spp > vf 1; port add vhost:1 rx forwarder3
   spp > vf 1; port add vhost:2 tx forwarder2
   spp > vf 1; port add vhost:3 rx forwarder4

Add classifier table entries.

.. code-block:: console

   # add classifier table entry
   spp > vf 1; classifier_table add mac 52:54:00:12:34:56 ring:0
   spp > vf 1; classifier_table add mac 52:54:00:12:34:58 ring:1


To capture incoming packets on VM1, use tcpdump for the interface, ``ens4``
in this case.

.. code-block:: console

    # capture on ens4 of VM1
    $ tcpdump -i ens4

You send packets from the remote host1 and confirm packets are received.

.. code-block:: console

    # spp-vm1 via NIC0 from host1
    $ ping 192.168.140.21


Mirroring with spp_mirror
-------------------------

The second step is starting with creating VM running with spp_mirror.

Network Configuration
^^^^^^^^^^^^^^^^^^^^^

Incoming packets from NIC are forwarded to VM1 through spp_vf.

Detailed configuration of :numref:`figure_simple_mirroring` is
described below. There are two NICs on the host to send and receive packets.
During that path, mirror component mirror1 replicates packet to merger3.

.. _figure_spp_mirror_usecase_nwconfig:

  .. figure:: ../../images/spp_vf/spp_mirror_usecase_nwconfig.*
     :width: 80%

     Network configuration of mirroring

Launch spp_mirror
^^^^^^^^^^^^^^^^^
Change directory to spp and confirm that it is already compiled.

.. code-block:: console

   $ cd /path/to/spp

Run secondary process ``spp_mirror``.

.. code-block:: console

   $ sudo ./src/mirror/x86_64-native-linuxapp-gcc/app/spp_mirror \
     -l 0,15 -n 4 --proc-type=secondary \
     -- \
     --client-id 2 \
     -s 127.0.0.1:6666 \
     --vhost-client


.. note::
   For SPP secondary processes, client id given with ``--client-id`` option
   should not be overlapped each otherand. It is also the same for core list
   ``-l``.

Start mirror component with core id 15.

.. code-block:: console

    # Start component of spp_mirror on coreID 15
    spp > sec 2;component start mirror1 15 mirror

Add ring:0 as rx ports and add ring:8 and ring:9 as tx port to mirror.

.. code-block:: console

   # mirror1
   spp > mirror 2;port add ring:0 rx mirror1
   spp > mirror 2;port add ring:8 tx mirror1
   spp > mirror 2;port add ring:9 tx mirror1

Start merger3 with core id 14.

.. code-block:: console

   # Start component of spp_vf on coreID 14
   spp > vf 1;component start merger3 14 forward

Add ring:9 as rx port of merger3 and vhost:4 as tx port of merger3.

.. code-block:: console

   # merger3
   spp > vf 1;port add ring:9 rx merger3
   spp > vf 1;port add vhost:4 tx merger3

Delete ring:0 as rx port of forwarder1 and ring:8  as rx port of forwarder1.

.. code-block:: console

   # forward1
   spp > vf 1;port del ring:0 rx forwarder1
   spp > vf 1;port add ring:8 rx forwarder1


Receive packet on VM3
^^^^^^^^^^^^^^^^^^^^^

You can capture incoming packets on VM3.
If you capture packet on VM1, the same packet would be captured.

.. code-block:: console

   # capture on ens4 fo VM1 and VM3
   $ tcpdump -i ens4

Now, you can send packet from the remote host1.

.. code-block:: console

   # spp-vm1 via NIC0 from host1
   $ ping 192.168.140.21


Stop Mirroring
^^^^^^^^^^^^^^

Firstly, delete ports for components.

Delete ports for components.

.. code-block:: console

   # Delete port for mirror1
   spp > mirror 2;port del ring:0 rx mirror1
   spp > mirror 2;port del ring:8 tx mirror1
   spp > mirror 2;port del ring:9 tx mirror1

   # Delete port for merger3
   spp > vf 1;port del ring:9 rx merger3
   spp > vf 1;port del vhost:4 tx merger3

   # Delete port for forwarder1
   spp > vf 1;port del ring:8 rx forwarder1

Next, stop components.

.. code-block:: console

   # Stop mirror
   spp > mirror 2;component stop mirror1 15 mirror

   # Stop merger
   spp > vf 1;component stop merger3 14 forward

Add port from classifier_mac1 to VM1.

.. code-block:: console

    # Add port from classifier_mac1 to VM1.
    spp > vf 1;port add ring:0 rx forwarder1
