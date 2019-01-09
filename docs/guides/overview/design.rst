..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation
    Copyright(c) 2018-2019 Nippon Telegraph and Telephone Corporation

.. _spp_overview_design:

Design
======

SPP is composed of several DPDK processes and controller processes for
connecting each of client processes with high-throughput path of DPDK.
:numref:`figure_spp_overview_design_all` shows SPP processes and client apps
for describing overview of design of SPP. In this diagram, solid line arrows
describe a data path for packet forwarding and it can be configured from
controller via command messaging of blue dashed line arrows.

.. _figure_spp_overview_design_all:

.. figure:: ../images/overview/design/spp_overview_design_all.*
   :width: 85%

   Overview of design of SPP

In terms of DPDK processes, SPP is derived from DPDK's multi-process sample
application and it consists of a primary process and multiple secondary
processes.
SPP primary process is responsible for resource management, for example,
initializing ports, mbufs or shared memory. On the other hand,
secondary processes of ``spp_nfv`` are working for forwarding [1].


.. _spp_overview_spp_controller:

SPP Controller
--------------

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

As shown in :numref:`figure_spp_overview_design_spp_ctl`,
``spp-ctl`` behaves as a TCP server for SPP primary and secondary processes,
and REST API server for client applications.
It should be launched in advance to setup connections with other processes.
``spp-ctl``  uses three TCP ports for primary, secondaries and clients.
The default port numbers are ``5555``, ``6666`` and ``7777``.

.. _figure_spp_overview_design_spp_ctl:

.. figure:: ../images/overview/design/spp_overview_design_spp-ctl.*
   :width: 48%

   Spp-ctl as a REST API server

SPP CLI
~~~~~~~

SPP CLI is a user interface for managing SPP and implemented as a client of
``spp-ctl``. It provides several kinds of command for inspecting SPP
processes, changing path configuration or showing statistics of packets.
However, you do not need to use SPP CLI if you use ``netowrking-spp`` or
other client applications of ``spp-ctl``. SPP CLI is one of them.

From SPP CLI, user is able to configure paths as similar as
patch panel like manner by sending commands to each of SPP secondary processes.
``patch phy:0 ring:0`` is to connect two ports, ``phy:0`` and ``ring:0``.

As described in :ref:`Getting Started<spp_setup_howto_use_spp_cli>` guide,
SPP CLI is able to communicate several ``spp-ctl`` to support multiple nodes
configuration.


.. _spp_overview_design_spp_primary:

SPP Primary
-----------

SPP is originally derived from
`Client-Server Multi-process Example
<https://doc.dpdk.org/guides/sample_app_ug/multi_process.html#client-server-multi-process-example>`_
of
`Multi-process Sample Application
<https://doc.dpdk.org/guides/sample_app_ug/multi_process.html>`_
in DPDK's sample applications.
``spp_primary`` is a server process for other secondary processes and
basically working as described in
"How the Application Works" section of the sample application.

However, there are also differences between ``spp_primary`` and
the server process of the sample application.
``spp_primary`` has no limitation of the number of secondary processes.
It does not work for packet forwaring, but just provide rings and memory pools
for secondary processes.


Reference
---------

* [1] `Implementation and Testing of Soft Patch Panel
  <https://dpdksummit.com/Archive/pdf/2017USA/Implementation%20and%20Testing%20of%20Soft%20Patch%20Panel.pdf>`_
* [2] `Integrating OpenStack with DPDK for High Performance Applications
  <https://www.openstack.org/summit/vancouver-2018/summit-schedule/events/20826>`_
