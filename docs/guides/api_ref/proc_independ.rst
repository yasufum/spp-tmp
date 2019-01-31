..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018-2019 Nippon Telegraph and Telephone Corporation


.. _spp_ctl_rest_api_proc_independ:

API Independent of Process Type
===============================

GET /v1/processes
-----------------

Show the SPP processes connected to the ``spp-ctl``.

* Normarl response codes: 200


Request example
~~~~~~~~~~~~~~~

.. code-block:: console

    $ curl -X GET -H 'application/json' \
    http://127.0.0.1:7777/v1/processes


Response
~~~~~~~~

An array of process objects.

Process objects:

.. _table_spp_ctl_processes:

.. table:: Response params of getting processes info.

    +-----------+---------+-----------------------------------------------------------------+
    | Name      | Type    | Description                                                     |
    |           |         |                                                                 |
    +===========+=========+=================================================================+
    | type      | string  | process type. one of ``primary``, ``nfv`` or ``vf``.            |
    +-----------+---------+-----------------------------------------------------------------+
    | client-id | integer | client id. if type is ``primary`` this member does not exist.   |
    +-----------+---------+-----------------------------------------------------------------+


Response example
~~~~~~~~~~~~~~~~

.. code-block:: json

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
      ...
    ]
