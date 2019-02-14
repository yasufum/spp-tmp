..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Nippon Telegraph and Telephone Corporation

.. _spp_vf_gsg_howto_use:

How to Use
==========

SPP Controller
--------------

Go to the SPP's directory first.

.. code-block:: console

    $ cd /path/to/spp

Launch ``spp-ctl`` before launching SPP primary and secondary processes.
You also need to launch ``spp.py``  if you use ``spp_vf`` from CLI.
``-b`` option is for binding IP address to communicate other SPP processes,
but no need to give it explicitly if ``127.0.0.1`` or ``localhost`` although
doing explicitly in this example to be more understandable.

.. code-block:: console

    # Launch spp-ctl and spp.py
    $ python3 ./src/spp-ctl/spp-ctl -b 192.168.1.100
    $ python ./src/spp.py -b 192.168.1.100


SPP Primary
-----------

SPP primary allocates and manages resources for secondary processes.
You need to run SPP primary before secondary processes.

SPP primary has two kinds of options for DPDK and spp.
Before ``--`` are for DPDK is, and after it are for spp.

See `Running a Sample Application
<http://dpdk.org/doc/guides/linux_gsg/build_sample_apps.html#running-a-sample-application>`_
in DPDK documentation for options.

Application specific options of spp primary.

  * ``-p``: Port mask.
  * ``-n``: Number of rings.
  * ``-s``: IPv4 address and port for spp primary.

This is an example of launching ``spp_primary``.

.. code-block:: console

    $ sudo ./src/primary/x86_64-native-linuxapp-gcc/spp_primary \
      -l 0 -n 4 --socket-mem 512,512 \
      --huge-dir /run/hugepages/kvm \
      --proc-type primary \
      -- \
      -p 0x03 -n 10 \
      -s 192.168.1.100:5555


.. _spp_vf_gsg_howto_use_spp_vf:

spp_vf
------

``spp_vf`` is a kind of secondary process, so it takes both of EAL options and
application specific options. Here is a list of application specific options.

  * ``--client-id``: Client ID unique among secondary processes.
  * ``-s``: IPv4 address and secondary port of spp-ctl.
  * ``--vhost-client``: Enable vhost-user client mode.

This is an example of launching ``spp_vf``.

.. code-block:: console

    $ sudo ./src/vf/x86_64-native-linuxapp-gcc/spp_vf \
      -l 0,2-13 -n 4 \
      --proc-type=secondary \
      -- \
      --client-id 1 \
      -s 192.168.1.100:6666 \
      --vhost-client

If ``--vhost-client`` option is specified, then ``vhost-user`` act as
the client, otherwise the server.
For reconnect feature from SPP to VM, ``--vhost-client`` option can be
used. This reconnect features requires QEMU 2.7 (or later).
See also `Vhost Sample Application
<http://dpdk.org/doc/guides/sample_app_ug/vhost.html>`_.


.. _spp_vf_gsg_howto_use_spp_mirror:

spp_mirror
----------

``spp_mirror`` is a kind of secondary process, and options are same as
``spp_vf``.

.. code-block:: console

    $ sudo ./src/mirror/x86_64-native-linuxapp-gcc/spp_mirror \
      -l 1,2 -n 4 \
      --proc-type=secondary \
      -- \
      --client-id 1 \
      -s 192.168.1.100:6666 \
      -vhost-client


.. _spp_vf_gsg_howto_use_spp_pcap:

spp_pcap
--------

``spp_pcap`` is a kind of secondary process, so it takes both of EAL options
and application specific options.

.. code-block:: console

    $ sudo ./src/pcap/x86_64-native-linuxapp-gcc/spp_pcap \
      -l 0-3 -n 4 \
      --proc-type=secondary \
      -- \
      --client-id 1 \
      -s 192.168.1.100:6666 \
      -c phy:0 \
      --out-dir /path/to/dir \
      --fsize 107374182

Here is a list of ``spp_pcap`` specific options.

 * ``-c``: Captured port, e.g. ``phy:0``, ``ring:1`` or so.
 * ``--out-dir``: Optional. Path of dir for captured file. Default is ``/tmp``.
 * ``--fsize``: Optional. Maximum size of a capture file. Default is ``1GiB``.

Captured file of LZ4 is generated in ``/tmp`` by default.
The name of file is consists of timestamp, resource ID of captured port,
ID of ``writer`` threads and sequential number.
Timestamp is decided when capturing is started and formatted as
``YYYYMMDDhhmmss``.
Both of ``writer`` thread ID and sequential number are started from ``1``.
Sequential number is required for the case if the size of
captured file is reached to the maximum and another file is generated to
continue capturing.

This is an example of captured file. It consists of timestamp,
``20190214154925``, port ``phy0``, thread ID ``1`` and sequential number
``1``.

.. code-block:: none

    /tmp/spp_pcap.20190214154925.phy0.1.1.pcap.lz4

``spp_pcap`` also generates temporary files which are owned by each of
``writer`` threads until capturing is finished or the size of captured file
is reached to the maximum.
This temporary file has additional extension ``tmp`` at the end of file
name.

.. code-block:: none

    /tmp/spp_pcap.20190214154925.phy0.1.1.pcap.lz4.tmp


Using VM with virsh
-------------------

In this section, VM is launched with ``virsh`` command.

.. code-block:: console

    $ virsh start [VM]

It is required to add network configuration for processes running on the VMs.
If this configuration is skipped, processes cannot communicate with others
via SPP.

On the VMs, add an interface and disable offload.

.. code-block:: console

    # Add interface
    $ sudo ifconfig [IF_NAME] inet [IP_ADDR] netmask [NETMASK] up

    # Disable offload
    $ sudo ethtool -K [IF_NAME] tx off

On host machine, it is also required to disable offload.

.. code-block:: console

    # Disable offload for VM
    $ sudo ethtool -K [IF_NAME] tx off
