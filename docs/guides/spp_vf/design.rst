..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation

.. _spp_vf_design:

Design
======

.. _spp_vf_design_port:

Ports
-----

Both of ``spp_vf`` and ``spp_mirror`` support three types of port,
``phy``, ``ring`` and ``vhost``.
``phy`` port is used to retrieve packets from specific physical NIC or sent to.
``ring`` is basically used to connect a process or a thread to make a network
path between them.
``vhost`` is used to forward packets from a VM or sent to.

.. _spp_vf_design_spp_vf:

spp_vf
------

``spp_vf`` is a kind of secondary process and consists of several
threads called component.
There are three types of components, ``forwarder``,
``merger`` and ``classifier``.

.. figure:: ../images/spp_vf/spp_vf_overview.*
    :width: 75%

    SPP VF components

Forwarder
^^^^^^^^^

Simply forwards packets from rx to tx port.

Merger
^^^^^^

Receives packets from multiple rx ports to aggregate
packets and sends to a desctination port.

Classifier
^^^^^^^^^^

Sends packets to multiple tx ports based on entries of
MAC address and destination port in a classifier table.
This component also supports VLAN tag.

For VLAN addressing, classifier has other tables than defalut.
Classifier prepares tables for each of VLAN ID and decides
which of table is referred
if TPID (Tag Protocol Indetifier) is included in a packet and
equals to 0x8100 as defined in IEEE 802.1Q standard.

.. _spp_vf_design_spp_mirror:

spp_mirror
----------

``spp_mirror`` is another kind of secondary process. The keyword ``mirror``
means that it duplicates incoming packets and forwards to additional
destination.
It supports only one type of component called ``mirror`` for duplicating.
In :numref:`figure_spp_mirror_design`, incoming packets are duplicated with
``mirror`` component and sent to original and additional destinations.

.. _figure_spp_mirror_design:

.. figure:: ../images/spp_vf/spp_mirror_design.*
    :width: 45%

    Spp_mirror component

Mirror
^^^^^^

``mirror`` component has one ``rx`` port and two ``tx`` ports. Incoming packets
from ``rx`` port are duplicated and sent to each of ``tx`` ports.

In general, copying packet is time-consuming because it requires to make a new
region on memory space. Considering to minimize impact for performance,
``spp_mirror`` provides a choice of copying methods, ``shallowocopy`` or
``deepcopy``.
The difference between those methods is ``shallowocopy`` does not copy whole of
packet data but share without header actually.
``shallowcopy`` is to share mbuf between packets to get better performance
than ``deepcopy``, but it should be used for read only for the packet.

.. note::

    ``shallowcopy`` calls ``rte_pktmbuf_clone()`` internally and
    ``deepcopy`` create a new mbuf region.

You should choose ``deepcopy`` if you use VLAN feature to make no change for
original packet while copied packet is modified.
