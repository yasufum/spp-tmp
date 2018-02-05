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

SPP_VF
------

``SPP_VF`` is a SR-IOV like network functionality for NFV.

.. image:: images/spp_vf_overview.svg
   :height: 550 em
   :width: 550 em

Environment
-----------

* Ubuntu 16.04
* qemu-kvm 2.7 or later
* DPDK v17.11 or later

Launch SPP
----------

Before launching spp, you need to setup DPDK, virsh, etc. described at
:doc:`build`.

SPP Controller
~~~~~~~~~~~~~~

First, run SPP Controller with port numbers for spp primary and secondary.

.. code-block:: console

    $ python ./src/spp_vf.py -p 5555 -s 6666


SPP Primary
~~~~~~~~~~~

SPP primary allocates and manages resources for secondary processes.
You need to run SPP primary before secondary processes.

SPP primary has two kinds of options for DPDK and spp.
Before ``--`` are for DPDK is, and after ``--`` are for spp.

See
`DPDK documentation <http://dpdk.org/doc/guides/linux_gsg/build_sample_apps.html#running-a-sample-application>`_
about options for DPDK.

Options of spp primary are:

  * -p : port mask
  * -n : number of rings
  * -s : IPv4 address and port for spp primary

Then, spp primary can be launched like this.

.. code-block:: console

    $ sudo ./src/primary/x86_64-native-linuxapp-gcc/spp_primary \
      -c 0x02 -n 4 --socket-mem 512,512 \
      --huge-dir=/run/hugepages/kvm \
      --proc-type=primary \
      -- -p 0x03 -n 9 -s 127.0.0.1:5555

SPP Secondary
~~~~~~~~~~~~~

spp secondary processes(``spp_vf``) can be launched with two kinds of
options, like primary process.

Like primary process, ``spp_vf`` has two kinds of options. One is for
DPDK, the other is ``spp_vf``.

``spp_vf`` specific options are:

  * --client-id    : client id
  * -s             : IPv4 address and port for spp secondary
  * --vhost-client : vhost-user client enable setting

``spp_vf`` can be launched like this.

.. code-block:: console

    $ sudo ./src/vf/x86_64-native-linuxapp-gcc/spp_vf \
    -c 0x3ffd -n 4 --proc-type=secondary \
    -- --client-id 1 -s 127.0.0.1:6666 --vhost-client

If ``--vhost-client`` option is specified, then ``vhost-user`` act as
the client, otherwise the server.
For reconnect feature from SPP to VM, ``--vhost-client`` option can be
used. This reconnect features requires QEMU 2.7 (or later).
See also `DPDK documentation <http://dpdk.org/doc/guides/sample_app_ug/vhost.html>_.

VM
--

Launch VMs with ``virsh`` command.

.. code-block:: console

    $ virsh start [VM]


Additional Network Configurations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To enable processes running on the VM to communicate through spp,
it is required additional network configurations on host and guest VMs.

Guest VMs
"""""""""

.. code-block:: console

    # Interface for vhost
    $ sudo ifconfig [IF_NAME] inet [IP_ADDR] netmask [NETMASK] up

    # Disable offload for vhost interface
    $ sudo ethtool -K [IF_NAME] tx off

Host2
"""""

.. code-block:: console

    # Disable offload for VM interface
    $ ethtool -K [IF_NAME] tx off
