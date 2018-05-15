.. _getting_started:

..  BSD LICENSE
    Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


Getting Started
===============

Setup
-----

Reserving Hugepages
~~~~~~~~~~~~~~~~~~~

Hugepages must be enabled for running DPDK with high performance.
Hugepage support is required to reserve large amount size of pages,
2MB or 1GB per page, to less TLB (Translation Lookaside Buffers) and
to reduce cache miss.
Less TLB means that it reduce the time for translating virtual address
to physical.

Hugepage reservation might be different for 2MB or 1GB.

For 1GB page, hugepage setting must be activated while booting system.
It must be defined in boot loader configuration, usually is
``/etc/default/grub``.
Add an entry to define pagesize and the number of pages.
Here is an example. `` hugepagesz`` is for the size and ``hugepages``
is for the number of pages.

.. code-block:: console

    GRUB_CMDLINE_LINUX="default_hugepagesz=1G hugepagesz=1G hugepages=8"

.. note::

    1GB hugepages might not be supported in your machine. It depends on
    that CPUs support 1GB pages or not. You can check it by referring
    ``/proc/cpuinfo``. If it is supported, you can find ``pdpe1gb`` in
    the ``flags`` attribute.

    .. code-block:: console

        $ cat /proc/cpuinfo | pdpe1gb

For 2MB page, you can activate hugepages while booting or at anytime
after system is booted.
Define hugepages setting in ``/etc/default/grub`` to activate it while
booting, or overwrite the number of 2MB hugepages as following.

.. code-block:: console

    $ echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

In this case, 1024 pages of 2MB (totally 2048 MB) are reserved.


Mount hugepages
~~~~~~~~~~~~~~~

Make the memory available for using hugepages from DPDK.

.. code-block:: console

    mkdir /mnt/huge
    mount -t hugetlbfs nodev /mnt/huge

It is also available while booting by adding a configuration of mount
point in ``/etc/fstab``, or after booted.

The mount point for 2MB or 1GB can be made permanent accross reboot.
For 2MB, it is no need to declare the size of hugepages explicity.

.. code-block:: console

    nodev /mnt/huge hugetlbfs defaults 0 0

For 1GB, the size of hugepage must be specified.

.. code-block:: console

    nodev /mnt/huge_1GB hugetlbfs pagesize=1GB 0 0


Disable ASLR
~~~~~~~~~~~~

SPP is a DPDK multi-process application and there are a number of
`limitations
<https://dpdk.org/doc/guides/prog_guide/multi_proc_support.html#multi-process-limitations>`_
.

Address-Space Layout Randomization (ASLR) is a security feature for
memory protection, but may cause a failure of memory
mapping while starting multi-process application as discussed in
`dpdk-dev
<http://dpdk.org/ml/archives/dev/2014-September/005236.html>`_
.

ASLR can be disabled by assigning `kernel.randomize_va_space` to `0`,
or be enabled by assigning to `2`.

.. code-block:: console

    # disable ASLR
    $ sudo sysctl -w kernel.randomize_va_space=0


Install DPDK and SPP
--------------------

Before using SPP, you need to install DPDK.
In this document, briefly describ how to install and setup DPDK.
Refer to `DPDK documentation
<https://dpdk.org/doc/guides/>`_ for more details.
For Linux, see `Getting Started Guide for Linux
<http://www.dpdk.org/doc/guides/linux_gsg/index.html>`_ .

DPDK
~~~~

First, download and compile DPDK in any directory.
Compiling DPDK takes a few minutes.

.. code-block:: console

    $ cd /path/to/any
    $ git clone http://dpdk.org/git/dpdk
    $ cd dpdk
    $ export RTE_SDK=$(pwd)
    $ export RTE_TARGET=x86_64-native-linuxapp-gcc  # depends on your env
    $ make install T=$RTE_TARGET

SPP
~~~

Then, download and compile SPP in any directory.

.. code-block:: console

    $ cd /path/to/any
    $ git clone http://dpdk.org/git/apps/spp
    $ cd spp
    $ make  # Confirm that $RTE_SDK and $RTE_TARGET are set


Python 2 or 3 ?
~~~~~~~~~~~~~~~

You need to install Python for using usertools of DPDK or SPP controller.
DPDK and SPP support both of Python2 and 3.


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

    sudo modprobe uio
    sudo insmod kmod/igb_uio.ko

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
    0000:29:00.0 '82571EB Gigabit Ethernet Controller (Copper) 10bc' if=enp41s0f0 drv=e1000e unused=
    0000:29:00.1 '82571EB Gigabit Ethernet Controller (Copper) 10bc' if=enp41s0f1 drv=e1000e unused=
    0000:2a:00.0 '82571EB Gigabit Ethernet Controller (Copper) 10bc' if=enp42s0f0 drv=e1000e unused=
    0000:2a:00.1 '82571EB Gigabit Ethernet Controller (Copper) 10bc' if=enp42s0f1 drv=e1000e unused=

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
    0000:2a:00.0 '82571EB Gigabit Ethernet Controller (Copper) 10bc' drv=uio_pci_generic unused=vfio-pci
    0000:2a:00.1 '82571EB Gigabit Ethernet Controller (Copper) 10bc' drv=uio_pci_generic unused=vfio-pci

    Network devices using kernel driver
    ===================================
    0000:29:00.0 '82571EB Gigabit Ethernet Controller (Copper) 10bc' if=enp41s0f0 drv=e1000e unused=vfio-pci,uio_pci_generic
    0000:29:00.1 '82571EB Gigabit Ethernet Controller (Copper) 10bc' if=enp41s0f1 drv=e1000e unused=vfio-pci,uio_pci_generic

    Other Network devices
    =====================
    <none>
    ....


Confirm DPDK is setup properly
------------------------------

You had better to run DPDK sample application before SPP
as checking DPDK is setup properly.

Try ``l2fwd`` as an example.

.. code-block:: console

   $ cd $RTE_SDK/examples/l2fwd
   $ make
     CC main.o
     LD l2fwd
     INSTALL-APP l2fwd
     INSTALL-MAP l2fwd.map

In this case, run this application with two options.

  - -c: core mask
  - -p: port mask

.. code-block:: console

   $ sudo ./build/app/l2fwd \
     -c 0x03 \
     -- -p 0x3

It must be separated with ``--`` to specify which option is
for EAL or application.
Refer to `L2 Forwarding Sample Application
<https://dpdk.org/doc/guides/sample_app_ug/l2_forward_real_virtual.html>`_
for more details.
