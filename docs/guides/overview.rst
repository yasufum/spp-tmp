..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation

.. _spp_overview:

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

The goal of SPP is to easily interconnect NFV applications via high
thoughput network interfaces provided by DPDK and change configurations
of resources dynamically to applications to build pipelines.


Design
------

SPP is composed of several DPDK processes and controller processes [1].

In terms of DPDK processes, SPP is derived from DPDK's multi-process sample
application and it consists of a primary process and multiple secondary
processes.
SPP primary process is responsible for resource management, for example, ports,
mbufs or shared memory. On the other hand, secondary processes are working for
tasks.

SPP is controlled from python based management framework. It consists of
front-end CLI and back-end server process.
SPP's front-end CLI provides a patch panel like interface for users.
This CLI process parses user input and sends request to the back-end via REST
APIs. It means that the back-end server process accepts requests from other
than CLI. It enables developers to implement control interface such as GUI, or
plugin for other framework.
`networking-spp
<https://github.com/openstack/networking-spp>`_
is a Neutron ML2 plugin for using SPP with OpenStack.
By using networking-spp and doing some of extra tunings for optimization, you
can deploy high-performance NFV services on OpenStack [2].


Reference
---------

* [1] `Implementation and Testing of Soft Patch Panel
  <https://dpdksummit.com/Archive/pdf/2017USA/Implementation%20and%20Testing%20of%20Soft%20Patch%20Panel.pdf>`_
* [2] `Integrating OpenStack with DPDK for High Performance Applications
  <https://www.openstack.org/summit/vancouver-2018/summit-schedule/events/20826>`_
