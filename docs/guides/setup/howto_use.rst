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


How to Use
==========

SPP consists of primary process for managing resources,
secondary processes for forwarding packet,
and SPP controller to accept user commands and sent it to SPP processes.

You must keep in mind the order of launching processes.
Primary process must be launched before secondary.
In addition, controller need to be launched before primary and secondary
because it prepares TCP connections for communicating primary and secondary.

1. SPP Controller
2. SPP Primary
3. SPP Secondary


SPP Controller
----------------

SPP controller is implemented as a python script ``spp.py``.

.. code-block:: console

    $ cd /path/to/spp
    $ python src/spp.py
    primary port : 5555
    secondary port : 6666
    Welcome to the spp.   Type help or ? to list commands.

    spp >

Controller communicate with primary via TCP port 5555 and with secondary
processes via 6666 in defalt.
You can change port number by using options.
Please refer help message for options.

.. code-block:: console

    $ python src/spp.py -h
    usage: spp.py [-h] [-p PRI_PORT] [-s SEC_PORT] [-m MNG_PORT] [-ip IPADDR]

    SPP Controller

    optional arguments:
      -h, --help            show this help message and exit
      -p PRI_PORT, --pri-port PRI_PORT
                            primary port number
      -s SEC_PORT, --sec-port SEC_PORT
                            secondary port number
      -m MNG_PORT, --mng-port MNG_PORT
                            management port number
      -ip IPADDR, --ipaddr IPADDR
                            IP address

:doc:`../../commands/index` describes
how to manage SPP processes from SPP controller.


SPP Primary
-----------

SPP primary is a resource manager and initializing EAL
for secondary processes.

To launch primary, run ``spp_primary`` with options.

.. code-block:: console

    $ sudo ./src/primary/x86_64-native-linuxapp-gcc/spp_primary \
        -l 1 -n 4 \
        --socket-mem 512,512 \
        --huge-dir=/dev/hugepages \
        --proc-type=primary \
        -- \
        -p 0x03 \
        -n 10 \
        -s 192.168.122.1:5555

SPP primary is a DPDK application and it takes EAL options before
application specific options.
Briefly describe about supported options.
You can use ``-m`` instead of ``--socket-mem`` if you use single NUMA
node.

- EAL options:

  - -l: core list (two cores required for displaying status)
  - --socket-mem: memory size on each of NUMA nodes
  - --huge-dir: path of hugepage dir
  - --proc-type: process type

- Application options:

  - -p: port mask
  - -n: number of ring PMD
  - -s: IP address of controller and port prepared for primary

.. note::

    You do not need to give two cores if you are not interested in
    statistics.
    SPP primary is able to run with only one core and use second one
    to show the statistics.


SPP Secondary
-------------

Secondary process behaves as a client of primary process and a worker
for doing tasks.

This section describes about ``spp_nfv`` and ``spp_vm``,
which just simply forward packets similar to ``l2fwd``.
The difference between them is running on host or VM.
``spp_vm`` runs inside a VM as described in name.


Launch on Host
~~~~~~~~~~~~~~

Run ``spp_nfv`` with options.

.. code-block:: console

    $ cd /path/to/spp
    $ sudo ./src/nfv/x86_64-native-linuxapp-gcc/spp_nfv \
        -l 2-3 -n 4 \
        --proc-type=secondary \
        -- \
        -n 1 \
        -s 192.168.122.1:6666

- EAL options:

  - -l: core list (two cores required)
  - --proc-type: process type

- Application options:

  - -n: secondary ID
  - -s: IP address of controller and port prepared for secondary

Secondary ID is used to identify for sending messages and must be
unique among all of secondaries.
If you attempt to launch a secondary process with the same ID,
SPP controller does not accept it and assign unused number.


Launch on VM
~~~~~~~~~~~~

To communicate DPDK application running on a VM,
it is required to create a virtual device for the VM.
In this instruction, launch a VM with qemu command and
create ``vhost-user`` and ``virtio-net-pci`` devices on the VM.

Before launching VM, you need to prepare a socket file for creating
``vhost-user`` device.
Socket file is created from SPP secondary as following.

.. code-block:: console

    spp > sec 1;add vhost 0

In this example, create socket file with index 0 from secondary of ID 1.
Socket file is created as ``/tmp/sock0``.
It is used as a qemu option to add vhost interface.

Launch VM with ``qemu-system-x86_64`` for x86 64bit architecture.
Qemu takes many options for defining resources including virtual
devices.

.. code-block:: console

    $ sudo qemu-system-x86_64 \
        -cpu host \
        -enable-kvm \
        -numa node,memdev=mem \
        -mem-prealloc \
        -hda /path/to/image.qcow2 \
        -m 4096 \
        -smp cores=4,threads=1,sockets=1 \
        -object memory-backend-file,id=mem,size=4096M,mem-path=/dev/hugepages,share=on \
        -device e1000,netdev=net0,mac=00:AD:BE:B3:11:00 \
        -netdev tap,id=net0,ifname=net0,script=/path/to/qemu-ifup \
        -nographic \
        -chardev socket,id=chr0,path=/tmp/sock0 \                   # /tmp/sock0
        -netdev vhost-user,id=net1,chardev=chr0,vhostforce \        # netdev for vhost-user
        -device virtio-net-pci,netdev=net1,mac=00:AD:BE:B4:11:00 \  # device for virtio-net-pci
        -monitor telnet::44911,server,nowait

This VM has two network interfaces.
``-device e1000`` is a management network port
which requires ``qemu-ifup`` to activate while launching.
Management network port is used for login and setup the VM.
``-device virtio-net-pci`` is created for SPP or DPDK application
running on the VM.

``vhost-user`` is a backend of ``virtio-net-pci`` which requires
a socket file ``/tmp/sock0`` created from secondary with ``-chardev``
option.

For other options, please refer to
`QEMU User Documentation
<https://qemu.weilnetz.de/doc/qemu-doc.html>`_.

.. note::

    To launch several VMs, you have to prepare qemu images for the VMs.
    You shortcut installing and setting up DPDK and SPP for each of
    VMs by creating a tmeplate image and copy it to the VMs.

After booted, you install DPDK and SPP in the VM as in the host.

Run ``spp_vm`` with options.

.. code-block:: console

    $ cd /path/to/spp
    $ sudo ./src/vm/x86_64-native-linuxapp-gcc/spp_vm \
        -l 0-1 -n 4 \
        --proc-type=primary \
        -- \
        -p 0x01 \
        -n 1 \
        -s 192.168.122.1:6666

- EAL options:

  - -l: core list (two cores required)
  - --proc-type: process type

- Application options:

  - -p: port mask
  - -n: secondary ID
  - -s: IP address of controller and port prepared for secondary

``spp_vm`` is also managed from SPP controller as same as on host.
Secondary ID is used to identify for sending messages and must be
unique among all of secondaries.
If you attempt to launch a secondary process with the same ID,
SPP controller does not accept it and assign unused number.

In this case, port mask option is ``-p 0x01`` (using one port) because
the VM is launched with just one vhost interface.
You can use two or more ports if you launch VM with several
``vhost-user`` and ``virtio-net-pci`` interfaces.

Notice that ``spp_vm`` takes options similar to ``spp_primary``, not
``spp_nfv``.
It means that ``spp_vm`` has responsibilities for initializing EAL
and forwarding packets in the VM.

.. note::

    ``spp_vm`` is actually running as primary process on a VM,
    but managed as secondary process from SPP controller.
    SPP does not support running resource manager as primary inside
    a VM. Client behaves as secondary, but actually a primary, running
    on the VM to communicate with other SPP procesess on host.

    ``spp_vm`` must be launched with ``--proc-type=primary`` and
    ``-p [PORTMASK]`` options similar to primary to initialize EAL.
