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


Performance Optimization
========================

Reduce Context Switches
-----------------------

Use the ``isolcpus`` Linux kernel parameter to isolate them
from Linux scheduler to reduce context switches.
It prevents workloads of other processes than DPDK running on
reserved cores with ``isolcpus`` parameter.

For Ubuntu 16.04, define ``isolcpus`` in ``/etc/default/grub``.

.. code-block:: console

    GRUB_CMDLINE_LINUX_DEFAULT=“isolcpus=0-3,5,7”

The value of this ``isolcpus`` depends on your environment and usage.
This example reserves six cores(0,1,2,3,5,7).


Optimizing QEMU Performance
---------------------------

QEMU process runs threads for vcpu emulation. It is effective strategy
for pinning vcpu threads to decicated cores.

To find vcpu threads, you use ``ps`` command to find PID of QEMU process
and ``pstree`` command for threads launched from QEMU process.

.. code-block:: console

    $ ps ea
       PID TTY     STAT  TIME COMMAND
    192606 pts/11  Sl+   4:42 ./x86_64-softmmu/qemu-system-x86_64 -cpu host ...

Run ``pstree`` with ``-p`` and this PID to find all threads launched from QEMU.

.. code-block:: console

    $ pstree -p 192606
    qemu-system-x86(192606)--+--{qemu-system-x8}(192607)
                             |--{qemu-system-x8}(192623)
                             |--{qemu-system-x8}(192624)
                             |--{qemu-system-x8}(192625)
                             |--{qemu-system-x8}(192626)

Update affinity by using ``taskset`` command to pin vcpu threads.
The vcpu threads is listed from the second entry and later.
In this example, assign PID 192623 to core 4, PID 192624 to core 5
and so on.

.. code-block:: console

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


Reference
---------

* [1] `Best pinning strategy for latency/performance trade-off
  <https://www.redhat.com/archives/vfio-users/2017-February/msg00010.html>`_
* [2] `PVP reference benchmark setup using testpmd
  <http://dpdk.org/doc/guides/howto/pvp_reference_benchmark.html>`_
* [3] `Enabling Additional Functionality
  <http://dpdk.org/doc/guides/linux_gsg/enable_func.html>`_
* [4] `How to get best performance with NICs on Intel platforms
  <http://dpdk.org/doc/guides/linux_gsg/nic_perf_intel_platform.html>`_
