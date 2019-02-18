..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation
    Copyright(c) 2017-2019 Nippon Telegraph and Telephone Corporation


.. _gsg_setup:

Setup
=====

This documentation is described for Ubuntu 16.04 and later.


.. _gsg_reserve_hugep:

Reserving Hugepages
-------------------

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
Here is an example. ``hugepagesz`` is for the size and ``hugepages``
is for the number of pages.

You can also configure isolcpus for performance tuning as described in
:ref:`Performance Optimizing<gsg_performance_opt>`.

.. code-block:: console

    # /etc/default/grub
    GRUB_CMDLINE_LINUX="default_hugepagesz=1G hugepagesz=1G hugepages=8"

.. note::

    1GB hugepages might not be supported in your machine. It depends on
    that CPUs support 1GB pages or not. You can check it by referring
    ``/proc/cpuinfo``. If it is supported, you can find ``pdpe1gb`` in
    the ``flags`` attribute.

    .. code-block:: console

        $ cat /proc/cpuinfo | grep pdpe1gb
        flags           : fpu vme ... pdpe1gb ...

You should run ``update-grub`` after editing to update grub's config file,
or this configuration is not activated.

.. code-block:: console

   $ sudo update-grub
   Generating grub configuration file ...

For 2MB page, you can activate hugepages while booting or at anytime
after system is booted.
Define hugepages setting in ``/etc/default/grub`` to activate it while
booting, or overwrite the number of 2MB hugepages as following.

.. code-block:: console

    $ echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

In this case, 1024 pages of 2MB (totally 2048 MB) are reserved.


Mount hugepages
---------------

Make the memory available for using hugepages from DPDK.

.. code-block:: console

    $ mkdir /mnt/huge
    $ mount -t hugetlbfs nodev /mnt/huge

It is also available while booting by adding a configuration of mount
point in ``/etc/fstab``, or after booted.

The mount point for 2MB or 1GB can be made permanent accross reboot.
For 2MB, it is no need to declare the size of hugepages explicity.

.. code-block:: console

    # /etc/fstab
    nodev /mnt/huge hugetlbfs defaults 0 0

For 1GB, the size of hugepage must be specified.

.. code-block:: console

    # /etc/fstab
    nodev /mnt/huge_1GB hugetlbfs pagesize=1GB 0 0


Disable ASLR
------------

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

ASLR can be disabled by assigning ``kernel.randomize_va_space`` to
``0``, or be enabled by assigning it to ``2``.

.. code-block:: console

    # disable ASLR
    $ sudo sysctl -w kernel.randomize_va_space=0

    # enable ASLR
    $ sudo sysctl -w kernel.randomize_va_space=2

You can check the value as following.

.. code-block:: console

    $ sysctl -n kernel.randomize_va_space


Vhost Client Mode
-----------------

SPP secondary process supports ``--vhost-client`` options for using vhost port.
In vhost client mode, qemu creates socket file instead of secondary process.
It means that you can launch a VM before secondary process create vhost port.

.. note::

    Vhost client mode is supported by qemu 2.7 or later.


Using Libvirt
-------------

If you use libvirt for managing virtual machines, you might need some
additional configurations.

Uncomment user and group in ``/etc/libvirt/qemu.conf``.

.. code-block:: console

    # /etc/libvirt/qemu.conf

    user = "root"
    group = "root"

To use hugepages with libvirt, change ``KVM_HUGEPAGES`` from 0 to 1
in ``/etc/default/qemu-kvm``.

.. code-block:: console

    # /etc/default/qemu-kvm

    KVM_HUGEPAGES=1

Change grub config as similar to
:ref:`Reserving Hugepages<gsg_reserve_hugep>`.
You can check hugepage settings as following.

.. code-block:: console

    $ cat /proc/meminfo | grep -i huge
    AnonHugePages:      2048 kB
    HugePages_Total:      36		#	/etc/default/grub
    HugePages_Free:       36
    HugePages_Rsvd:        0
    HugePages_Surp:        0
    Hugepagesize:    1048576 kB		#	/etc/default/grub

    $ mount | grep -i huge
    cgroup on /sys/fs/cgroup/hugetlb type cgroup (rw,...,nsroot=/)
    hugetlbfs on /dev/hugepages type hugetlbfs (rw,relatime)
    hugetlbfs-kvm on /run/hugepages/kvm type hugetlbfs (rw,...,gid=117)
    hugetlb on /run/lxcfs/controllers/hugetlb type cgroup (rw,...,nsroot=/)

Finally, you umount default hugepages.

.. code-block:: console

    $ sudo umount /dev/hugepages


Trouble Shooting
~~~~~~~~~~~~~~~~

You might encounter a permission error while creating a resource,
such as a socket file under ``tmp/``, because of AppArmor.

You can avoid this error by editing ``/etc/libvirt/qemu.conf``.

.. code-block:: console

    # Set security_driver to "none"
    $sudo vi /etc/libvirt/qemu.conf
    ...
    security_driver = "none"
    ...

Restart libvirtd to activate this configuration.

.. code-block:: console

    $sudo systemctl restart libvirtd.service

Or, you can also avoid by simply removing AppArmor itself.

.. code-block:: console

    $ sudo apt-get remove apparmor

If you use CentOS, not Ubuntu, confirm that SELinux doesn't prevent
for permission.
SELinux should be disabled in this case.

.. code-block:: console

    # /etc/selinux/config
    SELINUX=disabled

Check your SELinux configuration.

.. code-block:: console

    $ getenforce
    Disabled


Python 2 or 3 ?
---------------

In SPP, Python3 is required only for running ``spp-ctl``. Other python scripts
are able to be launched both of Python2 and 3.

Howevrer, Python2 will not be maintained after 2020 and SPP is going to update
only supporting Python3.
In SPP, it is planned to support only Python3 before the end of 2019.


Reference
---------

* [1] `Use of Hugepages in the Linux Environment
  <http://dpdk.org/doc/guides/linux_gsg/sys_reqs.html#running-dpdk-applications>`_

* [2] `Using Linux Core Isolation to Reduce Context Switches
  <http://dpdk.org/doc/guides/linux_gsg/enable_func.html#using-linux-core-isolation-to-reduce-context-switches>`_

* [3] `Linux boot command line
  <http://dpdk.org/doc/guides/linux_gsg/nic_perf_intel_platform.html#linux-boot-command-line>`_
