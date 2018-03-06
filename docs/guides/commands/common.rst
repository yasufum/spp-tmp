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

Show the number of connected primary and secondary processes.
It also show a list of secondary IDs

.. code-block:: console

    spp > status
    Soft Patch Panel Status :
    primary: 1
    secondary count: 2
    Connected secondary id: 1
    Connected secondary id: 2


record
------

Start recording user's input and create a history file for ``playback``
commnad.
Recording is stopped by executing ``exit`` or ``playback`` command.

.. code-block:: console

    spp > record 2nfv_uni.config

.. note::

    It is not supported to stop recording without ``exit`` or ``playback``
    command.
    It is planned to support ``stop`` command for stopping record in
    next relase.


playback
--------

Restore configuration from a config file.
Content of config file is just a series of SPP commnad.
You prepare a config file by using ``record`` command or editing a text
file by hand.

It is recommended to use extension ``.config`` to be self-sxplanatory
as a config, although you can use any of extensions such as ``.txt`` or
``.log``.

.. code-block:: console

    spp> playback 2nfv_uni.config


pwd
---

Show current path.

.. code-block:: console

    spp> pwd
    /path/to/curdir


cd
--

Change current directory.

.. code-block:: console

    spp> cd /path/to/dir


ls
--

Show a list of directory contents.

.. code-block:: console

    spp> ls /path/to/dir


mkdir
-----

Make a directory.

.. code-block:: console

    spp> mkdir /path/to/dir


cat
---

Show contents of a file.

.. code-block:: console

    spp> cat /path/to/file


less
----

Show contents of a file.

.. code-block:: console

    spp> less /path/to/file


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


exit
----

Same as ``bye`` command but just for terminating SPP controller and
not for other processes.

.. code-block:: console

    spp > exit
    Thank you for using Soft Patch Panel


help
----

Show help message for SPP commands.

.. code-block:: console

    spp > help

    Documented commands (type help <topic>):
    ========================================
    bye  cd    help  load_cmd  mkdir     pri  record  status  topo_subgraph
    cat  exit  less  ls        playback  pwd  sec     topo

    spp > help status
    Display status info of SPP processes

        spp > status

    spp > help sec
    Send command to secondary process

        SPP secondary process is specified with secondary ID and takes
        sub commands.

        spp > sec 1;status
        spp > sec 1;add ring 0
        spp > sec 1;patch 0 2
