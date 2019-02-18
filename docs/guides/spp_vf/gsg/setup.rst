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


.. _spp_vf_gsg_virsh_setup:

virsh setup
-----------

First of all, please check version of qemu-kvm.

.. code-block:: console

    $ qemu-system-x86_64 --version

If your system does not have qemu-kvm or the version of qemu is less than 2.7,
then please install qemu following
the instruction of https://wiki.qemu.org/index.php/Hosts/Linux
to install qemu 2.7.
You may need to install libvirt-bin,
virtinst, bridge-utils packages via ``apt-get`` install to run
``virt-install``.


``virsh`` is a command line interface that can be used to create, destroy,
stop start and edit VMs and configure. After create an image file,
you can setup it with ``virt-install``.
``--location`` is a URL of installer and it should be
``http://archive.ubuntu.com/ubuntu/dists/xenial/main/installer-amd64/``
for amd64.

.. code-block:: console

   virt-install \
   --name [VM_NAME] \
   --ram 4096 \
   --disk path=/var/lib/libvirt/images/[VM_NAME].img,size=30 \
   --vcpus 4 \
   --os-type linux \
   --os-variant ubuntu16.04 \
   --network network=default \
   --graphics none \
   --console pty,target_type=serial \
   --location '[LOCATION]' \
   --extra-args 'console=ttyS0,115200n8 serial'

You may need type the following commands through ssh to activate console.

.. code-block:: console

    $sudo systemctl enable serial-getty@ttyS0.service
    $sudo systemctl start serial-getty@ttyS0.service


Edit VM configuration with virsh.

.. code-block:: console

    $ virsh edit [VM_NAME]

You need to add ``xmlns:qemu='http://libvirt.org/schemas/domain/qemu/1.0'``
into the domain tag because of adding ``<qemu:commandline>`` tag.
In addition, you need to add the tag enclosed by ``<memoryBacking>`` and
``</memoryBacking>``, ``<qemu:commandline>`` and ``</qemu:commandline>``
because SPP uses vhost-user as interface with VM.
Note that number used in those tags should be the same value
(e.g. chr0,sock0,vhost-net0) and these values should correspond
to "add vhost N" (in this example 0).
MAC address used in
``<qemu:arg value='virtio-net-pci,netdev=vhost-net0,mac=52:54:00:12:34:56'/>``
can be specified when registering MAC address to classifier
using Secondary command.

        The following is an example of modified xml file:

.. code-block:: xml

    <domain type='kvm' xmlns:qemu='http://libvirt.org/schemas/domain/qemu/1.0'>
      <name>spp-vm1</name>
      <uuid>d90f5420-861a-4479-8559-62d7a1545cb9</uuid>
      <memory unit='KiB'>4194304</memory>
      <currentMemory unit='KiB'>4194304</currentMemory>
      <memoryBacking>
        <hugepages/>
      </memoryBacking>
      <vcpu placement='static'>4</vcpu>
      <os>
        <type arch='x86_64' machine='pc-i440fx-2.3'>hvm</type>
        <boot dev='hd'/>
      </os>
      <features>
        <acpi/>
        <apic/>
        <pae/>
      </features>
      <clock offset='utc'/>
      <on_poweroff>destroy</on_poweroff>
      <on_reboot>restart</on_reboot>
      <on_crash>restart</on_crash>
      <devices>
        <emulator>/usr/local/bin/qemu-system-x86_64</emulator>
        <disk type='file' device='disk'>
          <driver name='qemu' type='raw'/>
          <source file='/var/lib/libvirt/images/spp-vm1.qcow2'/>
          <target dev='hda' bus='ide'/>
          <address type='drive' controller='0' bus='0' target='0' unit='0'/>
        </disk>
        <disk type='block' device='cdrom'>
          <driver name='qemu' type='raw'/>
          <target dev='hdc' bus='ide'/>
          <readonly/>
          <address type='drive' controller='0' bus='1' target='0' unit='0'/>
        </disk>
        <controller type='usb' index='0'>
          <address type='pci' domain='0x0000' bus='0x00' slot='0x01'
          function='0x2'/>
        </controller>
        <controller type='pci' index='0' model='pci-root'/>
        <controller type='ide' index='0'>
          <address type='pci' domain='0x0000' bus='0x00' slot='0x01'
          function='0x1'/>
        </controller>
        <interface type='network'>
          <mac address='52:54:00:99:aa:7f'/>
          <source network='default'/>
          <model type='rtl8139'/>
          <address type='pci' domain='0x0000' bus='0x00' slot='0x02'
          function='0x0'/>
        </interface>
        <serial type='pty'>
          <target type='isa-serial' port='0'/>
        </serial>
        <console type='pty'>
          <target type='serial' port='0'/>
        </console>
        <memballoon model='virtio'>
          <address type='pci' domain='0x0000' bus='0x00' slot='0x03'
          function='0x0'/>
        </memballoon>
      </devices>
      <qemu:commandline>
        <qemu:arg value='-cpu'/>
        <qemu:arg value='host'/>
        <qemu:arg value='-object'/>
        <qemu:arg
        value='memory-backend-file,id=mem,size=4096M,mem-path=/run/hugepages/kvm,share=on'/>
        <qemu:arg value='-numa'/>
        <qemu:arg value='node,memdev=mem'/>
        <qemu:arg value='-mem-prealloc'/>
        <qemu:arg value='-chardev'/>
        <qemu:arg value='socket,id=chr0,path=/tmp/sock0,server'/>
        <qemu:arg value='-device'/>
        <qemu:arg
        value='virtio-net-pci,netdev=vhost-net0,mac=52:54:00:12:34:56'/>
        <qemu:arg value='-netdev'/>
        <qemu:arg value='vhost-user,id=vhost-net0,chardev=chr0,vhostforce'/>
        <qemu:arg value='-chardev'/>
        <qemu:arg value='socket,id=chr1,path=/tmp/sock1,server'/>
        <qemu:arg value='-device'/>
        <qemu:arg
        value='virtio-net-pci,netdev=vhost-net1,mac=52:54:00:12:34:57'/>
        <qemu:arg value='-netdev'/>
        <qemu:arg value='vhost-user,id=vhost-net1,chardev=chr1,vhostforce'/>
      </qemu:commandline>
    </domain>


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
