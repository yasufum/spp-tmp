..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Nippon Telegraph and Telephone Corporation


.. _usecase_hardware_offload:

Hardware Offload
================

SPP provides hardware offload functions.

.. note::

    We tested following use cases at Connect-X 5 by Mellanox only.
    Even if you cannot use these use cases on different NIC, we don't support.

Hardware Classification
-----------------------

Some hardware provides packet classification function based on
L2 mac address. This use case shows you how to use L2 classification.

.. _figure_spp_hardware_offload_classify:

.. figure:: ../images/setup/use_cases/spp_hardware_offload_classify.*
   :width: 100%

Setup
~~~~~

Before using hardware packet classification, you must setup number of queues
in hardware.

In ``bin/config.sh``.

.. code-block:: sh

    PRI_PORT_QUEUE=(
     "0 rxq 10 txq 10"
     "1 rxq 16 txq 16"
    )

Above example includes the line ``0 rxq 10 txq 10``. ``0``
of this line specifies physical port number, ``rxq 10`` is for 10 rx-queues,
``txq 10`` is for 10 tx-queues.

You should uncomment the following block in ``bin/config.sh``
to indicate hardware white list. The option ``dv_flow_en=1``
is for MLX5 poll mode driver.

.. code-block:: sh

    PRI_WHITE_LIST=(
     "0000:04:00.0,dv_flow_en=1"
     "0000:05:00.0"
    )

After editing ``bin/config.sh``, you can launch SPP as following.

.. code-block:: console

    $ bin/start.sh
    Start spp-ctl
    Start spp_primary
    Waiting for spp_primary is ready .................... OK! (8.5[sec])
    Welcome to the SPP CLI. Type `help` or `?` to list commands.
    spp >

Then, you can launch ``spp_vf`` like this.

.. code-block:: none

    spp > pri; launch vf 1 -l 2,3,4,5 -m 512 --file-prefix spp \
    -- --client-id 1 -s 127.0.0.1:6666
    ...

Configuration
~~~~~~~~~~~~~

Before configure the flow of classifying packets, you
can validate such rules by using ``flow validate`` command.

.. code-block:: none

    spp > pri; flow validate phy:0 ingress pattern eth dst is \
    10:22:33:44:55:66 / end actions queue index 1 / end
    spp > pri; flow validate phy:0 ingress pattern eth dst is \
    10:22:33:44:55:67 / end actions queue index 2 / end

Then, you can configure flow using ``flow create`` command like this.

.. code-block:: none

    spp > pri; flow create phy:0 ingress pattern eth dst is \
    10:22:33:44:55:66 / end actions queue index 1 / end
    spp > pri; flow create phy:0 ingress pattern eth dst is \
    10:22:33:44:55:67 / end actions queue index 2 / end

You can confirm created flows by using ``flow list`` or ``flow status``
commands. ``flow list`` command provides the flow information of specified
physical port.

.. code-block:: none

    spp > pri; flow list phy:0
    ID      Group   Prio    Attr    Rule
    0       0       0       i--     ETH => QUEUE
    1       0       0       i--     ETH => QUEUE

To get detailed information for specific rule. The following example shows
the case where showing detailed information for rule ID ``0`` of ``phy:0``.

.. code-block:: none

    spp > pri; flow status phy:0 0
    Attribute:
      Group   Priority Ingress Egress Transfer
      0       0        true    false  false
    Patterns:
      - eth:
        - spec:
          - dst: 10:22:33:44:55:66
          - src: 00:00:00:00:00:00
          - type: 0x0000
        - last:
        - mask:
          - dst: FF:FF:FF:FF:FF:FF
          - src: 00:00:00:00:00:00
          - type: 0x0000
    Actions:
        - queue:
          - index: 1
    spp >

In this use case, two components ``fwd1`` and ``fwd2`` simply forward
the packet to multi-tx queues. You can start these components like this.

.. code-block:: none

    spp > vf 1; component start fwd1 2 forward
    spp > vf 1; component start fwd2 3 forward

For each ``fwd1`` and ``fwd2``, configure the rx port like this.

.. code-block:: none

    spp > vf 1; port add phy:0 nq 1 rx fwd1
    spp > vf 1; port add phy:0 nq 2 rx fwd2

Then, you can configure tx ports like this.

.. code-block:: none

    spp > vf 1; port add phy:1 nq 1 tx fwd1
    spp > vf 1; port add phy:1 nq 2 tx fwd2

For confirming above configuration, you can use ping and tcpdump as described
in :ref:`spp_usecases_vf_cls_icmp`.

Also, when you destroy the flow created above, commands will be like the
following.

.. code-block:: none

    spp > pri; flow destroy phy:0 0
    spp > pri; flow destroy phy:0 1

Or you can destroy all rules on specific hardware
by using ``flow destroy`` command with ``ALL`` parameter.

.. code-block:: none

    spp > pri; flow destroy phy:0 ALL

Manipulate VLAN tag
-------------------

Some hardware provides VLAN tag manipulation function.
This use case shows you the case where incoming VLAN tagged packet detagged
and non-tagged packet tagged when outgoing using hardware offload function.

.. _figure_spp_hardware_offload_vlan:

.. figure:: ../images/setup/use_cases/spp_hardware_offload_vlan.*
   :width: 100%

After having done above use case, you can continue to following.
In this use case, we are assuming incoming packets which includes
``vid=100`` to ``phy:0``, these vid will be removed(detagged) and
transferred to ``fwd1``. Tx packets from ``fwd1`` are sent to
queue#0 on phy:1 with tagged by ``vid=100``. Packets which includes
``vid=200`` to ``phy:0`` are to be sent to ``fwd2`` with removing
the vid,
Tx packets from ``fwd2`` are sent to ``queue#1`` on ``phy:1`` with tagged
by ``vid=200``.

For detagging flow creation.

.. code-block:: none

    spp > pri; flow create phy:0 ingress group 1 pattern eth dst is \
    10:22:33:44:55:66 / vlan vid is 100 / end actions queue index 1 \
    / of_pop_vlan / end
    spp > pri; flow create phy:0 ingress group 1 pattern eth dst is \
    10:22:33:44:55:67 / vlan vid is 200 / end actions queue index 2 \
    / of_pop_vlan / end
    spp > pri; flow create phy:0 ingress group 0 pattern eth / end \
    actions jump group 1 / end

For tagging flow creation.

.. code-block:: none

    spp > pri; flow create phy:1 egress group 1 pattern eth dst is \
    10:22:33:44:55:66 / end actions of_push_vlan ethertype 0x8100 \
    / of_set_vlan_vid vlan_vid 100 / of_set_vlan_pcp vlan_pcp 3 / end
    spp > pri; flow create phy:1 egress group 1 pattern eth dst is \
    10:22:33:44:55:67 / end actions of_push_vlan ethertype 0x8100 \
    / of_set_vlan_vid vlan_vid 200 / of_set_vlan_pcp vlan_pcp 3 / end
    spp > pri; flow create phy:1 egress group 0 pattern eth / end \
    actions jump group 1 / end

If you want to send vlan-tagged packets, the NIC connected to ``phy:0``
will be configured by following.

.. code-block:: sh

    $ sudo ip l add link ens0 name ens0.100 type vlan id 100
    $ sudo ip l add link ens0 name ens0.200 type vlan id 200
    $ sudo ip a add 192.168.140.1/24 dev ens0.100
    $ sudo ip a add 192.168.150.1/24 dev ens0.100
    $ sudo ip l set ens0.100 up
    $ sudo ip l set ens0.200 up


Connecting with VMs
-------------------

This use case shows you how to configure hardware offload and VMs.

.. _figure_spp_hardware_offload_vm:

.. figure:: ../images/setup/use_cases/spp_hardware_offload_vm.*
   :width: 100%

First, we should clean up flows and delete ports.

.. code-block:: none

    spp > vf 1; port del phy:0 nq 0 rx fwd1
    spp > vf 1; port del phy:0 nq 1 rx fwd2
    spp > vf 1; port del phy:1 nq 0 tx fwd1
    spp > vf 1; port del phy:1 nq 1 tx fwd2
    spp > pri; flow destroy phy:0 ALL
    spp > pri; flow destroy phy:1 ALL

Configure flows.

.. code-block:: none

    spp > pri; flow create phy:0 ingress group 1 pattern eth dst is \
    10:22:33:44:55:66 / vlan vid is 100 / end actions queue index 1 \
    / of_pop_vlan / end
    spp > pri; flow create phy:0 ingress group 1 pattern eth dst is \
    10:22:33:44:55:67 / vlan vid is 200 / end actions queue index 2 \
    / of_pop_vlan / end
    spp > pri; flow create phy:0 ingress group 0 pattern eth / end \
    actions jump group 1 / end
    spp > pri; flow create phy:0 egress group 1 pattern eth src is \
    10:22:33:44:55:66 / end actions of_push_vlan ethertype 0x8100 \
    / of_set_vlan_vid vlan_vid 100 / of_set_vlan_pcp vlan_pcp 3 / end
    spp > pri; flow create phy:0 egress group 1 pattern eth src is \
    10:22:33:44:55:67 / end actions of_push_vlan ethertype 0x8100 \
    / of_set_vlan_vid vlan_vid 200 / of_set_vlan_pcp vlan_pcp 3 / end
    spp > pri; flow create phy:0 egress group 0 pattern eth / end \
    actions jump group 1 / end

Start components.

.. code-block:: none

    spp > vf 1; component start fwd3 4 forward
    spp > vf 1; component start fwd4 5 forward

Start and setup two VMs as described in :ref:`spp_usecases_vf_ssh`.
Add ports to forwarders.

.. code-block:: none

    spp > vf 1; port add phy:0 nq 1 rx fwd1
    spp > vf 1; port add vhost:0 tx fwd1
    spp > vf 1; port add phy:0 nq 2 rx fwd2
    spp > vf 1; port add vhost:1 tx fwd2
    spp > vf 1; port add vhost:0 rx fwd3
    spp > vf 1; port add phy:0 nq 3 tx fwd3
    spp > vf 1; port add vhost:1 rx fwd4
    spp > vf 1; port add phy:0 nq 4 tx fwd4

Then you can login to each VMs.

Note that you must add arp entries of MAC addresses statically to be resolved.

.. code-block:: none

   # terminal 1 on remote host
   # set MAC address
   $ sudo arp -i ens0 -s 192.168.140.31 10:22:33:44:55:66
   $ sudo arp -i ens0 -s 192.168.150.32 10:22:33:44:55:67


Reference
---------

The following features are tested.

MT27710 Family [ConnectX-4 Lx] 1015
- dstMAC
- dstMAC(range)

MT27800 Family [ConnectX-5] 1017
- dstMAC
- dstMAC(range)
- vlan vid
- vlan vid+dstMAC
- tagging+detagging

Ethernet Controller XXV710 for 25GbE SFP28 158b
- dstMAC
