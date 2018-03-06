..  BSD LICENSE
    Copyright(c) 2017 Nippon Telegraph and Telephone Corporation
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
    * Neither the name of Nippon Telegraph and Telephone Corporation
    nor the names of its contributors may be used to endorse or
    promote products derived from this software without specific
    prior written permission.

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


Experimental Commands
=====================

There are experimental commands in SPP controller.
It might not work for some cases properly because it is not well tested
currently.

topo
----

Output network topology in several formats.

Support four types of output.
* terminal (but very few terminals supporting to display images)
* browser (websocket server is required)
* image file (jpg, png, bmp)
* text (dot, json, yaml)

Most used format migth be ``term`` for output an image of network
configuration in terminal.
``topo`` command also show an image in a browser.

This command uses `graphviz
<https://www.graphviz.org/>`_
for generating topology file and you can
also generate a dot formatted file directory.

There are some usecases.

.. code-block:: console

    spp > topo term  # terminal
    spp > topo http  # browser
    spp > topo network_conf.jpg  # image
    spp > topo network_conf.dot  # text


topo_subgraph
-------------

``topo_subgraph`` is a supplemental command for manageing subgraphs
for ``topo``.

Subgraph is a group of object defined in dot language. Grouping objects
helps your understanding relationship or hierarchy of each of objects.
For topo command, it is used for grouping resources of each
of VM or container to topology be more understandable.

For example, add subgraph labeled ``vm1`` for a VM which has two vhost
interfaces ``VHOST1`` and ``VHOST2``.
You do not need to use upper case for resource names because
``topo_subgraph`` command capitalizes given names internally.

.. code-block:: console

    spp > topo_subgraph add vm1 VHOST1;VHOST2  # upper case
    spp > topo_subgraph add vm1 vhost1;vhost2  # lower case

If VM is shut down and subgraph is not needed anymore,
delete subgraph 'vm1'.

.. code-block:: console

    spp > topo_subgraph del vm1

To show all of subgraphs, run topo_subgraph without args.

.. code-block:: console

    spp > topo_subgraph
    label: vm2    subgraph: "VHOST3;VHOST4"
    label: vm1    subgraph: "VHOST1;VHOST2"


load_cmd
--------

Load a command plugin dynamically while running SPP controller.


Plugin file must be placed in ``spp/src/controller/command`` and
command name must be the same as file name.
For example, ``hello`` command is loaded from
``spp/src/controller/command/hello.py``.

.. code-block:: console

    spp > load hello
    Module 'command.hello' loaded.
    spp > hello alice
    Hello, alice!
