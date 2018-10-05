..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

.. _spp_ctl_api_ref:

=============
API Reference
=============

Overview
========

``spp-ctl`` provides simple REST like API. It supports http only, not https.

Request and Response
--------------------

Request body is JSON format.
It is accepted both ``text/plain`` and ``application/json``
for the content-type header.

Response of ``GET`` is JSON format and the content-type is
``application/json`` if the request success.

If a request fails, the response is a text which shows error reason
and the content-type is ``text/plain``.

Error code
----------

``spp-ctl`` does basic syntax and lexical check of a request.

+------+----------------------------------------------------------------+
| Error| Description                                                    |
+======+================================================================+
| 400  | Syntax or lexical error, or SPP returns error for the request. |
+------+----------------------------------------------------------------+
| 404  | URL is not supported, or no SPP process of client-id in a URL. |
+------+----------------------------------------------------------------+
| 500  | System error occured in ``spp-ctl``.                           |
+------+----------------------------------------------------------------+


API independent of the process type
===================================

GET /v1/processes
-----------------

Show the SPP processes connected to the ``spp-ctl``.

* Normarl response codes: 200

Request example
^^^^^^^^^^^^^^^

.. code-block:: console

    curl -X GET -H 'application/json' \
    http://127.0.0.1:7777/v1/processes

Response
^^^^^^^^

An array of process objects.

process object:

+-----------+---------+---------------------------------------------------------------+
| Name      | Type    | Description                                                   |
+===========+=========+===============================================================+
| type      | string  | process type. one of ``primary``, ``vf`` or ``nfv``.          |
+-----------+---------+---------------------------------------------------------------+
| client-id | integer | client id. if type is ``primary`` this member does not exist. |
+-----------+---------+---------------------------------------------------------------+

Response example
^^^^^^^^^^^^^^^^

.. code-block:: yaml

    [
      {
        "type": "primary"
      },
      {
        "type": "vf",
        "client-id": 1
      },
      {
        "type": "nfv",
        "client-id": 2
      }
    ]


API for spp_primary
===================

GET /v1/primary/status
----------------------

Show statistical information.

* Normal response codes: 200

Request example
^^^^^^^^^^^^^^^

.. code-block:: console

    curl -X GET -H 'application/json' \
    http://127.0.0.1:7777/v1/primary/status

Response
^^^^^^^^

There is no data at the moment. The statistical information will be returned
when ``spp_primary`` implements it.


DELETE /v1/primary/status
-------------------------

Clear statistical information.

* Normal response codes: 204

Request example
^^^^^^^^^^^^^^^

.. code-block:: console

    curl -X DELETE -H 'application/json' \
    http://127.0.0.1:7777/v1/primary/status

Response
^^^^^^^^

There is no body content for the response of a successful ``PUT`` request.


API for spp_nfv/spp_vm
======================

GET /v1/nfvs/{client_id}
------------------------

Get the information of the ``spp_nfv`` or ``spp_vm`` process.

* Normal response codes: 200
* Error response codes: 400, 404

Request(path)
^^^^^^^^^^^^^

+-----------+---------+-------------+
| Name      | Type    | Description |
+===========+=========+=============+
| client_id | integer | client id.  |
+-----------+---------+-------------+

Request example
^^^^^^^^^^^^^^^

.. code-block:: console

    curl -X GET -H 'application/json' \
    http://127.0.0.1:7777/v1/nfvs/1

Response
^^^^^^^^

+-----------+---------+-------------------------------------------+
| Name      | Type    | Description                               |
+===========+=========+===========================================+
| client-id | integer | client id.                                |
+-----------+---------+-------------------------------------------+
| status    | string  | ``Running`` or ``Idle``.                  |
+-----------+---------+-------------------------------------------+
| ports     | array   | an array of port ids used by the process. |
+-----------+---------+-------------------------------------------+
| patches   | array   | an array of patches.                      |
+-----------+---------+-------------------------------------------+

patch objest

+------+--------+----------------------+
| Name | Type   | Description          |
+======+========+======================+
| src  | string | source port id.      |
+------+--------+----------------------+
| dst  | string | destination port id. |
+------+--------+----------------------+

Response example
^^^^^^^^^^^^^^^^

.. code-block:: yaml

    {
      "client-id": 1,
      "status": "Running",
      "ports": [
        "phy:0", "phy:1", "vhost:0", "vhost:1", "ring:0", "ring:1", "ring:2", "ring:3"
      ],
      "patches": [
        {
          "src": "vhost:0", "dst": "ring:0"
        },
        {
          "src": "ring:1", "dst": "vhost:1"
        }
      ]
    }

Equivalent CLI command
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: console

    sec {client_id};status


PUT /v1/nfvs/{client_id}/forward
--------------------------------

Start or Stop forwarding.

* Normal response codes: 204
* Error response codes: 400, 404

Request(path)
^^^^^^^^^^^^^

+-----------+---------+-------------+
| Name      | Type    | Description |
+===========+=========+=============+
| client_id | integer | client id.  |
+-----------+---------+-------------+

Request example
^^^^^^^^^^^^^^^

.. code-block:: console

    curl -X PUT -H 'application/json' \
    -d '{"action": "start"}' \
    http://127.0.0.1:7777/v1/nfvs/1/forward

Request(body)
^^^^^^^^^^^^^

+--------+--------+------------------------+
| Name   | Type   | Description            |
+========+========+========================+
| action | string | ``start`` or ``stop``. |
+--------+--------+------------------------+

Response
^^^^^^^^

There is no body content for the response of a successful ``PUT`` request.

Equivalent CLI command
^^^^^^^^^^^^^^^^^^^^^^

action is ``start``

.. code-block:: yaml

    sec {client_id};forward

action is ``stop``

.. code-block:: yaml

    sec {client_id};stop


PUT /v1/nfvs/{client_id}/ports
------------------------------

Add or Delete port.

* Normal response codes: 204
* Error response codes: 400, 404

Request(path)
^^^^^^^^^^^^^

+-----------+---------+-------------+
| Name      | Type    | Description |
+===========+=========+=============+
| client_id | integer | client id.  |
+-----------+---------+-------------+

Request(body)
^^^^^^^^^^^^^

+--------+--------+---------------------------------------------------------------+
| Name   | Type   | Description                                                   |
+========+========+===============================================================+
| action | string | ``add`` or ``del``.                                           |
+--------+--------+---------------------------------------------------------------+
| port   | string | port id. port id is the form {interface_type}:{interface_id}. |
+--------+--------+---------------------------------------------------------------+

Request example
^^^^^^^^^^^^^^^

.. code-block:: console

    curl -X PUT -H 'application/json' \
    -d '{"action": "add", "port": "ring:0"}' \
    http://127.0.0.1:7777/v1/nfvs/1/ports

Response
^^^^^^^^

There is no body content for the response of a successful ``PUT`` request.

Equivalent CLI command
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: console

    sec {client_id};{action} {interface_type} {interface_id}


PUT /v1/nfvs/{client_id}/patches
--------------------------------

Add a patch.

* Normal response codes: 204
* Error response codes: 400, 404

Request(path)
^^^^^^^^^^^^^

+-----------+---------+-------------+
| Name      | Type    | Description |
+===========+=========+=============+
| client_id | integer | client id.  |
+-----------+---------+-------------+

Request(body)
^^^^^^^^^^^^^

+------+--------+----------------------+
| Name | Type   | Description          |
+======+========+======================+
| src  | string | source port id.      |
+------+--------+----------------------+
| dst  | string | destination port id. |
+------+--------+----------------------+

Request example
^^^^^^^^^^^^^^^

.. code-block:: console

    curl -X PUT -H 'application/json' \
    -d '{"src": "ring:0", "dst": "ring:1"}' \
    http://127.0.0.1:7777/v1/nfvs/1/patches

Response
^^^^^^^^

There is no body content for the response of a successful ``PUT`` request.

Equivalent CLI command
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: console

    sec {client_id};patch {src} {dst}


DELETE /v1/nfvs/{client_id}/patches
-----------------------------------

Reset patches.

* Normal response codes: 204
* Error response codes: 400, 404

Request(path)
^^^^^^^^^^^^^

+-----------+---------+-------------+
| Name      | Type    | Description |
+===========+=========+=============+
| client_id | integer | client id.  |
+-----------+---------+-------------+

Request example
^^^^^^^^^^^^^^^

.. code-block:: console

    curl -X DELETE -H 'application/json' \
    http://127.0.0.1:7777/v1/nfvs/1/patches

Response
^^^^^^^^

There is no body content for the response of a successful ``DELETE`` request.

Equivalent CLI command
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: console

    sec {client_id};patch reset


API for spp_vf
==============

GET /v1/vfs/{client_id}
-----------------------

Get the information of the ``spp_vf`` process.

* Normal response codes: 200
* Error response codes: 400, 404

Request(path)
^^^^^^^^^^^^^

+-----------+---------+-------------+
| Name      | Type    | Description |
+===========+=========+=============+
| client_id | integer | client id.  |
+-----------+---------+-------------+

Request example
^^^^^^^^^^^^^^^

.. code-block:: console

    curl -X GET -H 'application/json' \
    http://127.0.0.1:7777/v1/vfs/1

Response
^^^^^^^^

+------------------+---------+-----------------------------------------------+
| Name             | Type    | Description                                   |
+==================+=========+===============================================+
| client-id        | integer | client id.                                    |
+------------------+---------+-----------------------------------------------+
| ports            | array   | an array of port ids used by the process.     |
+------------------+---------+-----------------------------------------------+
| components       | array   | an array of component objects in the process. |
+------------------+---------+-----------------------------------------------+
| classifier_table | array   | an array of classifier tables in the process. |
+------------------+---------+-----------------------------------------------+

component object:

+---------+---------+---------------------------------------------------------------------+
| Name    | Type    | Description                                                         |
+=========+=========+=====================================================================+
| core    | integer | core id running on the component                                    |
+---------+---------+---------------------------------------------------------------------+
| name    | string  | an array of port ids used by the process.                           |
+---------+---------+---------------------------------------------------------------------+
| type    | string  | an array of component objects in the process.                       |
+---------+---------+---------------------------------------------------------------------+
| rx_port | array   | an array of port objects connected to the rx side of the component. |
+---------+---------+---------------------------------------------------------------------+
| tx_port | array   | an array of port objects connected to the tx side of the component. |
+---------+---------+---------------------------------------------------------------------+

port object:

+---------+---------+---------------------------------------------------------------+
| Name    | Type    | Description                                                   |
+=========+=========+===============================================================+
| port    | string  | port id. port id is the form {interface_type}:{interface_id}. |
+---------+---------+---------------------------------------------------------------+
| vlan    | object  | vlan operation which is applied to the port.                  |
+---------+---------+---------------------------------------------------------------+

vlan object:

+-----------+---------+-------------------------------+
| Name      | Type    | Description                   |
+===========+=========+===============================+
| operation | string  | ``add``, ``del`` or ``none``. |
+-----------+---------+-------------------------------+
| id        | integer | vlan id.                      |
+-----------+---------+-------------------------------+
| pcp       | integer | vlan pcp.                     |
+-----------+---------+-------------------------------+

classifier table:

+-----------+--------+------------------------------------------------------------+
| Name      | Type   | Description                                                |
+===========+========+============================================================+
| type      | string | ``mac`` or ``vlan``.                                       |
+-----------+--------+------------------------------------------------------------+
| value     | string | mac_address for ``mac``, vlan_id/mac_address for ``vlan``. |
+-----------+--------+------------------------------------------------------------+
| port      | string | port id applied to classify.                               |
+-----------+--------+------------------------------------------------------------+

Response example
^^^^^^^^^^^^^^^^

.. code-block:: yaml

    {
      "client-id": 1,
      "ports": [
        "phy:0", "phy:1", "vhost:0", "vhost:1", "ring:0", "ring:1", "ring:2", "ring:3"
      ],
      "components": [
        {
          "core": 2,
          "name": "forward_0_tx",
          "type": "forward",
          "rx_port": [
            {
            "port": "ring:0",
            "vlan": { "operation": "none", "id": 0, "pcp": 0 }
            }
          ],
          "tx_port": [
            {
              "port": "vhost:0",
              "vlan": { "operation": "none", "id": 0, "pcp": 0 }
            }
          ]
        },
        {
          "core": 3,
          "type": "unuse"
        },
        {
          "core": 4,
          "type": "unuse"
        },
        {
          "core": 5,
          "name": "forward_1_rx",
          "type": "forward",
          "rx_port": [
            {
            "port": "vhost:1",
            "vlan": { "operation": "none", "id": 0, "pcp": 0 }
            }
          ],
          "tx_port": [
            {
              "port": "ring:3",
              "vlan": { "operation": "none", "id": 0, "pcp": 0 }
            }
          ]
        },
        {
          "core": 6,
          "name": "classifier",
          "type": "classifier_mac",
          "rx_port": [
            {
              "port": "phy:0",
              "vlan": { "operation": "none", "id": 0, "pcp": 0 }
            }
          ],
          "tx_port": [
            {
              "port": "ring:0",
              "vlan": { "operation": "none", "id": 0, "pcp": 0 }
            },
            {
              "port": "ring:2",
              "vlan": { "operation": "none", "id": 0, "pcp": 0 }
            }
          ]
        },
        {
          "core": 7,
          "name": "merger",
          "type": "merge",
          "rx_port": [
            {
              "port": "ring:1",
              "vlan": { "operation": "none", "id": 0, "pcp": 0 }
            },
            {
              "port": "ring:3",
              "vlan": { "operation": "none", "id": 0, "pcp": 0 }
            }
          ],
          "tx_port": [
            {
              "port": "phy:0",
              "vlan": { "operation": "none", "id": 0, "pcp": 0 }
            }
          ]
        },
      ],
      "classifier_table": [
        {
          "type": "mac",
          "value": "FA:16:3E:7D:CC:35",
          "port": "ring:0"
        }
      ]
    }

The component which type is ``unused`` is to indicate unused core.

Equivalent CLI command
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: console

    sec {client_id};status


POST /v1/vfs/{client_id}/components
-----------------------------------

Start the component.

* Normal response codes: 204
* Error response codes: 400, 404

Request(path)
^^^^^^^^^^^^^

+-----------+---------+-------------+
| Name      | Type    | Description |
+===========+=========+=============+
| client_id | integer | client id.  |
+-----------+---------+-------------+


Request(body)
^^^^^^^^^^^^^

+-----------+---------+----------------------------------------------------------------------+
| Name      | Type    | Description                                                          |
+===========+=========+======================================================================+
| name      | string  | component name. must be unique in the process.                       |
+-----------+---------+----------------------------------------------------------------------+
| core      | integer | core id.                                                             |
+-----------+---------+----------------------------------------------------------------------+
| type      | string  | component type. one of ``forward``, ``merge`` or ``classifier_mac``. |
+-----------+---------+----------------------------------------------------------------------+

Request example
^^^^^^^^^^^^^^^

.. code-block:: console

    curl -X POST -H 'application/json' \
    -d '{"name": "forwarder1", "core": 12, "type": "forward"}' \
    http://127.0.0.1:7777/v1/vfs/1/components

Response
^^^^^^^^

There is no body content for the response of a successful ``POST`` request.

Equivalent CLI command
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: console

    sec {client_id};component start {name} {core} {type}


DELETE /v1/vfs/{sec id}/components/{name}
-----------------------------------------

Stop the component.

* Normal response codes: 204
* Error response codes: 400, 404

Request(path)
^^^^^^^^^^^^^

+-----------+---------+-----------------+
| Name      | Type    | Description     |
+===========+=========+=================+
| client_id | integer | client id.      |
+-----------+---------+-----------------+
| name      | string  | component name. |
+-----------+---------+-----------------+

Request example
^^^^^^^^^^^^^^^

.. code-block:: console

    curl -X DELETE -H 'application/json' \
    http://127.0.0.1:7777/v1/vfs/1/components/forwarder1

Response
^^^^^^^^

There is no body content for the response of a successful ``POST`` request.

Equivalent CLI command
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: console

    sec {client_id};component stop {name}


PUT /v1/vfs/{client_id}/components/{name}/ports
-----------------------------------------------

Add or Delete port to the component.

* Normal response codes: 204
* Error response codes: 400, 404

Request(path)
^^^^^^^^^^^^^

+-----------+---------+-----------------+
| Name      | Type    | Description     |
+===========+=========+=================+
| client_id | integer | client id.      |
+-----------+---------+-----------------+
| name      | string  | component name. |
+-----------+---------+-----------------+

Request(body)
^^^^^^^^^^^^^

+---------+---------+-----------------------------------------------------------------+
| Name    | Type    | Description                                                     |
+=========+=========+=================================================================+
| action  | string  | ``attach`` or ``detach``.                                       |
+---------+---------+-----------------------------------------------------------------+
| port    | string  | port id. port id is the form {interface_type}:{interface_id}.   |
+---------+---------+-----------------------------------------------------------------+
| dir     | string  | ``rx`` or ``tx``.                                               |
+---------+---------+-----------------------------------------------------------------+
| vlan    | object  | vlan operation which is applied to the port. it can be omitted. |
+---------+---------+-----------------------------------------------------------------+

vlan object:

+-----------+---------+----------------------------------------------------------+
| Name      | Type    | Description                                              |
+===========+=========+==========================================================+
| operation | string  | ``add``, ``del`` or ``none``.                            |
+-----------+---------+----------------------------------------------------------+
| id        | integer | vlan id. ignored when operation is ``del`` or ``none``.  |
+-----------+---------+----------------------------------------------------------+
| pcp       | integer | vlan pcp. ignored when operation is ``del`` or ``none``. |
+-----------+---------+----------------------------------------------------------+

Request example
^^^^^^^^^^^^^^^

.. code-block:: console

    curl -X PUT -H 'application/json' \
    -d '{"action": "attach", "port": "vhost:1", "dir": "rx", \
         "vlan": {"operation": "add", "id": 677, "pcp": 0}}' \
    http://127.0.0.1:7777/v1/vfs/1/components/forwarder1/ports

.. code-block:: console

    curl -X PUT -H 'application/json' \
    -d '{"action": "detach", "port": "vhost:0", "dir": "tx"} \
    http://127.0.0.1:7777/v1/vfs/1/components/forwarder1/ports

Response
^^^^^^^^

There is no body content for the response of a successful ``PUT`` request.

Equivalent CLI command
^^^^^^^^^^^^^^^^^^^^^^

action is ``attach``

.. code-block:: console

    sec {client_id};port add {port} {dir} {name} [add_vlantag {id} {pcp} | del_vlantag]

action is ``detach``

.. code-block:: console

    sec {client_id};port del {port} {dir} {name}


PUT /v1/vfs/{sec id}/classifier_table
-------------------------------------

Set or Unset classifier table.

* Normal response codes: 204
* Error response codes: 400, 404

Request(path)
^^^^^^^^^^^^^

+-----------+---------+-------------+
| Name      | Type    | Description |
+===========+=========+=============+
| client_id | integer | client id.  |
+-----------+---------+-------------+

Request(body)
^^^^^^^^^^^^^

+-------------+-----------------+----------------------------------------------------+
| Name        | Type            | Description                                        |
+=============+=================+====================================================+
| action      | string          | ``add`` or ``del``.                                |
+-------------+-----------------+----------------------------------------------------+
| type        | string          | ``mac`` or ``vlan``.                               |
+-------------+-----------------+----------------------------------------------------+
| vlan        | integer or null | vlan id for ``vlan``. null or omitted for ``mac``. |
+-------------+-----------------+----------------------------------------------------+
| mac_address | string          | mac address.                                       |
+-------------+-----------------+----------------------------------------------------+
| port        | string          | port id.                                           |
+-------------+-----------------+----------------------------------------------------+

Request example
^^^^^^^^^^^^^^^

.. code-block:: console

    curl -X PUT -H 'application/json' \
    -d '{"action": "add", "type": "mac", "mac_address": "FA:16:3E:7D:CC:35", \
       "port": "ring:0"}' \
    http://127.0.0.1:7777/v1/vfs/1/classifier_table

.. code-block:: console

    curl -X PUT -H 'application/json' \
    -d '{"action": "del", "type": "vlan", "vlan": 475, \
       "mac_address": "FA:16:3E:7D:CC:35", "port": "ring:0"}' \
    http://127.0.0.1:7777/v1/vfs/1/classifier_table

Response
^^^^^^^^

There is no body content for the response of a successful ``PUT`` request.

Equivalent CLI command
^^^^^^^^^^^^^^^^^^^^^^

type is ``mac``

.. code-block:: console

    classifier_table {action} mac {mac_address} {port}

type is ``vlan``

.. code-block:: console

    classifier_table {action} vlan {vlan} {mac_address} {port}
