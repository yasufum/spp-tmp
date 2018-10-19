..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation

.. _commands_spp_vf:

SPP VF Commands
===============

Each of secondary processes is managed with ``sec`` command.
It is for sending sub commands to secondary with specific ID called
secondary ID.

``sec`` command takes an secondary ID and a sub command. They must be
separated with delimiter ``;``.
Some of sub commands take additional arguments for speicfying resource
owned by secondary process.

.. code-block:: console

    spp > sec [SID];[SUB_CMD]


status
------

Show running status and resources.

.. code-block:: console

    spp > sec 1;status
    recv:7:{Client ID 1 Idling
    1
    port id: 0,on,PHY,outport: none
    port id: 1,on,PHY,outport: none
    }


component
---------

.. note::
    This command is only supported for spp_vf.

Start or stop a component. SPP VF provides three types of components,
``forwarder``, ``classifier`` and ``merger``.

``component start`` command creates and starts a component with given
options.

.. code-block:: console

    spp > sec [SID];component start [NAME] [CORE_ID] [DIRECTIVE]

* ``NAME`` is used as an identifier of the component.
* ``DIRECTIVE`` is a role of the component and corresponds to three types
  of components.

  * forward
  * merge
  * classifier_mac

This is an example for starting three types of components.

.. code-block:: console

    spp > sec 1;component start forwarder1 2 forward
    spp > sec 1;component start merger1 3 merge
    spp > sec 1;component start classifier1 4 classifier_mac

``component stop`` command terminates a component with given options
same as ``component start`` command..

.. code-block:: console

    spp > sec [SID];component stop [NAME] [CORE_ID] [DIRECTIVE]

This is an example for stopping three types of components.

.. code-block:: console

    spp > sec 1;component stop forwarder1 2 forward
    spp > sec 1;component stop merger1 3 merge
    spp > sec 1;component stop classifier1 4 classifier_mac


port
----

.. note::
    This command is only supported for spp_vf.

Add a port to a component or delete it from.
SPP VF is able to treat VLAN tag by adding port with VLAN options.

``port add`` command adds a port to a component with given options.

.. code-block:: console

    spp > sec [SID];port add [RES_ID] [PORT_TYPE] [NAME]

* ``RES_ID`` is a resource ID and defined as a combination of resource
  type and number separated with delimiter ``:``.
  There are three types of resources.

  * ``phy`` for physical NIC
  * ``vhost`` for vhost PMD
  * ``ring`` for ring PMD

* ``PORT_TYPE`` is ``rx`` or ``tx``.
* ``NAME`` is used as an identifier of the component.

This is an example for adding port ``phy:0`` to ``classifier1`` as
``rx`` and to ``merger1`` as ``tx``.

.. code-block:: console

    spp > sec 1;port add phy:0 rx classifier1
    spp > sec 1;port add phy:0 tx merger1

For VLAN support, you need to add options for ``port add`` command.
To add VLAN tag, additional option ``add_vlantag`` with its options
``VID`` and ``PCP`` are required.

.. code-block:: console

    spp > sec [SID];port add [RES_ID] [PORT_TYPE] [NAME] add_vlantag [VID] [PCP]

* ``PCP`` (Priority Code Point) is an attribute for priority defined in
  IEEE 802.1p standard. It is ranged from 0 to 7 and
  7 is the highest priority.

Or to delete VLAN tag, ``del_vlantag`` option is required.

.. code-block:: console

    spp > sec [SID];port add [RES_ID] [PORT_TYPE] [NAME] del_vlantag

This is an example for adding a port with ``add_vlantag`` or
``del_vlantag``.
In this case, add rx port to append VLAN ID 101 with PCP 3 and
tx port to append VLAN ID 102 with PCP3.

(1) Add VLAN tag

.. code-block:: console

    spp > sec 1;port add phy:0 rx classifier1 add_vlantag 101 3
    spp > sec 1;port add phy:0 tx merger1 add_vlantag 102 3

(2) Delete VLAN tag

.. code-block:: console

    spp > sec 1;port add phy:0 rx classifier1 del_vlantag
    spp > sec 1;port add phy:0 tx merger1 del_vlantag

``port del`` command deletes a port from a component with given options
same as ``port add`` command..

.. code-block:: console

    spp > sec [SID];port del [RES_ID] [PORT_TYPE] [NAME]

This is an example for deleting port added in previous example.

.. code-block:: console

    spp > sec 1;port del phy:0 rx classifier1
    spp > sec 1;port del phy:0 tx merger1


classifier_table
----------------

.. note::
    This command is only supported for spp_vf.

Register an entry as a combination of MAC address and resource ID
to classifier table.

.. code-block:: console

    spp > sec [SID];classifier_table add mac [MAC_ADDRESS] [RES_ID]

This is an example to register MAC address ``52:54:00:01:00:01``
with resource ID ``ring:0``.

.. code-block:: console

    spp > sec 1;classifier_table add mac 52:54:00:01:00:01 ring:0

Here is another example in which using keyword ``default`` instead of
``52:54:00:01:00:01``.
``default`` is applied for packets which are not matched any of registered
MAC addresses.
In this case, all of not matched packets are sent to ``ring:0``.

.. code-block:: console

    spp > sec 0;classifier_table add mac default ring:0

Register an entry with a VLAN ID to classifier table.

.. code-block:: console

    spp > sec 1;classifier_table add vlan [VID] [MAC_ADDRESS] [RES_ID]

This is an example to register MAC address ``52:54:00:01:00:01``
with VLAN ID and resource ID ``ring:0``.

.. code-block:: console

    spp > sec 0;classifier_table add vlan 101 52:54:00:01:00:01 ring:0

Here is another example in which using keyword ``default`` instead of
``52:54:00:01:00:01``.
``default`` is applied for packets which are not matched any of registered
MAC addresses for specified VLAN ID.
In this case, all of not matched packets are sent to ``ring:0``.

.. code-block:: console

    spp > sec 0;classifier_table add vlan 101 default ring:1

Delete an entry.

.. code-block:: console

    spp > sec 1;classifier_table add del [MAC_ADDRESS] [RES_ID]

This is an example to delete an entry for port ``ring:0``.

.. code-block:: console

    spp > sec 1;classifier_table del mac 52:54:00:01:00:01 ring:0

Delete an entry with a VLAN ID.

.. code-block:: console

    spp > sec 1;classifier_table del vlan [VID] [MAC_ADDRESS] [RES_ID]

This is an example to delete an entry with VLAN ID 101.

.. code-block:: console

    spp > sec 0;classifier_table del vlan 101 52:54:00:01:00:01 ring:0
