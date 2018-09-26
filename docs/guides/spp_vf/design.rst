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
