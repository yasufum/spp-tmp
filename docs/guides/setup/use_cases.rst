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

Use Cases
=========

.. _single_spp_nfv:

Single spp_nfv
--------------

The most simple use case mainly for testing performance of packet
forwarding on host.
One ``spp_nfv`` and two physical ports.

In this use case, try to configure two senarios.

- Configure spp_nfv as L2fwd
- Configure spp_nfv for Loopback


First of all, Check the status of ``spp_nfv`` from SPP controller.

.. code-block:: console

    spp > sec 1;status
    status: idling
    ports:
      - 'phy:0'
      - 'phy:1'

This message explains that ``sec 1`` has two physical ports.


Configure spp_nfv as L2fwd
~~~~~~~~~~~~~~~~~~~~~~~~~~

Assing the destination of ports with ``patch`` subcommand and
start forwarding.
Patch from ``phy:0`` to ``phy:1`` and ``phy:1`` to ``phy:0``,
which means it is bi-directional connection.

.. code-block:: console

    spp > sec 1;patch phy:0 phy:1
    spp > sec 1;patch phy:1 phy:0
    spp > sec 1;forward

Confirm that status of ``sec 1`` is updated.

.. code-block:: console

    spp > sec 1;status
    status: running
    ports:
      - 'phy:0 -> phy:1'
      - 'phy:1 -> phy:0'

.. _figure_spp_nfv_as_l2fwd:

.. figure:: ../images/setup/use_cases/spp_nfv_l2fwd.*
   :height: 380 em
   :width: 380 em

   spp_nfv as l2fwd


Stop forwarding and reset patch to clear configuration.

.. code-block:: console

    spp > sec 1;stop
    spp > sec 1;patch reset


Configure spp_nfv for Loopback
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Patch ``phy:0`` to ``phy:0`` and ``phy:1`` to ``phy:1``
for loopback.

.. code-block:: console

    spp > sec 1;patch phy:0 phy:0
    spp > sec 1;patch phy:1 phy:1
    spp > sec 1;forward


Dual spp_nfv
------------

Use case for testing performance of packet forwarding
with two ``spp_nfv`` on host.
Throughput is expected to be better than
:ref:`Single spp_nfv<single_spp_nfv>`
use case
because bi-directional forwarding of single nfv shared with two of
uni-directional forwarding between dual spp_nfv.

In this use case, configure two senarios almost similar to
previous section.

- Configure Two spp_nfv as L2fwd
- Configure Two spp_nfv for Loopback


Configure Two spp_nfv as L2fwd
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Assing the destination of ports with ``patch`` subcommand and
start forwarding.
Patch from ``phy:0`` to ``phy:1`` for ``sec 1`` and
from ``phy:1`` to ``phy:0`` for ``sec 2``.

.. code-block:: console

    spp > sec 1;patch phy:0 phy:1
    spp > sec 2;patch phy:1 phy:0
    spp > sec 1;forward
    spp > sec 2;forward

.. _figure_spp_two_nfv_as_l2fwd:

.. figure:: ../images/setup/use_cases/spp_two_nfv_l2fwd.*
   :height: 420 em
   :width: 420 em

   Two spp_nfv as l2fwd


Configure two spp_nfv for Loopback
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Patch ``phy:0`` to ``phy:0`` for ``sec 1`` and
``phy:1`` to ``phy:1`` for ``sec 2`` for loopback.

.. code-block:: console

    spp > sec 1;patch phy:0 phy:0
    spp > sec 2;patch phy:1 phy:1
    spp > sec 1;forward
    spp > sec 2;forward

.. _figure_spp_two_nfv_loopback:

.. figure:: ../images/setup/use_cases/spp_two_nfv_loopback.*
   :height: 420 em
   :width: 420 em

   Two spp_nfv for loopback


Dual spp_nfv with Ring PMD
--------------------------

In this use case, configure two senarios by using ring PMD.

- Uni-Directional L2fwd
- Bi-Directional L2fwd

Ring PMD
~~~~~~~~

Ring PMD is an interface for communicating between secondaries on host.
The maximum number of ring PMDs is defined as ``-n``  option of
``spp_primary`` and ring ID is started from 0.

Ring PMD is added by using ``add`` subcommand.
All of ring PMDs is showed with ``status`` subcommand.

.. code-block:: console

    spp > sec 1;add ring 0
    recv:6:{addring0}
    spp > sec 1;status
    status: idling
    ports:
      - 'phy:0'
      - 'phy:1'
      - 'ring:0'

Notice that ``ring:0`` is added to ``sec 1``.
You can delete it with ``del`` command if you do not need to
use it anymore.

.. code-block:: console

    spp > sec 1;del ring 0
    recv:6:{delring0}
    spp > sec 1;status
    status: idling
    ports:
      - 'phy:0'
      - 'phy:1'


Uni-Directional L2fwd
~~~~~~~~~~~~~~~~~~~~~

Add a ring PMD and connect two ``spp_nvf`` processes.
To configure network path, add ``ring 0`` to ``sec 1`` and ``sec 2``.
Then, connect it with ``patch`` subcommand.

.. code-block:: console

    spp > sec 1;add ring 0
    spp > sec 2;add ring 0
    spp > sec 1;patch phy:0 ring:0
    spp > sec 2;patch ring:0 phy:1
    spp > sec 1;forward
    spp > sec 2;forward

.. _figure_spp_uni_directional_l2fwd:

.. figure:: ../images/setup/use_cases/spp_unidir_l2fwd.*
   :height: 460 em
   :width: 460 em

   Uni-Directional l2fwd


Bi-Directional L2fwd
~~~~~~~~~~~~~~~~~~~~

Add two ring PMDs to two ``spp_nvf`` processes.
For bi-directional forwarding,
patch ``ring 0`` for a path from ``sec 1`` to ``sec 2``
and ``ring 1`` for another path from ``sec 2`` to ``sec 1``.

First, add ``ring 0`` and ``ring 1`` to ``sec 1``.

.. code-block:: console

    spp > sec 1;add ring 0
    spp > sec 1;add ring 1
    status: idling
    ports:
      - 'phy:0'
      - 'phy:1'
      - 'ring:0'
      - 'ring:1'


Then, add ``ring 0`` and ``ring 1`` to ``sec 2``.

.. code-block:: console

    spp > sec 2;add ring 0
    spp > sec 2;add ring 1
    status: idling
    ports:
      - 'phy:0'
      - 'phy:1'
      - 'ring:0'
      - 'ring:1'

.. code-block:: console

    spp > sec 1;patch phy:0 ring:0
    spp > sec 1;patch ring:1 phy:0
    spp > sec 2;patch phy:1 ring:1
    spp > sec 2;patch ring:0 phy:1
    spp > sec 1;forward
    spp > sec 2;forward

.. _figure_spp_bi_directional_l2fwd:

.. figure:: ../images/setup/use_cases/spp_bidir_l2fwd.*
   :height: 460 em
   :width: 460 em

   Bi-Directional l2fwd


Single spp_nfv with Vhost PMD
-----------------------------

Vhost PMD
~~~~~~~~~

Vhost PMD is an interface for communicating between on hsot and guest VM.
As described in
:doc:`How to Use<howto_use>`,
vhost must be created by ``add`` subcommand before the VM is launched.


Setup Vhost PMD
~~~~~~~~~~~~~~~

In this use case, add ``vhost 0`` to ``sec 1`` for communicating
with the VM.
First, check if ``/tmp/sock0`` is already exist.
You have to remove it already exist to avoid failure of socket file
creation.

.. code-block:: console

    $ ls /tmp | grep sock
    sock0 ...

    # remove it if exist
    $ rm /tmp/sock0

Create ``/tmp/sock0`` from ``sec 1``.

.. code-block:: console

    spp > sec 1;add vhost 0


Uni-Directional L2fwd with Vhost PMD
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Start a VM with vhost interface as described in
:doc:`How to Use<howto_use>`
and launch ``spp_vm`` with secondary ID 2.
You find ``sec 2`` from controller after launched.

Patch ``phy:0`` and ``phy:1`` to ``vhost:0`` with ``sec 1``
running on host.
Inside VM, configure loopback by patching ``phy:0`` and ``phy:0``
with ``sec 2``.

.. code-block:: console

    spp > sec 1;patch phy:0 vhost:0
    spp > sec 1;patch vhost:0 phy:1
    spp > sec 2;patch phy:0 phy:0
    spp > sec 1;forward
    spp > sec 2;forward

.. _figure_spp_uni_directional_l2fwd_vhost:

.. figure:: ../images/setup/use_cases/spp_unidir_l2fwd_vhost.*
   :height: 460 em
   :width: 460 em

   Uni-Directional l2fwd with vhost
