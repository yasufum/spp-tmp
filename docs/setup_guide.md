Sample usage of the application
===============================

Compilation
-----------
Change to DPDK directory
Set RTE_SDK variable to current folder
Set RTE_TARGET variable to any valid target.
Compile DPDK: "make T=x86_64-native-linuxapp-gcc install"

Change to SPP directory
Compile SPP: "make"

Start Controller
----------------
python spp.py -p 5555 -s 6666

Start spp_primary
-----------------
sudo ./src/primary/src/primary/x86_64-native-linuxapp-gcc/spp_primary \
	-c 0x02 -n 4 \
	--socket-mem 512,512 \
	--huge-dir=/dev/hugepages \
	--proc-type=primary \
	-- \
	-p 0x03 \
	-n 4 \
	-s 192.168.122.1:5555

Start spp_nfv
-------------
sudo ./src/nfv/src/nfv/x86_64-native-linuxapp-gcc/spp_nfv \
	-c 0x06 -n 4 \
	--proc-type=secondary \
	-- \
	-n 1 \
	-s 192.168.122.1:6666

sudo ./src/nfv/src/nfv/x86_64-native-linuxapp-gcc/spp_nfv \
	-c 0x0A -n 4 \
	--proc-type=secondary \
	-- \
	-n 1 \
	-s 192.168.122.1:6666

Start VM (QEMU)
---------------
Common qemu command line:
sudo ./x86_64-softmmu/qemu-system-x86_64 \
	-cpu host \
	-enable-kvm \
	-object memory-backend-file,id=mem,size=2048M,mem-path=/dev/hugepages,share=on \
	-numa node,memdev=mem \
	-mem-prealloc \
	-hda /home/dpdk/debian_wheezy_amd64_standard.qcow2 \
	-m 2048 \
	-smp cores=4,threads=1,sockets=1 \
	-device e1000,netdev=net0,mac=DE:AD:BE:EF:00:01 \
	-netdev tap,id=net0 \
	-nographic -vnc :2

To start spp_vm "qemu-ifup" script required, please copy docs/qemu-ifup to host /etc/qemu-ifup

Vhost interface is supported to communicate between guest and host:

vhost interface
~~~~~~~~~~~~~~~~~
  - spp should do a "sec x:add vhost y" before starting the VM. x: vnf number, y: vhost port id.
  - Needs vhost argument for qemu:
    sudo ./x86_64-softmmu/qemu-system-x86_64 \
	-cpu host \
	-enable-kvm \
	-object memory-backend-file,id=mem,size=2048M,mem-path=/dev/hugepages,share=on \
	-numa node,memdev=mem \
	-mem-prealloc \
	-hda /home/dpdk/debian_wheezy_amd64_standard.qcow2 \
	-m 2048 \
	-smp cores=4,threads=1,sockets=1 \
	-device e1000,netdev=net0,mac=DE:AD:BE:EF:00:01 \
	-netdev tap,id=net0 \
	-chardev socket,id=chr0,path=/tmp/sock0 \
	-netdev vhost-user,id=net1,chardev=chr0,vhostforce \
	-device virtio-net-pci,netdev=net1 \
	-nographic -vnc :2

Start spp_vm (Inside the VM)
----------------------------
sudo ./src/vm/src/vm/x86_64-native-linuxapp-gcc/spp_vm \
	-c 0x03 -n 4 \
	--proc-type=primary \
	-- \
	-p 0x01 \
	-n 1 \
	-s 192.168.122.1:6666

Test Setups
===========

Test Setup 1: Single NFV
------------------------

                                                                        __
                                    +--------------+                      |
                                    |    spp_nfv   |                      |
                                    |              |                      |
                                    +--------------+                      |
                                         ^      :                         |
                                         |      |                         |
                                         :      v                         |
    +----+----------+-------------------------------------------------+   |
    |    | primary  |                    ^      :                     |   |
    |    +----------+                    :      :                     |   |
    |                                    :      :                     |   |
    |                         +----------+      +---------+           |   |  host
    |                         :                           v           |   |
    |                  +--------------+            +--------------+   |   |
    |                  |   phy port 0 |  ovs-dpdk  |   phy port 1 |   |   |
    +------------------+--------------+------------+--------------+---+ __|
                              ^                           :
                              |                           |
                              :                           v

Configuration for L2fwd
~~~~~~~~~~~~~~~~~~~~~~~
spp > sec 0;patch 0 1
spp > sec 0;patch 1 0
spp > sec 0;forward

Configuration for loopback
~~~~~~~~~~~~~~~~~~~~~~~~~~
spp > sec 0;patch 0 0
spp > sec 0;patch 1 1
spp > sec 0;forward


Test Setup 2: Dual NFV
----------------------

                                                                        __
                         +--------------+          +--------------+       |
                         |    spp_nfv   |          |    spp_nfv   |       |
                         |              |          |              |       |
                         +--------------+          +--------------+       |
                            ^        :               :         :          |
                            |        |      +--------+         |          |
                            :        v      |                  v          |
    +----+----------+-----------------------+-------------------------+   |
    |    | primary  |       ^        :      |                  :      |   |
    |    +----------+       |        +------+--------+         :      |   |
    |                       :               |        :         :      |   |
    |                       :        +------+        :         |      |   |  host
    |                       :        v               v         v      |   |
    |                  +--------------+            +--------------+   |   |
    |                  |   phy port 0 |            |   phy port 1 |   |   |
    +------------------+--------------+------------+--------------+---+ __|
                              ^                           :
                              |                           |
                              :                           v

Configuration for L2fwd
~~~~~~~~~~~~~~~~~~~~~~~
spp > sec 0;patch 0 1
spp > sec 1;patch 1 0
spp > sec 0;forward
spp > sec 1;forward


                                                                        __
                         +--------------+          +--------------+       |
                         |    spp_nfv   |          |    spp_nfv   |       |
                         |              |          |              |       |
                         +--------------+          +--------------+       |
                            ^        :               ^         :          |
                            |        |               |         |          |
                            :        v               :         v          |
    +----+----------+-------------------------------------------------+   |
    |    | primary  |       ^        :               ^         :      |   |
    |    +----------+       |        :               |         :      |   |
    |                       :        :               :         :      |   |
    |                       :        |               :         |      |   |  host
    |                       :        v               :         v      |   |
    |                  +--------------+            +--------------+   |   |
    |                  |   phy port 0 |            |   phy port 1 |   |   |
    +------------------+--------------+------------+--------------+---+ __|
                              ^                           ^
                              |                           |
                              v                           v

Configuration for loopback
~~~~~~~~~~~~~~~~~~~~~~~~~~
spp > sec 0;patch 0 0
spp > sec 1;patch 1 1
spp > sec 0;forward
spp > sec 1;forward


Test Setup 3: Dual NFV with ring pmd
------------------------------------

                                                                        __
                       +----------+      ring        +----------+         |
                       |  spp_nfv |    +--------+    |  spp_nfv |         |
                       |        2 | -> |  |  |  |- > |  2       |         |
                       +----------+    +--------+    +----------+         |
                          ^                                   :           |
                          |                                   |           |
                          :                                   v           |
    +----+----------+-------------------------------------------------+   |
    |    | primary  |       ^                               :         |   |
    |    +----------+       |                               :         |   |
    |                       :                               :         |   |
    |                       :                               |         |   |  host
    |                       :                               v         |   |
    |                  +--------------+            +--------------+   |   |
    |                  |   phy port 0 |            |   phy port  1|   |   |
    +------------------+--------------+------------+--------------+---+ __|
                              ^                           :
                              |                           |
                              :                           v

Configuration for Uni directional L2fwd
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
spp > sec 0;add ring 0
spp > sec 1;add ring 0
spp > sec 0;patch 0 2
spp > sec 1;patch 2 1
spp > sec 0;forward
spp > sec 1;forward


                                                                        __
                                         ring                             |
                                       +--------+                         |
                       +----------+ <--|  |  |  |<-- +----------+         |
                       |        3 |    +--------+    |  3       |         |
                       |  spp_nfv |                  |  spp_nfv |         |
                       |        2 |--> +--------+ -->|  2       |         |
                       +----------+    |  |  |  |    +----------+         |
                          ^            +--------+             ^           |
                          |              ring                 |           |
                          v                                   v           |
    +----+----------+-------------------------------------------------+   |
    |    | primary  |       ^                               ^         |   |
    |    +----------+       |                               :         |   |
    |                       :                               :         |   |
    |                       :                               |         |   |  host
    |                       v                               v         |   |
    |                  +--------------+            +--------------+   |   |
    |                  |   phy port 0 |            |   phy port  1|   |   |
    +------------------+--------------+------------+--------------+---+ __|
                              ^                           ^
                              |                           |
                              v                           v

Configuration for L2fwd
~~~~~~~~~~~~~~~~~~~~~~~
spp > sec 0;add ring 0
spp > sec 0;add ring 1
spp > sec 1;add ring 0
spp > sec 1;add ring 1
spp > sec 0;patch 0 2
spp > sec 0;patch 3 0
spp > sec 1;patch 1 3
spp > sec 1;patch 2 1
spp > sec 0;forward
spp > sec 1;forward


Test Setup 4: Single NFV with VM through vhost pmd
--------------------------------------------------

                                                   __
                          +----------------------+   |
                          | guest                |   |
                          |                      |   |
                          |   +-------------+    |   |  guest 192.168.122.51
                          |   |    spp_vm   |    |   |
                          |   |      0      |    |   |
                          +---+--------------+---+ __|
                               ^           :
                               |           |
                               |  virtio   |
                               |           |
                               |           V                            __
                           +--------------------+                         |
                           |  spp_nfv           |                         |
                           |  2                 |                         |
                           +--------------------+                         |
                               ^           :                              |
                               |           +---------- +                  |
                               :                       v                  |
    +----+----------+--------------------------------------------+        |
    |    | primary  |       ^                          :         |        |
    |    +----------+       |                          :         |        |
    |                       :                          :         |        |
    |                       :                          |         |        |  host 192.168.122.1
    |                       :                          v         |        |
    |                  +--------------+       +--------------+   |        |
    |                  |   phy port 0 |       |   phy port  1|   |        |
    +------------------+--------------+-------+--------------+---+      __|
                              ^                           :
                              |                           |
                              :                           v

Legend:-
sec 0 = spp_nfv
sec 1 = spp_vm


Configuration for Uni directional L2fwd
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
[rm –rf /tmp/sock0]
spp > sec 0;add vhost 0
[start VM]
spp > sec 0;patch 0 2
spp > sec 0;patch 2 1
spp > sec 1;patch 0 0
spp > sec 1;forward
spp > sec 0;forward



Optimizing qemu performance
---------------------------
First find out the PID for qemu-system-x86 process
ps ea
   PID TTY      STAT   TIME COMMAND
192606 pts/11   Sl+    4:42 ./x86_64-softmmu/qemu-system-x86_64 -cpu host -enable-kvm -object memory-backend-file,id=mem,siz

Using pstree to list out qemu-system-x86_64 threads:-
pstree -p 192606
qemu-system-x86(192606)--+--{qemu-system-x8}(192607)
                         |--{qemu-system-x8}(192623)
                         |--{qemu-system-x8}(192624)
                         |--{qemu-system-x8}(192625)
                         |--{qemu-system-x8}(192626)

To Optimize, use taskset to pin each thread:-
$ sudo taskset -pc 4 192623
pid 192623's current affinity list: 0-31
pid 192623's new affinity list: 4
$ sudo taskset -pc 5 192624
pid 192624's current affinity list: 0-31
pid 192624's new affinity list: 5
$ sudo taskset -pc 6 192625
pid 192625's current affinity list: 0-31
pid 192625's new affinity list: 6
$ sudo taskset -pc 7 192626
pid 192626's current affinity list: 0-31
pid 192626's new affinity list: 7
