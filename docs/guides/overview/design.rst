..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation
    Copyright(c) 2018-2019 Nippon Telegraph and Telephone Corporation

.. _spp_overview_design:

Design
======

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


SPP Controller
--------------

spp-ctl
~~~~~~~

``spp-ctl`` is designed for managing SPP from several controllers
via REST-like APIs for users or other applications.

There are several usecases where SPP is managed from other process without
user inputs. For example, you need a intermediate process if you think of
using SPP from a framework, such as OpenStack.
`networking-spp
<https://github.com/openstack/networking-spp>`_
is a Neutron ML2 plugin for SPP and `spp-agent` works as a SPP controller.


Reference
---------

* [1] `Implementation and Testing of Soft Patch Panel
  <https://dpdksummit.com/Archive/pdf/2017USA/Implementation%20and%20Testing%20of%20Soft%20Patch%20Panel.pdf>`_
* [2] `Integrating OpenStack with DPDK for High Performance Applications
  <https://www.openstack.org/summit/vancouver-2018/summit-schedule/events/20826>`_
