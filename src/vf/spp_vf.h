/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation
 */

#ifndef __SPP_VF_H__
#define __SPP_VF_H__

/**
 * @file
 * SPP_VF main
 *
 * Main function of spp_vf.
 * This provides the function for initializing and starting the threads.
 */

#include "common.h"

/** Identifier string for each component (status command) @{*/
#define SPP_TYPE_CLASSIFIER_MAC_STR "classifier_mac"
#define SPP_TYPE_MERGE_STR          "merge"
#define SPP_TYPE_FORWARD_STR        "forward"
#define SPP_TYPE_UNUSE_STR          "unuse"
/**@}*/

/** Identifier string for each interface @{*/
#define SPP_IFTYPE_NIC_STR   "phy"
#define SPP_IFTYPE_VHOST_STR "vhost"
#define SPP_IFTYPE_RING_STR  "ring"
/**@}*/

/** The max number of client ID */
#define SPP_CLIENT_MAX    128

/** The max number of buffer for management */
#define SPP_INFO_AREA_MAX 2

/** The length of shortest character string */
#define SPP_MIN_STR_LEN   32

/** The length of NAME string */
#define SPP_NAME_STR_LEN  128

/** Update wait timer (micro sec) */
#define SPP_CHANGE_UPDATE_INTERVAL 10

/** Character sting for default port of classifier */
#define SPP_DEFAULT_CLASSIFIED_SPEC_STR     "default"

/** Character sting for default MAC address of classifier */
#define SPP_DEFAULT_CLASSIFIED_DMY_ADDR_STR "00:00:00:00:00:01"

/** Value for default MAC address of classifier */
#define SPP_DEFAULT_CLASSIFIED_DMY_ADDR     0x010000000000

/** Maximum number of port abilities available */
#define SPP_PORT_ABILITY_MAX 4

/** Number of VLAN ID */
#define SPP_NUM_VLAN_VID 4096

/** Maximum VLAN PCP */
#define SPP_VLAN_PCP_MAX 7

/**
 * State on component
 */
enum spp_core_status {
	SPP_CORE_UNUSE,        /**< Not used */
	SPP_CORE_STOP,         /**< Stopped */
	SPP_CORE_IDLE,         /**< Idling */
	SPP_CORE_FORWARD,      /**< Forwarding  */
	SPP_CORE_STOP_REQUEST, /**< Request stopping */
	SPP_CORE_IDLE_REQUEST  /**< Request idling */
};

/**
 * Process type for each component
 */
enum spp_component_type {
	SPP_COMPONENT_UNUSE,          /**< Not used */
	SPP_COMPONENT_CLASSIFIER_MAC, /**< Classifier_mac */
	SPP_COMPONENT_MERGE,          /**< Merger */
	SPP_COMPONENT_FORWARD,        /**< Forwarder */
};

/**
 * Classifier Type
 */
enum spp_classifier_type {
	SPP_CLASSIFIER_TYPE_NONE, /**< Type none */
	SPP_CLASSIFIER_TYPE_MAC,  /**< MAC address */
	SPP_CLASSIFIER_TYPE_VLAN  /**< VLAN ID */
};

/**
 * API Return value
 */
enum spp_return_value {
	SPP_RET_OK = 0,  /**< succeeded */
	SPP_RET_NG = -1, /**< failed */
};

/** Port type (rx or tx) */
enum spp_port_rxtx {
	SPP_PORT_RXTX_NONE, /**< none */
	SPP_PORT_RXTX_RX,   /**< rx port */
	SPP_PORT_RXTX_TX,   /**< tx port */
	SPP_PORT_RXTX_ALL,  /**< rx/tx port */
};

/** command setting type */
enum spp_command_action {
	SPP_CMD_ACTION_NONE,  /**< none */
	SPP_CMD_ACTION_START, /**< start */
	SPP_CMD_ACTION_STOP,  /**< stop */
	SPP_CMD_ACTION_ADD,   /**< add */
	SPP_CMD_ACTION_DEL,   /**< delete */
};

/** Port ability operation */
enum spp_port_ability_ope {
	SPP_PORT_ABILITY_OPE_NONE,        /**< none */
	SPP_PORT_ABILITY_OPE_ADD_VLANTAG, /**< add VLAN tag */
	SPP_PORT_ABILITY_OPE_DEL_VLANTAG, /**< delete VLAN tag */
};

/**
 * Interface information structure
 */
struct spp_port_index {
	enum port_type  iface_type; /**< Interface type (phy/vhost/ring) */
	int             iface_no;   /**< Interface number */
};

/** VLAN tag information */
struct spp_vlantag_info {
	int vid; /**< VLAN ID */
	int pcp; /**< Priority Code Point */
	int tci; /**< Tag Control Information */
};

/** Data for each port ability */
union spp_ability_data {
	/** VLAN tag information */
	struct spp_vlantag_info vlantag;
};

/** Port ability information */
struct spp_port_ability {
	enum spp_port_ability_ope ope; /**< Operation */
	enum spp_port_rxtx rxtx;       /**< rx/tx identifier */
	union spp_ability_data data;   /**< Port ability data */
};

/** Port class identifier for classifying */
struct spp_port_class_identifier {
	uint64_t mac_addr;                      /**< Mac address (binary) */
	char     mac_addr_str[SPP_MIN_STR_LEN]; /**< Mac address (text) */
	struct spp_vlantag_info vlantag;        /**< VLAN tag information */
};

/**
 * Port info
 */
struct spp_port_info {
	enum port_type iface_type;      /**< Interface type (phy/vhost/ring) */
	int            iface_no;        /**< Interface number */
	int            dpdk_port;       /**< DPDK port number */
	struct spp_port_class_identifier class_id;
					/**< Port class identifier */
	struct spp_port_ability ability[SPP_PORT_ABILITY_MAX];
					/**< Port ability */
};

/**
 * Component info
 */
struct spp_component_info {
	char name[SPP_NAME_STR_LEN];    /**< Component name */
	enum spp_component_type type;   /**< Component type */
	unsigned int lcore_id;          /**< Logical core ID for component */
	int component_id;               /**< Component ID */
	int num_rx_port;                /**< The number of rx ports */
	int num_tx_port;                /**< The number of tx ports */
	struct spp_port_info *rx_ports[RTE_MAX_ETHPORTS];
					/**< Array of pointers to rx ports */
	struct spp_port_info *tx_ports[RTE_MAX_ETHPORTS];
					/**< Array of pointers to tx ports */
};

/**
 * Get client ID
 *
 * @return Client ID(0~127)
 */
int spp_get_client_id(void);

/**
 * Update Classifier_table
 *
 * @param action
 *  Action identifier (add or del)
 * @param type
 *  Classify type (currently only for mac)
 * @param data
 *  Value to be classified
 * @param port
 *  Destination port type and number
 *
 * @retval SPP_RET_OK succeeded.
 * @retval SPP_RET_NG failed.
 */
int spp_update_classifier_table(
		enum spp_command_action action,
		enum spp_classifier_type type,
		int vid,
		const char *mac,
		const struct spp_port_index *port);

/**
 * Update component
 *
 * @param action
 *  Action identifier (start or stop)
 * @param name
 *  Component name
 * @param lcore_id
 *  Logical core number
 * @param type
 *  Component type
 *
 * @retval SPP_RET_OK succeeded.
 * @retval SPP_RET_NG failed.
 */
int spp_update_component(
		enum spp_command_action action,
		const char *name, unsigned int lcore_id,
		enum spp_component_type type);

/**
 * Update port
 *
 * @param action
 *  Action identifier (add or del)
 * @param port
 *  Port type and number
 * @param rxtx
 *  rx/tx identifier
 * @param name
 *  Attached component name
 *
 * @retval SPP_RET_OK succeeded.
 * @retval SPP_RET_NG failed.
 */
int spp_update_port(
		enum spp_command_action action,
		const struct spp_port_index *port,
		enum spp_port_rxtx rxtx,
		const char *name,
		const struct spp_port_ability *ability);

/**
 * Flush SPP component
 *
 * @retval SPP_RET_OK succeeded.
 * @retval SPP_RET_NG failed.
 */
int spp_flush(void);

struct spp_iterate_core_params;
/** definition of iterated core element procedure function */
typedef int (*spp_iterate_core_element_proc)(
		struct spp_iterate_core_params *params,
		const unsigned int lcore_id,
		const char *name,
		const char *type,
		const int num_rx,
		const struct spp_port_index *rx_ports,
		const int num_tx,
		const struct spp_port_index *tx_ports);

/** iterate core information parameters */
struct spp_iterate_core_params {
	/** Output buffer */
	char *output;

	/** The function for creating core information */
	spp_iterate_core_element_proc element_proc;
};

/**
 * Iterate core information
 *
 * @param params
 *  The pointer to struct spp_iterate_core_params.@n
 *  The value for generating core information (status command).
 *
 * @retval SPP_RET_OK succeeded.
 * @retval SPP_RET_NG failed.
 */
int spp_iterate_core_info(struct spp_iterate_core_params *params);

struct spp_iterate_classifier_table_params;
/** definition of iterated classifier element procedure function */
typedef int (*spp_iterate_classifier_element_proc)(
		struct spp_iterate_classifier_table_params *params,
		enum spp_classifier_type type,
		int vid, const char *mac,
		const struct spp_port_index *port);

/** iterate classifier table parameters */
struct spp_iterate_classifier_table_params {
	void *output;
	spp_iterate_classifier_element_proc element_proc;
};

/**
 * Iterate Classifier_table
 *
 * @param params
 *  The pointer to struct spp_iterate_classifier_table_params.@n
 *  The value for generating classifier table.
 *
 * @retval SPP_RET_OK succeeded.
 * @retval SPP_RET_NG failed.
 */
int spp_iterate_classifier_table(
		struct spp_iterate_classifier_table_params *params);

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
 * Get component type being updated on target core
 *
 * @param lcore_id
 *  Logical core ID.
 *
 * @return
 *  Type of component that will be executed on
 *  specified logical core after update.
 */
enum spp_component_type spp_get_component_type_update(unsigned int lcore_id);

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

/**
 * Check core index change
 *
 * @param lcore_id
 *  Logical core ID.
 *
 * @return
 *  True if index has changed.
 */
int spp_check_core_index(unsigned int lcore_id);

/**
 * Get name matching component ID
 *
 * @param name
 *  Component name.
 *
 * @retval 0~127      Component ID.
 * @retval SPP_RET_NG failed.
 */
int spp_get_component_id(const char *name);

/**
 * Check mac address used on the port for registering or removing
 *
 * @param vid
 *  VLAN ID to be validated.
 * @param mac_addr
 *  Mac address to be validated.
 * @param iface_type
 *  Interface to be validated.
 * @param iface_no
 *  Interface number to be validated.
 *
 * @return
 *  True if target identifier(VLAN ID, MAC address)
 *  matches identifier(VLAN ID, MAC address) of port.
 */
int spp_check_classid_used_port(
		int vid, uint64_t mac_addr,
		enum port_type iface_type, int iface_no);

/**
 * Check if port has been added.
 *
 * @param iface_type
 *  Interface to be validated.
 * @param iface_no
 *  Interface number to be validated.
 *
 * @return
 *  True if port has been added.
 */
int spp_check_added_port(enum port_type iface_type, int iface_no);

/**
 * Check if port has been flushed.
 *
 * @param iface_type
 *  Interface to be validated.
 * @param iface_no
 *  Interface number to be validated.
 *
 * @return
 *  True if port has been flushed.
 */
int spp_check_flush_port(enum port_type iface_type, int iface_no);

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
 * Change mac address string to int64
 *
 * @param mac
 *  Character string of MAC address to be converted.
 *
 * @retval 0< int64 that store mac address
 * @retval -1
 */
int64_t spp_change_mac_str_to_int64(const char *mac);

/**
 * Get the port number of DPDK.
 *
 * @param iface_type
 *  Interface type obtained from port.
 * @param iface_no
 *  Interface number obtained from port.
 *
 * @return
 *  Port id generated by DPDK.
 */
int spp_get_dpdk_port(enum port_type iface_type, int iface_no);

/**
 * Extract if-type/if-number from port string
 *
 * @param port
 *  Character string expressing the port, e.g. "phy:0","ring:1"
 * @param iface_type
 *  Interface type obtained from port.
 * @param iface_no
 *  Interface number obtained from port.
 *
 * @retval 0  succeeded.
 * @retval -1 failed.
 */
int spp_get_iface_index(
		const char *port,
		enum port_type *iface_type,
		int *iface_no);

/**
 * Format port string form if-type/if-number
 *
 * @param port
 *  Character string expressing the port, e.g. "phy:0","ring:1"
 * @param iface_type
 *  Interface type.
 * @param iface_no
 *  Interface number.
 *
 * @retval 0  succeeded.
 * @retval -1 failed.
 */
int spp_format_port_string(char *port, enum port_type iface_type, int iface_no);

/**
 * Change component type from string to type value.
 *
 * @param type_str
 *  Name string for each component
 *
 * @return
 *  Component type corresponding to type_str.
 */
enum spp_component_type spp_change_component_type(const char *type_str);

#endif /* __SPP_VF_H__ */
