/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 IGEL Co., Ltd.
 * Copyright(c) 2016-2018 Intel Corporation
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#include <rte_mbuf.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_vdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_bus_vdev.h>
#include <rte_kvargs.h>
#include <rte_vhost.h>
#include <rte_spinlock.h>

static int vhost_logtype;

#define VHOST_LOG(level, ...) \
	rte_log(RTE_LOG_ ## level, vhost_logtype, __VA_ARGS__)

enum {VIRTIO_RXQ, VIRTIO_TXQ, VIRTIO_QNUM};

#define ETH_VHOST_IFACE_ARG		"iface"
#define ETH_VHOST_QUEUES_ARG		"queues"
#define ETH_VHOST_CLIENT_ARG		"client"

static const char *valid_arguments[] = {
	ETH_VHOST_IFACE_ARG,
	ETH_VHOST_QUEUES_ARG,
	ETH_VHOST_CLIENT_ARG,
	NULL
};

struct vhost_queue {
	struct pmd_internal *internal;
	struct rte_mempool *mb_pool;
	uint16_t virtqueue_id;
	rte_spinlock_t queuing_lock;
	uint64_t pkts;
	uint64_t missed_pkts;
};

struct pmd_internal {
	uint16_t max_queues;
	uint64_t vhost_flags;
	struct rte_eth_dev_data *eth_dev_data;
	int vid;
	char iface_name[PATH_MAX];
};

static struct rte_eth_link pmd_link = {
	.link_speed = 10000,
	.link_duplex = ETH_LINK_FULL_DUPLEX,
	.link_status = ETH_LINK_DOWN
};

#define MZ_RTE_VHOST_PMD_INTERNAL "vhost_pmd_internal"
static struct pmd_internal **pmd_internal_list;

static uint16_t
eth_vhost_rx(void *q, struct rte_mbuf **bufs, uint16_t nb_bufs)
{
	struct vhost_queue *r = q;
	struct pmd_internal *internal;
	uint16_t nb_rx = 0;

	if (!q)
		return 0;

	internal = r->internal;
	rte_spinlock_lock(&r->queuing_lock);
	if (internal->vid == -1)
		goto out;

	/* Dequeue packets from guest TX queue */
	nb_rx = rte_vhost_dequeue_burst(internal->vid, r->virtqueue_id,
				        r->mb_pool, bufs, nb_bufs);
	r->pkts += nb_rx;

out:
	rte_spinlock_unlock(&r->queuing_lock);

	return nb_rx;
}

static uint16_t
eth_vhost_tx(void *q, struct rte_mbuf **bufs, uint16_t nb_bufs)
{
	struct vhost_queue *r = q;
	struct pmd_internal *internal;
	uint16_t i, nb_tx = 0;

	if (!q)
		return 0;

	internal = r->internal;
	rte_spinlock_lock(&r->queuing_lock);
	if (internal->vid == -1)
		goto out;

	/* Enqueue packets to guest RX queue */
	nb_tx = rte_vhost_enqueue_burst(internal->vid, r->virtqueue_id,
				        bufs, nb_bufs);
	r->pkts += nb_tx;
	r->missed_pkts += nb_bufs - nb_tx;

	for (i = 0; i < nb_tx; i++)
		rte_pktmbuf_free(bufs[i]);

out:
	rte_spinlock_unlock(&r->queuing_lock);

	return nb_tx;
}

static inline struct pmd_internal *
find_internal_resource(int vid)
{
	struct pmd_internal *internal;
	int i;
	char ifname[PATH_MAX];

	if (rte_vhost_get_ifname(vid, ifname, sizeof(ifname)) == -1)
		return NULL;

	/* likely(found in a few loops) */
	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		internal = pmd_internal_list[i];
		if (internal != NULL && !strcmp(internal->iface_name, ifname)) {
			return internal;
		}
	}

	return NULL;
}

static int
new_device(int vid)
{
	struct pmd_internal *internal;

	internal = find_internal_resource(vid);
	if (internal == NULL) {
		VHOST_LOG(INFO, "Invalid device : %d\n", vid);
		return -1;
	}

	internal->vid = vid;
	internal->eth_dev_data->dev_link.link_status = ETH_LINK_UP;

	VHOST_LOG(INFO, "Vhost device %d created\n", vid);

	return 0;
}

static void
destroy_device(int vid)
{
	struct pmd_internal *internal;
	struct vhost_queue *vq;
	struct rte_eth_dev_data *data;
	int i;

	internal = find_internal_resource(vid);
	if (internal == NULL) {
		VHOST_LOG(ERR, "Invalid device : %d\n", vid);
		return;
	}
	data = internal->eth_dev_data;

	/* wait inflight queuing done */
	for (i = 0; i < data->nb_rx_queues; i++) {
		if ((vq = data->rx_queues[i]) != NULL)
			rte_spinlock_lock(&vq->queuing_lock);
	}
	for (i = 0; i < data->nb_tx_queues; i++) {
		if ((vq = data->tx_queues[i]) != NULL)
			rte_spinlock_lock(&vq->queuing_lock);
	}

	data->dev_link.link_status = ETH_LINK_DOWN;
	internal->vid = -1;

	for (i = 0; i < data->nb_rx_queues; i++) {
		if ((vq = data->rx_queues[i]) != NULL)
			rte_spinlock_unlock(&vq->queuing_lock);
	}
	for (i = 0; i < data->nb_tx_queues; i++) {
		if ((vq = data->tx_queues[i]) != NULL)
			rte_spinlock_unlock(&vq->queuing_lock);
	}

	VHOST_LOG(INFO, "Vhost device %d destroyed\n", vid);
}

static struct vhost_device_ops vhost_ops = {
	.new_device          = new_device,
	.destroy_device      = destroy_device,
};

static int
eth_dev_start(struct rte_eth_dev *eth_dev)
{
	struct pmd_internal *internal = eth_dev->data->dev_private;

	if (rte_vhost_driver_register(internal->iface_name, internal->vhost_flags))
		return -1;

	if (rte_vhost_driver_callback_register(internal->iface_name, &vhost_ops) < 0) {
		VHOST_LOG(ERR, "Can't register callbacks\n");
		return -1;
	}

	if (rte_vhost_driver_start(internal->iface_name) < 0) {
		VHOST_LOG(ERR, "Failed to start driver for %s\n",
			internal->iface_name);
		return -1;
	}

	return 0;
}

static void
eth_dev_stop(struct rte_eth_dev *dev)
{
	struct pmd_internal *internal = dev->data->dev_private;

	rte_vhost_driver_unregister(internal->iface_name);
}

static int
eth_dev_configure(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

static int
eth_dev_info(struct rte_eth_dev *dev,
	     struct rte_eth_dev_info *dev_info)
{
	struct pmd_internal *internal = dev->data->dev_private;

	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_pktlen = (uint32_t)-1;
	dev_info->max_rx_queues = internal->max_queues;
	dev_info->max_tx_queues = internal->max_queues;
	dev_info->min_rx_bufsize = 0;

	return 0;
}

static int
eth_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		   uint16_t nb_rx_desc __rte_unused,
		   unsigned int socket_id,
		   const struct rte_eth_rxconf *rx_conf __rte_unused,
		   struct rte_mempool *mb_pool)
{
	struct pmd_internal *internal = dev->data->dev_private;
	struct vhost_queue *vq;

	vq = rte_zmalloc_socket(NULL, sizeof(struct vhost_queue),
			RTE_CACHE_LINE_SIZE, socket_id);
	if (vq == NULL) {
		VHOST_LOG(ERR, "Failed to allocate memory for rx queue\n");
		return -ENOMEM;
	}

	vq->internal = internal;
	vq->mb_pool = mb_pool;
	vq->virtqueue_id = rx_queue_id * VIRTIO_QNUM + VIRTIO_TXQ;
	rte_spinlock_init(&vq->queuing_lock);
	dev->data->rx_queues[rx_queue_id] = vq;

	return 0;
}

static int
eth_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		   uint16_t nb_tx_desc __rte_unused,
		   unsigned int socket_id,
		   const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct pmd_internal *internal = dev->data->dev_private;
	struct vhost_queue *vq;

	vq = rte_zmalloc_socket(NULL, sizeof(struct vhost_queue),
			RTE_CACHE_LINE_SIZE, socket_id);
	if (vq == NULL) {
		VHOST_LOG(ERR, "Failed to allocate memory for tx queue\n");
		return -ENOMEM;
	}

	vq->internal = internal;
	vq->virtqueue_id = tx_queue_id * VIRTIO_QNUM + VIRTIO_RXQ;
	rte_spinlock_init(&vq->queuing_lock);
	dev->data->tx_queues[tx_queue_id] = vq;

	return 0;
}

static void
eth_queue_release(void *q)
{
	rte_free(q);
}

static int
eth_link_update(struct rte_eth_dev *dev __rte_unused,
		int wait_to_complete __rte_unused)
{
	return 0;
}

static int
eth_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	unsigned i;
	unsigned long rx_total = 0, tx_total = 0, tx_err_total = 0;
	struct vhost_queue *vq;

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS &&
			i < dev->data->nb_rx_queues; i++) {
		if ((vq = dev->data->rx_queues[i]) != NULL) {
			stats->q_ipackets[i] = vq->pkts;
			rx_total += vq->pkts;
		}
	}

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS &&
			i < dev->data->nb_tx_queues; i++) {
		if ((vq = dev->data->rx_queues[i]) != NULL) {
			stats->q_opackets[i] = vq->pkts;
			tx_total += vq->pkts;
			stats->q_errors[i] = vq->missed_pkts;
			tx_err_total += vq->missed_pkts;
		}
	}

	stats->ipackets = rx_total;
	stats->opackets = tx_total;
	stats->oerrors = tx_err_total;

	return 0;
}

static int
eth_stats_reset(struct rte_eth_dev *dev)
{
	struct vhost_queue *vq;
	unsigned i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if ((vq = dev->data->rx_queues[i]) != NULL)
			vq->pkts = 0;
	}
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		if ((vq = dev->data->rx_queues[i]) != NULL) {
			vq->pkts = 0;
			vq->missed_pkts = 0;
		}
	}

	return 0;
}

static const struct eth_dev_ops ops = {
	.dev_start = eth_dev_start,
	.dev_stop = eth_dev_stop,
	.dev_configure = eth_dev_configure,
	.dev_infos_get = eth_dev_info,
	.rx_queue_setup = eth_rx_queue_setup,
	.tx_queue_setup = eth_tx_queue_setup,
	.rx_queue_release = eth_queue_release,
	.tx_queue_release = eth_queue_release,
	.link_update = eth_link_update,
	.stats_get = eth_stats_get,
	.stats_reset = eth_stats_reset,
};

static int
init_shared_data(void)
{
	const struct rte_memzone *mz;

	if (pmd_internal_list == NULL) {
		if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
			size_t len = sizeof(*pmd_internal_list) * RTE_MAX_ETHPORTS;
			mz = rte_memzone_reserve(MZ_RTE_VHOST_PMD_INTERNAL,
					len, rte_socket_id(), 0);
			if (mz)
				memset(mz->addr, 0, len);
		} else
			mz = rte_memzone_lookup(MZ_RTE_VHOST_PMD_INTERNAL);
		if (mz == NULL) {
			VHOST_LOG(ERR, "Cannot allocate vhost shared data\n");
			return -1;
		}
		pmd_internal_list = mz->addr;
	}

	return 0;
}

static int
eth_dev_vhost_create(struct rte_vdev_device *dev, char *iface_name,
	int16_t queues, const unsigned int numa_node, uint64_t flags)
{
	struct rte_eth_dev_data *data;
	struct pmd_internal *internal;
	struct rte_eth_dev *eth_dev;
	struct rte_ether_addr *eth_addr;

	VHOST_LOG(INFO, "Creating VHOST-USER backend on numa socket %u\n",
		numa_node);

	eth_addr = rte_zmalloc_socket(NULL, sizeof(*eth_addr), 0, numa_node);
	if (eth_addr == NULL)
		return -ENOMEM;

	eth_dev = rte_eth_vdev_allocate(dev, sizeof(*internal));
	if (eth_dev == NULL)
		return -ENOMEM;

	data = eth_dev->data;

	data->dev_link = pmd_link;
	data->dev_flags = RTE_ETH_DEV_INTR_LSC;
	data->mac_addrs = eth_addr;

	eth_dev->dev_ops = &ops;
	eth_dev->rx_pkt_burst = eth_vhost_rx;
	eth_dev->tx_pkt_burst = eth_vhost_tx;

	internal = data->dev_private;

	internal->max_queues = queues;
	internal->vid = -1;
	internal->vhost_flags = flags;
	internal->eth_dev_data = data;
	strncpy(internal->iface_name, iface_name, sizeof(internal->iface_name));

	pmd_internal_list[data->port_id] = internal;

	rte_eth_dev_probing_finish(eth_dev);

	return 0;
}

static inline int
open_iface(const char *key __rte_unused, const char *value, void *extra_args)
{
	const char **iface_name = extra_args;

	if (value == NULL)
		return -1;

	*iface_name = value;

	return 0;
}

static inline int
open_int(const char *key __rte_unused, const char *value, void *extra_args)
{
	uint16_t *n = extra_args;
	char *endptr;

	if (value == NULL)
		return -1;

	*n = (uint16_t)strtoul(value, &endptr, 0);
	if (*endptr != '\0' || errno == ERANGE)
		return -1;

	return 0;
}

static int
rte_pmd_vhost_probe(struct rte_vdev_device *dev)
{
	struct rte_kvargs *kvlist = NULL;
	int ret = 0;
	char *iface_name = NULL;
	uint16_t queues = 1;
	uint64_t flags = 0;
	uint16_t client_mode = 0;
	struct rte_eth_dev *eth_dev;
	const char *name = rte_vdev_device_name(dev);

	VHOST_LOG(INFO, "Initializing pmd_vhost for %s\n", name);

	if (init_shared_data() == -1)
		return -ENOMEM;

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		eth_dev = rte_eth_dev_attach_secondary(name);
		if (!eth_dev) {
			VHOST_LOG(ERR, "Failed to probe %s\n", name);
			return -ENOENT;
		}
		eth_dev->rx_pkt_burst = eth_vhost_rx;
		eth_dev->tx_pkt_burst = eth_vhost_tx;

		eth_dev->dev_ops = &ops;
		eth_dev->device = &dev->device;
		rte_eth_dev_probing_finish(eth_dev);
		return 0;
	}

	kvlist = rte_kvargs_parse(rte_vdev_device_args(dev), valid_arguments);
	if (kvlist == NULL)
		return -EINVAL;

	if (rte_kvargs_process(kvlist, ETH_VHOST_IFACE_ARG,
			&open_iface, &iface_name) == -1 ||
	    rte_kvargs_process(kvlist, ETH_VHOST_QUEUES_ARG,
			&open_int, &queues) == -1 ||
	    rte_kvargs_process(kvlist, ETH_VHOST_CLIENT_ARG,
			&open_int, &client_mode) == -1) {
		rte_kvargs_free(kvlist);
		return -EINVAL;
	}

	if (iface_name == NULL ||
	    queues == 0 || queues > RTE_MAX_QUEUES_PER_PORT ||
	    (client_mode != 0 && client_mode != 1)) {
		rte_kvargs_free(kvlist);
		return -EINVAL;
	}

	if (client_mode)
		flags |= RTE_VHOST_USER_CLIENT;

	ret = eth_dev_vhost_create(dev, iface_name, queues, rte_socket_id(), flags);

	rte_kvargs_free(kvlist);
	return ret;
}

static int
rte_pmd_vhost_remove(struct rte_vdev_device *dev)
{
	const char *name = rte_vdev_device_name(dev);
	struct rte_eth_dev *eth_dev;
	int i;

	VHOST_LOG(INFO, "Un-Initializing pmd_vhost for %s\n", name);

	eth_dev = rte_eth_dev_allocated(name);
	if (eth_dev == NULL)
		return 0;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return rte_eth_dev_release_port(eth_dev);

	if (eth_dev->data->dev_started) {
		VHOST_LOG(WARNING, "device must be stoped.\n");
	}

	for (i = 0; i < eth_dev->data->nb_rx_queues; i++)
		rte_free(eth_dev->data->rx_queues[i]);

	for (i = 0; i < eth_dev->data->nb_tx_queues; i++)
		rte_free(eth_dev->data->tx_queues[i]);

	pmd_internal_list[eth_dev->data->port_id] = NULL;

	return rte_eth_dev_release_port(eth_dev);
}

static struct rte_vdev_driver pmd_vhost_drv = {
	.probe = rte_pmd_vhost_probe,
	.remove = rte_pmd_vhost_remove,
};

RTE_PMD_REGISTER_VDEV(spp_vhost, pmd_vhost_drv);
RTE_PMD_REGISTER_PARAM_STRING(spp_vhost,
	"iface=<ifc> "
	"queues=<int> "
	"client=<0|1> ");

RTE_INIT(vhost_init_log)
{
	vhost_logtype = rte_log_register("pmd.spp.vhost");
	if (vhost_logtype >= 0)
		rte_log_set_level(vhost_logtype, RTE_LOG_NOTICE);
}
