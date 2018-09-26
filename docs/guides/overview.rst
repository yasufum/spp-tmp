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

Soft Patch Panel
==================

Overview
--------

`Soft Patch Panel
<http://dpdk.org/browse/apps/spp/>`_
(SPP) is a DPDK application for providing switching
functionality for Service Function Chaining in
NFV (Network Function Virtualization).

.. figure:: images/overview/spp_overview.*
    :width: 50%

    SPP overview

With SPP, user is able to configure network easily and dynamically
via simple patch panel like interface.

The goal of SPP is to easily interconnect DPDK applications together,
and assign resources dynamically to these applications to build a
pipeline.

Design
------

SPP is composed of a primary DPDK application that is
responsible for resource management. This primary application doesn't
interact with any traffic, and is used to manage creation and freeing of
resources only.

A Python based management interface, SPP controller, is provided to
control the primary
DPDK application to create resources, which are then to be used by
secondary applications.
This management application provides a socket
based interface for the primary and secondary DPDK applications to
interface to the manager.

Reference
---------

* [1] `Implementation and Testing of Soft Patch Panel <https://dpdksummit.com/Archive/pdf/2017USA/Implementation%20and%20Testing%20of%20Soft%20Patch%20Panel.pdf>`_
