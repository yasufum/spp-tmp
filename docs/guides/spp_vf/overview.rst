..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation

.. _spp_vf_overview:

Overview
========

SPP_VF provides SR-IOV like network functionality using DPDK for NFV.

PP_VF distributes incoming packets to VMs with referring to virtual
MAC address like Virtual Function(VF) of SR-IOV.
Virtual MAC address can be defined by commands from spp
controller(``spp_vf.py``).

SPP_VF is multi-process and multi-thread applications. A SPP_VF process
is referred to as ``spp_vf`` in this document. Each ``spp_vf`` has
one manager thread and component threads. The manager thread provides
function for command processing and creating the component threads.
The component threads have its own multiple components, ports and
classifier tables including Virtual MAC address.


This is an example of network configuration, in which one
``classifier_mac``,
one merger and four forwarders are running in SPP_VF process
for two destinations of vhost interface.
Incoming packets from rx on host1 are sent to each of vhosts of VM
by looking up destination MAC address in the packet.

.. figure:: ../images/spp_vf/spp_vf_overview.*
    :width: 70%

    Overview of SPP VF
