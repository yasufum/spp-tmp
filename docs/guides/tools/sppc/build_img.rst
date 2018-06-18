..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation

.. _spp_container_build_img:

Build Images
============

As explained in :doc:`getting_started` section,
container image is built with ``build/main.py``.
This script is for running ``docker build`` with a set of
``--build-args`` options for building DPDK applications.

This script supports building application from any of repositories.
For example, you can build SPP hosted on your repository
``https://github.com/your/spp.git``
with DPDK 18.02 as following.

.. code-block:: console

    $ cd /path/to/spp/tools/sppc
    $ python build/build.py --dpdk-branch v18.02 \
      --spp-repo https://github.com/your/spp.git

Refer all of options running with ``-h`` option.

.. code-block:: console

    $ python build/main.py -h
    usage: main.py [-h] [-t TARGET] [-ci CONTAINER_IMAGE]
                   [--dist-name DIST_NAME]
                   [--dist-ver DIST_VER] [--dpdk-repo DPDK_REPO]
                   [--dpdk-branch DPDK_BRANCH] [--pktgen-repo PKTGEN_REPO]
                   [--pktgen-branch PKTGEN_BRANCH] [--spp-repo SPP_REPO]
                   [--spp-branch SPP_BRANCH] [--only-envsh] [--dry-run]

    Docker image builder for DPDK applications

    optional arguments:
      -h, --help            show this help message and exit
      -t TARGET, --target TARGET
                            Build target ('dpdk', 'pktgen' or 'spp')
      -ci CONTAINER_IMAGE, --container-image CONTAINER_IMAGE
                            Name of container image
      --dist-name DIST_NAME
                            Name of Linux distribution
      --dist-ver DIST_VER   Version of Linux distribution
      --dpdk-repo DPDK_REPO
                            Git URL of DPDK
      --dpdk-branch DPDK_BRANCH
                            Specific version or branch of DPDK
      --pktgen-repo PKTGEN_REPO
                            Git URL of pktgen-dpdk
      --pktgen-branch PKTGEN_BRANCH
                            Specific version or branch of pktgen-dpdk
      --spp-repo SPP_REPO   Git URL of SPP
      --spp-branch SPP_BRANCH
                            Specific version or branch of SPP
      --only-envsh          Create config 'env.sh' and exit without docker build
      --dry-run             Print matrix for checking and exit without docker
                            build


.. _sppc_build_img_vci:

Version Control for Images
~~~~~~~~~~~~~~~~~~~~~~~~~~

SPP container provides version control as combination of
target name, Linux distribution name and version.
Built images are referred such as ``sppc/dpdk-ubuntu:latest`` or
``sppc/spp-ubuntu:16.04``.
``sppc`` is just a prefix to indicate an image of SPP container.

Build script decides a name from given options or default values.
If you run build script with only target and without distribution
name and version, it uses default values ``ubuntu`` and ``latest``.

.. code-block:: console

    # build 'sppc/dpdk-ubuntu:latest'
    $ python build/main.py -t dpdk

    # build 'sppc/spp-ubuntu:16.04'
    $ python build/main.py -t spp --dist-ver 16.04

.. note::

    SPP container does not support distributions other than Ubuntu
    currently.
    It is because SPP container has no Dockerfiles for building
    CentOS, Fedora or so. It will be supported in a future release.

    You can build any of distributions with build script
    if you prepare Dockerfile by yourself.
    How Dockerfiles are managed is described in
    :ref:`sppc_build_img_dockerfiles` section.


App container scripts also understand this naming rule.
For launching ``testpmd`` on Ubuntu 16.04,
simply give ``--dist-ver`` to indicate the version and other options
for ``testpmd`` itself.

.. code-block:: console

    # launch testpmd on 'sppc/dpdk-ubuntu:16.04'
    $ python app/testpmd.py --dist-ver 16.04 -l 3-4 ...

But, how can we build images for different versions of DPDK,
such as 17.11 and 18.05, on the same distribution?
In this case, you can use ``--container-image`` or ``-ci`` option for
using any of names. It is also referred from app container scripts.

.. code-block:: console

    # build image with arbitrary name
    $ python build/main.py -t dpdk -ci sppc/dpdk17.11-ubuntu:latest \
      --dpdk-branch v17.11

    # launch testpmd with '-ci'
    $ python app/testpmd.py -ci sppc/dpdk17.11-ubuntu:latest -l 3-4 ...


.. _sppc_build_img_dockerfiles:

Dockerfiles
~~~~~~~~~~~

SPP container includes Dockerfiles for each of distributions and
its versions.
For instance, Dockerfiles for Ubuntu are found in ``build/ubuntu``
directory.
You notice that each of Dockerfiles has its version as a part of
file name.
In other words, the list of Dockerfiles under the ``ubuntu`` directory
shows all of supported versions of Ubuntu.
You can not find Dockerfiles for CentOS as ``build/centos`` or other
distributions because it is not supported currently.
It is included in a future release.

.. code-block:: console

    $ tree build/ubuntu/
    build/ubuntu/
    ├── dpdk
    │   ├── Dockerfile.16.04
    │   ├── Dockerfile.18.04
    │   └── Dockerfile.latest
    ├── pktgen
    │   ├── Dockerfile.16.04
    │   ├── Dockerfile.18.04
    │   └── Dockerfile.latest
    └── spp
        ├── Dockerfile.16.04
        ├── Dockerfile.18.04
        └── Dockerfile.latest


.. _sppc_build_img_inspect:

Inspect Inside of Container
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Container is useful, but just bit annoying to inspect inside
the container because it is cleaned up immediately after process
is finished and there is no clue what is happened in.

``build/run.sh`` is a helper script to inspect inside the container.
You can run ``bash`` on the container to confirm behaviour of
targetting application, or run any of command.

This script refers ``ubuntu/dpdk/env.sh`` for Ubuntu image  to include
environment variables.
So, it is failed to ``build/run.sh`` if this config file
does not exist.
You can create it from ``build/build.py`` with ``--only-envsh`` option
if you removed it accidentally.
