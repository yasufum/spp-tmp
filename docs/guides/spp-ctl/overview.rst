..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

.. _spp_ctl_overview:

spp-ctl
=======

Architecture
------------

The design goal of spp-ctl is to be as simple as possible.
It is stateless.
Basically, spp-ctl only converts API requests into commands of SPP
processes and throws request, thouth it does syntax and lexical check
for API requests.

``spp-ctl`` adopts
`bottle
<https://bottlepy.org/docs/dev/>`_
which is simple and well known as a web framework and
`eventlet
<http://eventlet.net/>`_
for parallel processing.
``spp-ctl`` accepts multiple requests at the same time and serializes them
internally.


Setup
=====

You are required to install Python3 and packages described in
``requirements.txt`` via ``pip3`` for launching ``spp-ctl``.
You might need to run ``pip3`` with ``sudo``.

.. code-block:: console

    $ sudo apt update
    $ sudo apt install python3
    $ sudo apt install python3-pip
    $ pip3 install -r requirements.txt

Usage
-----

.. code-block:: console

    usage: spp-ctl [-p PRI_PORT] [-s SEC_PORT] [-a API_PORT]

    optional arguments:
      -p PRI_PORT  primary port. default is 5555.
      -s SEC_PORT  secondary port. default is 6666.
      -a API_PORT  web api port. default is 7777.

Using systemd
-------------

`spp-ctl` is assumed to be launched as a daemon process, or managed
by `systemd`.
Here is a simple example of service file for systemd.

::

    [Unit]
    Description = SPP Controller

    [Service]
    ExecStart = /usr/bin/python3 /path/to/spp/src/spp-ctl/spp-ctl
    User = root


REST APIs
=========

You can try to call ``spp-ctl`` APIs with ``curl`` command as following.

.. code-block:: console

    $ curl http://localhost:7777/v1/processes
    [{"type": "primary"}, ..., {"client-id": 2, "type": "vf"}]
    $ curl http://localhost:7777/v1/vfs/1
    ... snip
    $ curl -X POST http://localhost:7777/v1/vfs/1/components \
      -d '{"core": 2, "name": "forward_0_tx", "type": "forward"}'

For more details, see
:ref:`API Reference<spp_ctl_api_ref>`.
