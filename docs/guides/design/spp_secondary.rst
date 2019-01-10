..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Nippon Telegraph and Telephone Corporation


.. _spp_design_spp_secondary:

SPP Secondary
=============

SPP secondary process is a worker process in client-server multp-process
application model. Basically, the role of secondary process is to connenct
each of application running on host, containers or VMs for packet forwarding.
Spp secondary process forwards packets from source port to destination port
with DPDK's high-performance forwarding mechanizm. In other word, it behaves
as a cable to connect two patches ports.

All of secondary processes are able to attach ring PMD and vhost PMD ports
for sending or receiving packets with other processes. Ring port is used to
communicate with a process running on host or container if it is implemented
as secondary process to access shared ring memory.
Vhost port is used for a process on container or VM and implemented as primary
process, and no need to access shared memory of SPP primary.

In addition to the basic forwarding, SPP secondary process provides several
networking features. One of the typical example is packet cauture.
``spp_nfv`` is the simplest SPP secondary and used to connect two of processes
or other feature ports including PCAP PMD port. PCAP PMD is to dump packets to
a file or retrieve from.

There are more specific or funcional features than ``spp_nfv``. ``spp_vf`` is
a simple pseudo SR-IOV feature for classifying or merging packets.
``spp_mirror`` is to duplicate incoming packets to several destination ports.


.. _spp_design_spp_secondary_nfv:

spp_nfv
-------

``spp_nfv`` is ...
