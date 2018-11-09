..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation

Primary Commands
====================

Primary process is managed with ``pri`` command.


status
------

Show forwarding statistics of each of ports.

.. code-block:: console

    spp > pri; status
    Physical Ports:
      ID          rx          tx     tx_drop  mac_addr
       0    78932932    78932931           1  56:48:4f:53:54:00
    Ring Ports:
      ID          rx          tx     rx_drop     tx_drop
       0       89283       89283           0           0
       1        9208        9203           0           5
       ...

clear
-----

Clear statistics.

.. code-block:: console

    spp > pri; clear
