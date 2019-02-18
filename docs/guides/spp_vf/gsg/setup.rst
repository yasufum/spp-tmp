..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Nippon Telegraph and Telephone Corporation

.. _spp_vf_gsg_setup:

Setup
=====

Install DPDK
------------

Install DPDK in any directory. This is a simple instruction and please
refer
`Getting Started Guide for Linux
<http://dpdk.org/doc/guides/linux_gsg/index.html>`_
for details.

.. code-block:: console

    $ cd /path/to/any_dir
    $ git clone http://dpdk.org/git/dpdk
    $ cd dpdk
    $ git checkout [TAG_NAME(e.g. v17.05)]
    $ export RTE_SDK=`pwd`
    $ export RTE_TARGET=x86_64-native-linuxapp-gcc
    $ make T=x86_64-native-linuxapp-gcc install


Install SPP
-----------

Clone SPP in any directory and compile it.

.. code-block:: console

    $ cd /path/to/any_dir
    $ git clone http://dpdk.org/git/apps/spp
    $ cd spp
    $ make

Setup for DPDK
--------------

Load igb_uio module.

.. code-block:: console

    $ sudo modprobe uio
    $ sudo insmod $RTE_SDK/x86_64-native-linuxapp-gcc/kmod/igb_uio.ko
    $ lsmod | grep uio
    igb_uio                16384  0  # igb_uio is loaded
    uio                    20480  1 igb_uio

Then, bind your devices with PCI number by using ``dpdk-devbind.py``.
PCI number is inspected

.. code-block:: console

    # check your device for PCI_Number
    $ $RTE_SDK/usertools/dpdk-devbind.py --status

    $ sudo $RTE_SDK/usertools/dpdk-devbind.py --bind=igb_uio PCI_NUM


Setup spp_mirror
----------------

Setup of ``spp_mirror`` is almost the same as :ref:`SPP VF<spp_vf_gsg_setup>`.
Configuration of use of ``shallowcopy`` or ``deepcopy`` is different from
``spp_vf``.
It is defined in ``src/mirror/Makefile`` and which of copying is used is
configured by editing ``CFLAG`` option. It is defined to use ``shallowcopy``
by default.

If you use ``deepcopy``, comment out the line of ``-Dspp_mirror_SHALLOWCOPY``
to be disabled.

.. code-block:: c

   #CFLAGS += -Dspp_mirror_SHALLOWCOPY

Then, run make command to compile ``spp_mirror``.

.. code-block:: console

   $ make

Setup spp_pcap
--------------

Setup of ``spp_pcap`` is almost the same as :ref:`SPP VF<spp_vf_gsg_setup>`.
``libpcap-dev`` is  are used by ``spp_pcap`` when capturing and packet,
so you need to install ``libpcap-dev`` .
``liblz4-dev`` and ``liblz4-tool`` are used for compression and decompression
respectively, so you need to install ``liblz4-dev`` and ``liblz4-tool`` .

.. code-block:: console

   $ sudo apt install libpcap-dev
   $ sudo apt install liblz4-dev
   $ sudo apt install liblz4-tool
