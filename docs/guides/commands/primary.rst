..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation

Primary Commands
================

Primary process is managed with ``pri`` command.


status
------

Show status fo spp_primary and forwarding statistics of each of ports.

.. code-block:: console

    spp > pri; status
    - lcores:
      - [0]
    - physical ports:
        ID          rx          tx     tx_drop  mac_addr
         0           0           0           0  56:48:4f:53:54:00
         1           0           0           0  56:48:4f:53:54:01
    - ring Ports:
        ID          rx          tx     rx_drop     rx_drop
         0           0           0           0           0
         1           0           0           0           0


clear
-----

Clear statistics.

.. code-block:: console

    spp > pri; clear
