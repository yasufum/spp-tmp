..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018-2019 Nippon Telegraph and Telephone Corporation


.. _spp_ctl_rest_api_spp_primary:

API for spp_primary
===================

GET /v1/primary/status
----------------------

Show statistical information.

* Normal response codes: 200


Request example
~~~~~~~~~~~~~~~

.. code-block:: console

    $ curl -X GET -H 'application/json' \
      http://127.0.0.1:7777/v1/primary/status


Response
~~~~~~~~

.. _table_spp_ctl_primary_status:

.. table:: Response params of primary status.

    +------------+-------+----------------------------------------+
    | Name       | Type  | Description                            |
    |            |       |                                        |
    +============+=======+========================================+
    | lcores     | array | Array of lcores spp_primary is using.  |
    +------------+-------+----------------------------------------+
    | phy_ports  | array | Array of statistics of physical ports. |
    +------------+-------+----------------------------------------+
    | ring_ports | array | Array of statistics of ring ports.     |
    +------------+-------+----------------------------------------+

Physical port object.

.. _table_spp_ctl_primary_status_phy:

.. table:: Attributes of physical port of primary status.

    +---------+---------+-----------------------------------------------------+
    | Name    | Type    | Description                                         |
    |         |         |                                                     |
    +=========+=========+=====================================================+
    | id      | integer | Port ID of the physical port.                       |
    +---------+---------+-----------------------------------------------------+
    | rx      | integer | The total number of received packets.               |
    +---------+---------+-----------------------------------------------------+
    | tx      | integer | The total number of transferred packets.            |
    +---------+---------+-----------------------------------------------------+
    | tx_drop | integer | The total number of dropped packets of transferred. |
    +---------+---------+-----------------------------------------------------+
    | eth     | string  | MAC address of the port.                            |
    +---------+---------+-----------------------------------------------------+

Ring port object.

.. _table_spp_ctl_primary_status_ring:

.. table:: Attributes of ring port of primary status.

    +---------+---------+-----------------------------------------------------+
    | Name    | Type    | Description                                         |
    |         |         |                                                     |
    +=========+=========+=====================================================+
    | id      | integer | Port ID of the ring port.                           |
    +---------+---------+-----------------------------------------------------+
    | rx      | integer | The total number of received packets.               |
    +---------+---------+-----------------------------------------------------+
    | rx_drop | integer | The total number of dropped packets of received.    |
    +---------+---------+-----------------------------------------------------+
    | tx      | integer | The total number of transferred packets.            |
    +---------+---------+-----------------------------------------------------+
    | tx_drop | integer | The total number of dropped packets of transferred. |
    +---------+---------+-----------------------------------------------------+


Response example
~~~~~~~~~~~~~~~~

.. code-block:: json

    {
      "lcores": [
        0
      ],
      "phy_ports": [
        {
          "id": 0,
          "rx": 0,
          "tx": 0,
          "tx_drop": 0,
          "eth": "56:48:4f:53:54:00"
        },
        {
          "id": 1,
          "rx": 0,
          "tx": 0,
          "tx_drop": 0,
          "eth": "56:48:4f:53:54:01"
        }
      ],
      "ring_ports": [
        {
          "id": 0,
          "rx": 0,
          "rx_drop": 0,
          "tx": 0,
          "tx_drop": 0
        },
        {
          "id": 1,
          "rx": 0,
          "rx_drop": 0,
          "tx": 0,
          "tx_drop": 0
        },
        ...
      ]
    }


DELETE /v1/primary/status
-------------------------

Clear statistical information.

* Normal response codes: 204


Request example
~~~~~~~~~~~~~~~

.. code-block:: console

    $ curl -X DELETE -H 'application/json' \
      http://127.0.0.1:7777/v1/primary/status


Response
~~~~~~~~

There is no body content for the response of a successful ``DELETE`` request.


DELETE /v1/primary
------------------

Terminate primary process.

* Normal response codes: 204


Request example
~~~~~~~~~~~~~~~

.. code-block:: console

    $ curl -X DELETE -H 'application/json' \
      http://127.0.0.1:7777/v1/primary


Response
~~~~~~~~

There is no body content for the response of a successful ``DELETE`` request.
