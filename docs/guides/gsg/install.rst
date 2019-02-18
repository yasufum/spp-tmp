..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017-2019 Nippon Telegraph and Telephone Corporation


.. _setup_install_dpdk_spp:

Install DPDK and SPP
====================

Before using SPP, you need to install DPDK.
In this document, briefly describ how to install and setup DPDK.
Refer to `DPDK documentation
<https://dpdk.org/doc/guides/>`_ for more details.
For Linux, see `Getting Started Guide for Linux
<http://www.dpdk.org/doc/guides/linux_gsg/index.html>`_ .

DPDK
----

Clone repository and compile DPDK in any directory.

.. code-block:: console

    $ cd /path/to/any
    $ git clone http://dpdk.org/git/dpdk

To compile DPDK, required to install libnuma-devel library.

.. code-block:: console

    $ sudo apt install libnuma-dev

Python and pip are also required if not installed.

.. code-block:: console

    # Python2
    $ sudo apt install python python-pip

    # Python3
    $ sudo apt install python3 python3-pip

Some of secondary processes depend on external libraries and you failed to
compile SPP without them.

SPP provides libpcap-based PMD for dumping packet to a file or retrieve
it from the file.
``spp_nfv`` and ``spp_pcap`` use ``libpcap-dev`` for packet capture.
``spp_pcap`` uses ``liblz4-dev`` and ``liblz4-tool`` to compress PCAP file.

.. code-block:: console

   $ sudo apt install libpcap-dev
   $ sudo apt install liblz4-dev
   $ sudo apt install liblz4-tool

``text2pcap`` is also required for creating pcap file which
is included in ``wireshark``.

.. code-block:: console

    $ sudo apt install wireshark

PCAP is disabled by default in DPDK configuration.
``CONFIG_RTE_LIBRTE_PMD_PCAP`` and ``CONFIG_RTE_PORT_PCAP`` define the
configuration and enabled it to ``y``.

.. code-block:: console

    # dpdk/config/common_base
    CONFIG_RTE_LIBRTE_PMD_PCAP=y
    ...
    CONFIG_RTE_PORT_PCAP=y

Compile DPDK with target environment.

.. code-block:: console

    $ cd dpdk
    $ export RTE_SDK=$(pwd)
    $ export RTE_TARGET=x86_64-native-linuxapp-gcc  # depends on your env
    $ make install T=$RTE_TARGET


SPP
---

Clone repository and compile SPP in any directory.

.. code-block:: console

    $ cd /path/to/any
    $ git clone http://dpdk.org/git/apps/spp
    $ cd spp
    $ make  # Confirm that $RTE_SDK and $RTE_TARGET are set

It also required to install Python3 and packages for running python scripts
as following.
You might need to run ``pip3`` with ``sudo`` if it is failed.

.. code-block:: console

    $ sudo apt update
    $ sudo apt install python3
    $ sudo apt install python3-pip
    $ pip3 install -r requirements.txt


Binding Network Ports to DPDK
-----------------------------

Network ports must be bound to DPDK with a UIO (Userspace IO) driver.
UIO driver is for mapping device memory to userspace and registering
interrupts.

UIO Drivers
~~~~~~~~~~~

You usually use the standard ``uio_pci_generic`` for many use cases
or ``vfio-pci`` for more robust and secure cases.
Both of drivers are included by default in modern Linux kernel.

.. code-block:: console

    # Activate uio_pci_generic
    $ sudo modprobe uio_pci_generic

    # or vfio-pci
    $ sudo modprobe vfio-pci

You can also use kmod included in DPDK instead of ``uio_pci_generic``
or ``vfio-pci``.

.. code-block:: console

    $ sudo modprobe uio
    $ sudo insmod kmod/igb_uio.ko

Binding Network Ports
~~~~~~~~~~~~~~~~~~~~~

Once UIO driver is activated, bind network ports with the driver.
DPDK provides ``usertools/dpdk-devbind.py`` for managing devices.

Find ports for binding to DPDK by running the tool with ``-s`` option.

.. code-block:: console

    $ $RTE_SDK/usertools/dpdk-devbind.py --status

    Network devices using DPDK-compatible driver
    ============================================
    <none>

    Network devices using kernel driver
    ===================================
    0000:29:00.0 '82571EB ... 10bc' if=enp41s0f0 drv=e1000e unused=
    0000:29:00.1 '82571EB ... 10bc' if=enp41s0f1 drv=e1000e unused=
    0000:2a:00.0 '82571EB ... 10bc' if=enp42s0f0 drv=e1000e unused=
    0000:2a:00.1 '82571EB ... 10bc' if=enp42s0f1 drv=e1000e unused=

    Other Network devices
    =====================
    <none>
    ....

You can find network ports are bound to kernel driver and not to DPDK.
To bind a port to DPDK, run ``dpdk-devbind.py`` with specifying a driver
and a device ID.
Device ID is a PCI address of the device or more friendly style like
``eth0`` found by ``ifconfig`` or ``ip`` command..

.. code-block:: console

    # Bind a port with 2a:00.0 (PCI address)
    ./usertools/dpdk-devbind.py --bind=uio_pci_generic 2a:00.0

    # or eth0
    ./usertools/dpdk-devbind.py --bind=uio_pci_generic eth0


After binding two ports, you can find it is under the DPDK driver and
cannot find it by using ``ifconfig`` or ``ip``.

.. code-block:: console

    $ $RTE_SDK/usertools/dpdk-devbind.py -s

    Network devices using DPDK-compatible driver
    ============================================
    0000:2a:00.0 '82571EB ... 10bc' drv=uio_pci_generic unused=vfio-pci
    0000:2a:00.1 '82571EB ... 10bc' drv=uio_pci_generic unused=vfio-pci

    Network devices using kernel driver
    ===================================
    0000:29:00.0 '...' if=enp41s0f0 drv=e1000e unused=vfio-pci,uio_pci_generic
    0000:29:00.1 '...' if=enp41s0f1 drv=e1000e unused=vfio-pci,uio_pci_generic

    Other Network devices
    =====================
    <none>
    ....


Confirm DPDK is setup properly
------------------------------

You can confirm if you are ready to use DPDK by running DPDK's sample
application. ``l2fwd`` is good choice to confirm it before SPP because
it is very similar to SPP's worker process for forwarding.

.. code-block:: console

   $ cd $RTE_SDK/examples/l2fwd
   $ make
     CC main.o
     LD l2fwd
     INSTALL-APP l2fwd
     INSTALL-MAP l2fwd.map

In this case, run this application simply with just two options
while DPDK has many kinds of options.

  - -l: core list
  - -p: port mask

.. code-block:: console

   $ sudo ./build/app/l2fwd \
     -l 1-2 \
     -- -p 0x3

It must be separated with ``--`` to specify which option is
for EAL or application.
Refer to `L2 Forwarding Sample Application
<https://dpdk.org/doc/guides/sample_app_ug/l2_forward_real_virtual.html>`_
for more details.


Build Documentation
-------------------

This documentation is able to be biult as HTML and PDF formats from make
command. Before compiling the documentation, you need to install some of
packages required to compile.

For HTML documentation, install sphinx and additional theme.

.. code-block:: console

    $ pip install sphinx
    $ pip install sphinx-rtd-theme

For PDF, inkscape and latex packages are required.

.. code-block:: console

    $ sudo apt install inkscape
    $ sudo apt install texlive-latex-extra
    $ sudo apt install texlive-latex-recommended

You might also need to install ``latexmk`` in addition to if you use
Ubuntu 18.04 LTS.

.. code-block:: console

    $ sudo apt install latexmk

HTML documentation is compiled by running make with ``doc-html``. This
command launch sphinx for compiling HTML documents.
Compiled HTML files are created in ``docs/guides/_build/html/`` and
You can find the top page ``index.html`` in the directory.

.. code-block:: console

    $ make doc-html

PDF documentation is compiled with ``doc-pdf`` which runs latex for.
Compiled PDF file is created as ``docs/guides/_build/html/SoftPatchPanel.pdf``.

.. code-block:: console

    $ make doc-pdf

You can also compile both of HTML and PDF documentations with ``doc`` or
``doc-all``.

.. code-block:: console

    $ make doc
    # or
    $ make doc-all
