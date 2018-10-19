..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation

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

* [1] `Implementation and Testing of Soft Patch Panel
  <https://dpdksummit.com/Archive/pdf/2017USA/Implementation%20and%20Testing%20of%20Soft%20Patch%20Panel.pdf>`_
