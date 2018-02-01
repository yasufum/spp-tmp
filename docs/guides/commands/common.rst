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


Common Commands
====================

status
------

Display number of connected primary and secondary application count
Also display connected secondary application client_id.

.. code-block:: console

    spp > status
    Soft Patch Panel Status :
    primary: 1
    secondary count: 4
    Connected secondary id: 1
    Connected secondary id: 2


record
------

.. code-block:: console

    spp > record 2nfv_uni.config


playback
--------

.. code-block:: console

    spp> playback 2nfv_uni.config


bye
---

``bye`` command is for terminating SPP processes.
It supports two types of termination as sub commands.

  - sec
  - all

First one is for terminating only secondary processes at once.

.. code-block:: console

    spp > bye sec
    closing:<socket._socketobject object at 0x105750910>
    closing:<socket._socketobject object at 0x105750a60>

Second one is for all SPP processes other than controller.

.. code-block:: console

    spp > bye all
    closing:<socket._socketobject object at 0x10bd95910>
    closing:<socket._socketobject object at 0x10bd95a60>
    closing:('127.0.0.1', 53620)

help
----

.. note::

    ``help`` command is not implemented yet.

Displays brief help

.. code-block:: console

    spp > help
