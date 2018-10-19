..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation

Primary Commands
====================

Primary process is managed with ``pri`` command.


status
------

Show status of primary.

.. code-block:: console

    spp > pri status
    recv:('127.0.0.1', 50524):{Server Running}


exit
----

Terminate primary.

.. code-block:: console

    spp > pri exit
    closing:('127.0.0.1', 50524)

.. note::

    You should not use this command because terminating primary before
    secondaries may cause an error.
    You shold use ``bye`` command instead of ``pri exit``.

clear
-----

Clear statistics.

.. note::

    This command is not supported currently.

.. code-block:: console

    spp > pri clear
    recv:('127.0.0.1', 50524):{clear stats}
