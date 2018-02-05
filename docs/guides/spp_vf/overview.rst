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

.. image:: images/spp_vf_overview.svg
   :height: 550 em
   :width: 550 em
