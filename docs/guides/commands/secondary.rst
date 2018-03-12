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

Secondary Commands
======================

Each of secondary processes is managed with ``sec`` command.
It is for sending sub commands to secondary with specific ID called
secondary ID.

``sec`` command takes an secondary ID and a sub command. They must be
separated with delimiter ``;``.
Some of sub commands take additional arguments for speicfying resource
owned by secondary process.

.. code-block:: console

    spp > sec [SEC_ID];[SUB_CMD]


status
------

Show running status and resources.

.. code-block:: console

    spp > sec 1;status
    status: idling
    ports:
      - 'phy:0'
      - 'phy:1'


add
---

Add a PMD to the secondary with resource ID.

Adding ring 0 by

.. code-block:: console

    spp> sec 1;add ring 0

Or adding vhost 0 by

.. code-block:: console

    spp> sec 1;add vhost 0


patch
------

Create a path between two ports, source and destination ports.
This command just creates path and does not start forwarding.

.. code-block:: console

    spp > sec 1;patch phy:0 ring:0


forward
-------

Start forwarding.

.. code-block:: console

    spp > sec 1;forward

Running status is changed from ``idling`` to ``running`` by
executing it.

.. code-block:: console

    spp > sec 1;status
    status: running
    ports:
      - 'phy:0'
      - 'phy:1'


stop
----

Stop forwarding.

.. code-block:: console

    spp > sec 1;stop

Running status is changed from ``running`` to ``idling`` by
executing it.

.. code-block:: console

    spp > sec 1;status
    status: idling
    ports:
      - 'phy:0'
      - 'phy:1'


del
---

Delete PMD added by ``add`` subcommand from the secondary.

.. code-block:: console

    spp> sec 1;del ring 0


exit
----

Terminate the secondary. For terminating all secondaries, use ``bye sec``
command instead of it.

.. code-block:: console

    spp> sec 1;exit
