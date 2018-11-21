/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Nippon Telegraph and Telephone Corporation
 */

#ifndef _SPP_PROC_H_
#define _SPP_PROC_H_

/**
 * @file
 * SPP process
 *
 * SPP component common function.
 */

#include <netinet/in.h>
#include "common.h"

/**
 * Make a hexdump of an array data in every 4 byte.
 * This function is used to dump core_info or component info.
 *
 * @param name
 *  dump name.
 * @param addr
 *  dump address.
 * @param size
 *  dump byte size.
 *
 */
void dump_buff(const char *name, const void *addr, const size_t size);

/**
 * added ring_pmd
 *
 * @param ring_id
 *  added ring id.
 *
 * @retval 0~   ring_port_id.
 * @retval -1   failed.
 */
int add_ring_pmd(int ring_id);

/**
 * added vhost_pmd
 *
 * @param index
 *  add vohst id.
 * @param client
 *  add client id.
 *
 * @retval 0~   vhost_port_id.
 * @retval -1   failed.
 */
int add_vhost_pmd(int index, int client);

/**
 * Get core status
 *
 * @param lcore_id
 *  Logical core ID.
 *
 * @return
 *  Status of specified logical core.
 */
enum spp_core_status spp_get_core_status(unsigned int lcore_id);

/**
 * Get component type of target core
 *
 * @param lcore_id
 *  Logical core ID.
 *
 * @return
 *  Type of component executed on specified logical core
 */
enum spp_component_type spp_get_component_type(unsigned int lcore_id);

/**
 * Run check_core_status() for SPP_CORE_STATUS_CHECK_MAX times with
 * interval time (1sec)
 *
 * @param status
 *  wait check status.
 *
 * @retval 0  succeeded.
 * @retval -1 failed.
 */
int check_core_status_wait(enum spp_core_status status);

/**
 * Set core status
 *
 * @param lcore_id
 *  Logical core ID.
 * @param status
 *  set status.
 *
 */
void set_core_status(unsigned int lcore_id, enum spp_core_status status);

/**
 * Set all core status to given
 *
 * @param status
 *  set status.
 *
 */
void set_all_core_status(enum spp_core_status status);

/**
 * Set all of component status to SPP_CORE_STOP_REQUEST if received signal
 * is SIGTERM or SIGINT
 *
 * @param signl
 *  received signal.
 *
 */
void stop_process(int signal);

/**
 * Return port info of given type and num of interface
 *
 * @param iface_type
 *  Interface type to be validated.
 * @param iface_no
 *  Interface number to be validated.
 *
 * @retval !NULL  spp_port_info.
 * @retval NULL   failed.
 */
struct spp_port_info *
get_iface_info(enum port_type iface_type, int iface_no);

/* Dump of core information */
void dump_core_info(const struct core_mng_info *core_info);

/* Dump of component information */
void dump_component_info(const struct spp_component_info *component_info);

/* Dump of interface information */
void dump_interface_info(const struct iface_info *iface_info);

/* Dump of all management information */
void dump_all_mng_info(
		const struct core_mng_info *core,
		const struct spp_component_info *component,
		const struct iface_info *interface);

/* Copy management information */
void copy_mng_info(
		struct core_mng_info *dst_core,
		struct spp_component_info *dst_component,
		struct iface_info *dst_interface,
		const struct core_mng_info *src_core,
		const struct spp_component_info *src_component,
		const struct iface_info *src_interface,
		enum copy_mng_flg flg);

/* Backup the management information */
void backup_mng_info(struct cancel_backup_info *backup);

/**
 * Setup management info for spp_vf
 */
int init_mng_data(void);

#ifdef SPP_RINGLATENCYSTATS_ENABLE
/**
 * Print statistics of time for packet processing in ring interface
 */
void print_ring_latency_stats(void);
#endif /* SPP_RINGLATENCYSTATS_ENABLE */

/* Remove sock file if spp is not running */
void  del_vhost_sockfile(struct spp_port_info *vhost);

/**
 * Get component type of target core
 *
 * @param lcore_id
 *  Logical core ID.
 *
 * @return
 *  Type of component executed on specified logical core
 */
enum spp_component_type
spp_get_component_type(unsigned int lcore_id);

/**
 * Get core ID of target component
 *
 * @param component_id
 *  unique component ID.
 *
 * @return
 *  Logical core id of specified component.
 */
unsigned int spp_get_component_core(int component_id);

/* Get core information which is in use */
struct core_info *get_core_info(unsigned int lcore_id);

/**
 * Check core index change
 *
 * @param lcore_id
 *  Logical core ID.
 *
 *  True if index has changed.
 * @retval SPP_RET_OK index has changed.
 * @retval SPP_RET_NG index not changed.
 */
int spp_check_core_update(unsigned int lcore_id);

/**
 * Check if component is using port.
 *
 * @param iface_type
 *  Interface type to be validated.
 * @param iface_no
 *  Interface number to be validated.
 * @param rxtx
 *  tx/rx type to be validated.
 *
 * @retval 0~127      match component ID
 * @retval SPP_RET_NG failed.
 */
int spp_check_used_port(
		enum port_type iface_type,
		int iface_no,
		enum spp_port_rxtx rxtx);

/**
 * Set component update flag for given port.
 *
 * @param port
 *  spp_port_info address
 * @param rxtx
 *  enum spp_port_rxtx
 *
 */
void
set_component_change_port(struct spp_port_info *port, enum spp_port_rxtx rxtx);

/**
 * Get unused component id
 *
 * @retval 0~127 Component ID.
 * @retval -1    failed.
 */
int get_free_component(void);

/**
 * Get component id for specified component name
 *
 * @param name
 *  Component name.
 *
 * @retval 0~127      Component ID.
 * @retval SPP_RET_NG failed.
 */
int spp_get_component_id(const char *name);

/**
 *  Delete component information.
 *
 * @param component_id
 *  check data
 * @param component_num
 *  array check count
 * @param componet_array
 *  check array address
 *
 * @retval 0  succeeded.
 * @retval -1 failed.
 */
int
del_component_info(int component_id, int component_num, int *componet_array);

/**
 * get port element which matches the condition.
 *
 * @param info
 *  spp_port_info address
 * @param num
 *  port count
 * @param array[]
 *  spp_port_info array address
 *
 * @retval 0~ match index.
 * @retval -1 failed.
 */
int check_port_element(
		struct spp_port_info *info,
		int num,
		struct spp_port_info *array[]);

/**
 *  search matched port_info from array and delete it.
 *
 * @param info
 *  spp_port_info address
 * @param num
 *  port count
 * @param array[]
 *  spp_port_info array address
 *
 * @retval 0  succeeded.
 * @retval -1 failed.
 */
int get_del_port_element(
		struct spp_port_info *info,
		int num,
		struct spp_port_info *array[]);

/**
 * Flush initial setting of each interface.
 *
 * @retval SPP_RET_OK succeeded.
 * @retval SPP_RET_NG failed.
 */
int flush_port(void);

/**
 *  Flush changed core.
 */
void flush_core(void);

/**
 *  Flush change for forwarder or classifier_mac.
 *
 * @retval SPP_RET_OK succeeded.
 * @retval SPP_RET_NG failed.
 */
int flush_component(void);

/**
 * Port type to string
 *
 * @param port
 *  Character string of Port type to be converted.
 * @param iface_type
 *  port interface type
 * @param iface_no
 *  interface no
 *
 * @retval 0  succeeded.
 * @retval -1 failed.
 */
int
spp_format_port_string(char *port, enum port_type iface_type, int iface_no);

/**
 * Change mac address string to int64
 *
 * @param mac
 *  Character string of MAC address to be converted.
 *
 * @retval 0< int64 that store mac address
 * @retval -1
 */
int64_t spp_change_mac_str_to_int64(const char *mac);

#endif /* _SPP_PROC_H_ */
