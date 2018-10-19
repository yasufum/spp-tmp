..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation

.. _spp_vf_design:

Design
======

Components
----------

``spp_vf`` is a kind of secondary process and consists of several
threads called component.
There are three types of components, ``forwarder``,
``merger`` and ``classifier``.

.. figure:: ../images/spp_vf/spp_vf_overview.*
    :width: 70%

    SPP VF components

Forwarder
~~~~~~~~~

Simply forwards packets from rx to tx port.

Merger
~~~~~~

Receives packets from multiple rx ports to aggregate
packets and sends to a desctination port.

Classifier
~~~~~~~~~~

Sends packets to multiple tx ports based on entries of
MAC address and destination port in a classifier table.
This component also supports VLAN tag.

For VLAN addressing, classifier has other tables than defalut.
Classifier prepares tables for each of VLAN ID and decides
which of table is referred
if TPID (Tag Protocol Indetifier) is included in a packet and
equals to 0x8100 as defined in IEEE 802.1Q standard.


Ports
-----

``spp_vf`` supports three types of PMDs, ``phy`` (Physical NIC),
``ring`` (Ring PMD) and ``vhost`` (vhsot PMD).
Using ``phy`` port, component can get incoming packets from outside host
and transfer the packet to specific physical NIC.
Using ``ring`` port, variety of combination of components can be
configured.
And through ``vhost`` port component can transfer packets from/to VMs.
``port`` can also control vlan tagging and untagging.
