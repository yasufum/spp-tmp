.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

.. _spp_vf_explain_spp_mirror:

spp_mirror
==========

Initializing
------------

A main thread of ``spp_mirror`` initialize eal by ``rte_eal_init()``.
Then each of worker threads is launched from ``rte_eal_remote_launch()``
by giving a function ``slave_main()`` for forwarding.

.. code-block:: c

    /* spp_mirror.c */
    int ret_dpdk = rte_eal_init(argc, argv);

    /* Start worker threads of classifier and forwarder */
    unsigned int lcore_id = 0;
    RTE_LCORE_FOREACH_SLAVE(lcore_id) {
            rte_eal_remote_launch(slave_main, NULL, lcore_id);
    }


Main function of slave thread
-----------------------------

In ``slave_main()``, it calls ``mirror_proc()`` in which packet processing for
duplicating is defined after finding a core on which running the duplicating.

.. code-block:: c

	RTE_LOG(INFO, MIRROR, "Core[%d] Start.\n", lcore_id);
	set_core_status(lcore_id, SPP_CORE_IDLE);

	while ((status = spp_get_core_status(lcore_id)) !=
			SPP_CORE_STOP_REQUEST) {
		if (status != SPP_CORE_FORWARD)
			continue;

		if (spp_check_core_index(lcore_id)) {
			/* Setting with the flush command trigger. */
			info->ref_index = (info->upd_index+1) %
					SPP_INFO_AREA_MAX;
			core = get_core_info(lcore_id);
		}

		for (cnt = 0; cnt < core->num; cnt++) {
			/*
			 * mirror returns at once.
			 * It is for processing multiple components.
			 */
			ret = mirror_proc(core->id[cnt]);
			if (unlikely(ret != 0))
				break;
		}
		if (unlikely(ret != 0)) {
			RTE_LOG(ERR, MIRROR,
				"Core[%d] Component Error. (id = %d)\n",
					lcore_id, core->id[cnt]);
			break;
		}
	}

	set_core_status(lcore_id, SPP_CORE_STOP);
	RTE_LOG(INFO, MIRROR, "Core[%d] End.\n", lcore_id);

Packet mirroring
----------------

In ``mirror_proc()``, it receives packets from rx port.

.. code-block:: c

        /* Receive packets */
        nb_rx = spp_eth_rx_burst(rx->dpdk_port, 0, bufs, MAX_PKT_BURST);

Each of received packet is copied with ``rte_pktmbuf_clone()`` if you use
``shallowcopy`` defined as default in Makefile.
If you use ``deepcopy``, several mbuf objects are allocated for copying.

.. code-block:: c

                for (cnt = 0; cnt < nb_rx; cnt++) {
                        org_mbuf = bufs[cnt];
                        rte_prefetch0(rte_pktmbuf_mtod(org_mbuf, void *));
   #ifdef SPP_MIRROR_SHALLOWCOPY
                        /* Shallow Copy */
			copybufs[cnt] = rte_pktmbuf_clone(org_mbuf,
                                                        g_mirror_pool);

   #else
                        struct rte_mbuf *mirror_mbuf = NULL;
                        struct rte_mbuf **mirror_mbufs = &mirror_mbuf;
                        struct rte_mbuf *copy_mbuf = NULL;
                        /* Deep Copy */
                        do {
                                copy_mbuf = rte_pktmbuf_alloc(g_mirror_pool);
                                if (unlikely(copy_mbuf == NULL)) {
                                        rte_pktmbuf_free(mirror_mbuf);
                                        mirror_mbuf = NULL;
                                        RTE_LOG(INFO, MIRROR,
                                                "copy mbuf alloc NG!\n");
                                        break;
                                }

                                copy_mbuf->data_off = org_mbuf->data_off;
                                ...
                                copy_mbuf->packet_type = org_mbuf->packet_type;

                                rte_memcpy(rte_pktmbuf_mtod(copy_mbuf, char *),
                                        rte_pktmbuf_mtod(org_mbuf, char *),
                                        org_mbuf->data_len);

                                *mirror_mbufs = copy_mbuf;
                                mirror_mbufs = &copy_mbuf->next;
                        } while ((org_mbuf = org_mbuf->next) != NULL);
			copybufs[cnt] = mirror_mbuf;

   #endif /* SPP_MIRROR_SHALLOWCOPY */
                }
		if (cnt != 0)
                        nb_tx2 = spp_eth_tx_burst(tx->dpdk_port, 0,
								copybufs, cnt);
