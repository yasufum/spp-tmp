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


launch
------

Launch secondary process.

Spp_primary is able to launch a secondary process with given type, secondary
ID and options of EAL and application itself. This is a list of supported type
of secondary processes.

  * nfv
  * vf
  * mirror
  * pcap

.. code-block:: console

    # spp_nfv with sec ID 1
    spp > pri; launch nfv 1 -l 1,2 -m 512 -- -n -s 192.168.1.100:6666

    # spp_vf with sec ID 2
    spp > pri; launch vf 2 -l 1,3-5 -m 512 -- --client-id -s 192.168.1.100:6666

You notice that ``--proc-type secondary`` is not given for launching secondary
processes. ``launch`` command adds this option before requesting to launch
the process so that you do not need to input this option by yourself.

``launch`` command supports TAB completion for type, secondary ID and the rest
of options. Some of EAL and application options are just a template, so you
should update them before launching.

In terms of log, each of secondary processes are output its log messages to
files under ``log`` directory of project root. The name of log file is defined
with type of process and secondary ID. For instance, ``nfv 2``, the path of log
file is ``log/spp_nfv-2.log``.
