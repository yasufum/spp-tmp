# Soft Patch Panel


## Overview

[Soft Patch Panel](http://git.dpdk.org/apps/spp/)
(SPP) is a DPDK application for providing switching
functionality for Service Function Chaining in NFV
(Network Function Virtualization).
DPDK stands for Data Plane Development Kit.


## Project Goal

In general, implementation and configuration of DPDK application is
difficult because it requires deep understandings of system architecture
and networking technologies.

The goal of SPP is to easily inter-connect DPDK applications together
and assign resources dynamically to these applications
with patch panel like simple interface.


## Architecture Overview

The framework is composed of a primary DPDK application that is
responsible for resource management and secondary processes as workers
for packet forwarding. This primary application doesn't
interact with any traffic, and is used to manage creation and freeing of
resources only.

A Python based management interfaces, `spp-ctl` and `SPP CLI`,
are provided to control the primary DPDK application to create resources,
which are then to be used by secondary applications.

This management application provides a socket based interface for
the primary and secondary DPDK applications to
interface to the manager.


## Install

Before using SPP, you need to install DPDK. Briefly describ here how to install
and setup DPDK. Please refer to SPP's
[Getting Started](https://doc.dpdk.org/spp/setup/getting_started.html) guide
for more details. For DPDK, refer to
[Getting Started Guide for Linux](https://doc.dpdk.org/guides/linux_gsg/index.html).

### Install DPDK

It is required to install Python and libnuma-devel library before.

```sh
$ sudo apt install libnuma-dev

# Python2
$ sudo apt install python python-pip

# Python3
$ sudo apt install python3 python3-pip
```

Clone repository and compile DPDK in any directory.

```
$ cd /path/to/any
$ git clone http://dpdk.org/git/dpdk
```

Compile DPDK with target environment.

```sh
$ cd dpdk
$ export RTE_SDK=$(pwd)
$ export RTE_TARGET=x86_64-native-linuxapp-gcc  # depends on your env
$ make install T=$RTE_TARGET
```

### Install SPP

Clone repository and compile SPP in any directory.

```sh
$ cd /path/to/any
$ git clone http://dpdk.org/git/apps/spp
$ cd spp
$ make  # Confirm that $RTE_SDK and $RTE_TARGET are set
```

### Binding Network Ports to DPDK

Network ports must be bound to DPDK with a UIO (Userspace IO) driver. UIO driver
is for mapping device memory to userspace and registering interrupts.

You usually use the standard `uio_pci_generic` for many use cases or `vfio-pci`
for more robust and secure cases. Both of drivers are included by default in
modern Linux kernel.

```sh
# Activate uio_pci_generic
$ sudo modprobe uio_pci_generic

# or vfio-pci
$ sudo modprobe vfio-pci
```

Once UIO driver is activated, bind network ports with the driver. DPDK provides
`usertools/dpdk-devbind.py` for managing devices.

```
# Bind a port with 2a:00.0 (PCI address)
$ ./usertools/dpdk-devbind.py --bind=uio_pci_generic 2a:00.0

# or eth0
$ ./usertools/dpdk-devbind.py --bind=uio_pci_generic eth0
```

After binding two ports, you can find it is under the DPDK driver, and cannot
find it by using `ifconfig` or `ip`.

```sh
$ $RTE_SDK/usertools/dpdk-devbind.py -s

Network devices using DPDK-compatible driver
============================================
0000:2a:00.0 '82571EB ... 10bc' drv=uio_pci_generic unused=vfio-pci
....
```

## How to Use

You should keep in mind the order of launching processes. Primary process must
be launched before secondary processes. `spp-ctl` need to be launched before
`spp.py`, but no need to be launched before other processes.
In general, `spp-ctl` should be launched first, then `spp.py` and `spp_primary`
in each of terminals without running as background process.

It has a option -b for binding address explicitly to be accessed from other
than default, `127.0.0.1` or `localhost`.


### SPP Controller

SPP controller consists of `spp-ctl` and SPP CLI.
`spp-ctl` is a HTTP server for REST APIs for managing SPP processes.

```sh
# terminal 1
$ cd /path/to/spp
$ python3 src/spp-ctl/spp-ctl -b 192.168.1.100
```

SPP CLI is a client of `spp-ctl` for providing simple user interface without
using REST APIs.

```sh
# terminal 2
$ cd /path/to/spp
$ python3 src/spp.py -b 192.168.1.100
```


### SPP Primary

Launch SPP primary and secondary processes.
SPP primary is a resource manager and initializing EAL for secondary processes.
Secondary process behaves as a client of primary process and a worker for doing
tasks.

```sh
# terminal 3
$ sudo ./src/primary/x86_64-native-linuxapp-gcc/spp_primary \
    -l 1 -n 4 \
    --socket-mem 512,512 \
    --huge-dir=/dev/hugepages \
    --proc-type=primary \
    -- \
    -p 0x03 \
    -n 10 \
    -s 192.168.1.100:5555
```

There are several kinds of secondary process. Here is an example of the simplest
one.

```sh
# terminal 4
$ cd /path/to/spp
$ sudo ./src/nfv/x86_64-native-linuxapp-gcc/spp_nfv \
    -l 2-3 -n 4 \
    --proc-type=secondary \
    -- \
    -n 1 \
    -s 192.168.1.100:6666
```

After all of SPP processes are launched, configure network path from SPP CLI.
Please refer to SPP
[Use Cases](https://doc.dpdk.org/spp/setup/use_cases.html)
for the configuration.
