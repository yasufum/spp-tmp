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


Simple SSH Login
================

This section describes a usecase for simple SSH login through SPP VF.
Incoming packets are classified based on destination addresses defined
in ``classifier``.
Reterned packets are aggregated to ``merger`` to send it an outgoing
port.

.. _figure_simple_ssh_login:

.. figure:: ../images/usecase1_overview.*
    :height: 400 em
    :width: 400 em

    Simple SSH Login


Launch SPP VF
~~~~~~~~~~~~~

Change directory to spp and confirm that it is already compiled.

.. code-block:: console

    $ cd /path/to/spp

As spp, launch controller first. You notice that SPP VF has its own
controller ``spp_vf.py`` and do not use ``spp.py``.

.. code-block:: console

    # Launch spp_vf.py
    $ python ./src/spp_vf.py -p 5555 -s 6666

Then, run ``spp_primary``.

.. code-block:: console

    $ sudo ./src/primary/x86_64-native-linuxapp-gcc/spp_primary \
        -c 0x02 -n 4 \
        --socket-mem 512,512 \
        --huge-dir=/run/hugepages/kvm \
        --proc-type=primary \
        -- \
        -p 0x03 -n 8 -s 127.0.0.1:5555

After ``spp_primary`` is launched, run secondary process ``spp_vf``.

.. code-block:: console

    $ sudo ./src/vf/x86_64-native-linuxapp-gcc/spp_vf \
        -c 0x00fe -n 4 --proc-type=secondary \
        -- \
        --client-id 1 \
        -s 127.0.0.1:6666 \
        --vhost-client


Network Configuration
~~~~~~~~~~~~~~~~~~~~~

Detailed configuration of :numref:`figure_simple_ssh_login` is
described below.

.. _figure_network_config:

.. figure:: ../images/usecase1_nwconfig.*
    :height: 720 em
    :width: 720 em

    Network Configuration

First, start the ``component`` of classifier, forwarders, and merge
in ``spp_vf``.

.. code-block:: console

    # Start component to spp_vf
    spp > sec 1;component start classifier1 2 classifier_mac
    spp > sec 1;component start forward1 3 forward
    spp > sec 1;component start forward2 4 forward
    spp > sec 1;component start forward3 5 forward
    spp > sec 1;component start forward4 6 forward
    spp > sec 1;component start merge1 7 merge

Second, add ``port`` to each component in ``spp_vf``.

.. code-block:: console

    # Add port to component
    spp > sec 1;port add phy:0 rx classifier1
    spp > sec 1;port add ring:0 tx classifier1
    spp > sec 1;port add ring:1 tx classifier1
    spp > sec 1;port add ring:0 rx forward1
    spp > sec 1;port add vhost:0 tx forward1
    spp > sec 1;port add vhost:0 rx forward2
    spp > sec 1;port add ring:2 tx forward2
    spp > sec 1;port add ring:1 rx forward3
    spp > sec 1;port add vhost:2 rx forward3
    spp > sec 1;port add vhost:2 tx forward4
    spp > sec 1;port add ring:3 rx forward4
    spp > sec 1;port add ring:2 rx merge1
    spp > sec 1;port add ring:3 rx merge1
    spp > sec 1;port add phy:0 tx merge1

To communicate remote node and VM via NIC0, each of packets from
remote node is required to be routed to specific VM according
to its MAC address.
This configuration is done by ``classfier_table`` command.

.. code-block:: console

    # Register MAC address to classifier
    spp > classifier_table add mac 52:54:00:12:34:56 ring:0
    spp > classifier_table add mac 52:54:00:12:34:58 ring:1


For NIC1, also setup ``component``, ``port`` and ``classifier_table``,
as following steps.

.. code-block:: console

    # Start component to spp_vf
    spp > sec 1;component start classifier2 8 classifier_mac
    spp > sec 1;component start forward5 9 forward
    spp > sec 1;component start forward6 10 forward
    spp > sec 1;component start forward7 11 forward
    spp > sec 1;component start forward8 12 forward
    spp > sec 1;component start merge2 13 merge

.. code-block:: console

    # Add port to component
    spp > sec 1;port add phy:1 rx classifier2
    spp > sec 1;port add ring:4 tx classifier2
    spp > sec 1;port add ring:5 tx classifier2
    spp > sec 1;port add ring:4 rx forward5
    spp > sec 1;port add vhost:1 tx forward5
    spp > sec 1;port add vhost:1 rx forward6
    spp > sec 1;port add ring:6 tx forward6
    spp > sec 1;port add ring:5 rx forward7
    spp > sec 1;port add vhost:3 rx forward7
    spp > sec 1;port add vhost:3 tx forward8
    spp > sec 1;port add ring:7 rx forward8
    spp > sec 1;port add ring:6 rx merge2
    spp > sec 1;port add ring:7 rx merge2
    spp > sec 1;port add phy:1 tx merge2

.. code-block:: console

    # Register MAC address to classifier
    spp > classifier_table add mac 52:54:00:12:34:57 ring:4
    spp > classifier_table add mac 52:54:00:12:34:59 ring:5

To activate above settings, input the `flush` command.

.. code-block:: console

    spp > sec 1;flush

Setup for VMs
~~~~~~~~~~~~~

Start two VMs.

.. code-block:: console

    $ virsh start spp-vm1
    $ virsh start spp-vm2

Login to ``spp-vm1`` for network configuration.
To not ask for unknown keys while login VMs,
set ``-oStrictHostKeyChecking=no`` option for ssh.

.. code-block:: console

    $ ssh -oStrictHostKeyChecking=no sppuser@192.168.122.31

Up interfaces for vhost inside ``spp-vm1``.
In addition, you have to disable TCP offload function, or ssh is failed
after configuration is done.

.. code-block:: console

    # up interfaces
    $ sudo ifconfig ens4 inet 192.168.140.21 netmask 255.255.255.0 up
    $ sudo ifconfig ens5 inet 192.168.150.22 netmask 255.255.255.0 up

    # disable TCP offload
    $ sudo ethtool -K ens4 tx off
    $ sudo ethtool -K ens5 tx off

Configurations for ``spp-vm2`` is same as ``spp-vm1``.

.. code-block:: console

    $ ssh -oStrictHostKeyChecking=no sppuser@192.168.122.32

    # up interfaces
    $ sudo ifconfig ens4 inet 192.168.140.31 netmask 255.255.255.0 up
    $ sudo ifconfig ens5 inet 192.168.150.32 netmask 255.255.255.0 up

    # disable TCP offload
    $ sudo ethtool -K ens4 tx off
    $ sudo ethtool -K ens5 tx off


Login to VMs
~~~~~~~~~~~~

Now, you can login to VMs.

.. code-block:: console

    # spp-vm1 via NIC0
    $ ssh sppuser@192.168.140.21

    # spp-vm1 via NIC1
    $ ssh sppuser@192.168.150.22

    # spp-vm2 via NIC0
    $ ssh sppuser@192.168.140.31

    # spp-vm2 via NIC1
    $ ssh sppuser@192.168.150.32

Close Applications
~~~~~~~~~~~~~~~~~~

Describe the procedure to close the applications.

(1) Stop and delete command

By following commands from `spp_vf.py`, you can delete `classifier_table`
and ports, and stop components.
The `flush` command is required to reflect this deletion and stopping.
If you close the applications by `Ctrl+C` or `bye all` command,
all settings will be deleted, following steps are not mandatory.

First, delete the configuration between NIC-1 and VM and stop related components.

.. code-block:: console

    # Delete MAC address from Classifier
    spp > classifier_table del mac 51:54:00:12:34:56 ring:0
    spp > classifier_table del mac 51:54:00:12:34:58 ring:1

.. code-block:: console

    # Delete port to component
    spp > sec 0;port del phy:0 rx classifier1
    spp > sec 0;port del ring:0 tx classifier1
    spp > sec 0;port del ring:1 tx classifier1
    spp > sec 0;port del ring:0 rx forward1
    spp > sec 0;port del vhost:0 tx forward1
    spp > sec 0;port del vhost:0 rx forward2
    spp > sec 0;port del ring:2 tx forward2
    spp > sec 0;port del ring:1 rx forward3
    spp > sec 0;port del vhost:2 rx forward3
    spp > sec 0;port del vhost:2 tx forward4
    spp > sec 0;port del ring:3 rx forward4
    spp > sec 0;port del ring:2 rx merge1
    spp > sec 0;port del ring:3 rx merge1
    spp > sec 0;port del phy:0 tx merge1

.. code-block:: console

    # Stop component to spp_vf
    spp > sec 0;component stop classifier1
    spp > sec 0;component stop forward1
    spp > sec 0;component stop forward2
    spp > sec 0;component stop forward3
    spp > sec 0;component stop forward4
    spp > sec 0;component stop merge1

Second, delete the configuration between NIC0 and VM and stop related components.

.. code-block:: console

    # Delete MAC address from Classifier
    spp > classifier_table del mac 51:54:00:12:34:57 ring:4
    spp > classifier_table del mac 51:54:00:12:34:59 ring:5

.. code-block:: console

    # Delete port to component
    spp > sec 0;port del phy:1 rx classifier2
    spp > sec 0;port del ring:4 tx classifier2
    spp > sec 0;port del ring:5 tx classifier2
    spp > sec 0;port del ring:4 rx forward5
    spp > sec 0;port del vhost:1 tx forward5
    spp > sec 0;port del vhost:1 rx forward6
    spp > sec 0;port del ring:6 tx forward6
    spp > sec 0;port del ring:5 rx forward7
    spp > sec 0;port del vhost:3 rx forward7
    spp > sec 0;port del vhost:3 tx forward8
    spp > sec 0;port del ring:7 rx forward8
    spp > sec 0;port del ring:6 rx merge2
    spp > sec 0;port del ring:7 rx merge2
    spp > sec 0;port del phy:1 tx merge2

.. code-block:: console

    # Stop component to spp_vf
    spp > sec 0;component stop classifier2 8 classifier_mac
    spp > sec 0;component stop forward5 9 forward
    spp > sec 0;component stop forward6 10 forward
    spp > sec 0;component stop forward7 11 forward
    spp > sec 0;component stop forward8 12 forward
    spp > sec 0;component stop merge2 13 merge

To activate above settings, input the ``flush`` command.

.. code-block:: console

    spp > sec 0;flush


(2) Close SPP VF

Simply, ``spp_vf`` and primary process can be closed
by ``Ctrl+C`` or ``bye all`` command from ``spp_vf.py``.
Also ``spp_vf.py`` can be closed by the ``bye`` command.

.. code-block:: console

    # stop controller
    spp > bye all
    spp > bye
