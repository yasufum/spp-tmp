/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Nippon Telegraph and Telephone Corporation
 */

/**
 * TODO(yasufum) change this define tag because it is the same as
 * shared/.../cmd_utils.h. However, it should be the same to avoid both of
 * this and shared headers are included which are incompabtible and causes
 * an compile error. After fixing the incompatibility, change the tag name.
 */
#ifndef _SPPWK_CMD_UTILS_H_
#define _SPPWK_CMD_UTILS_H_

/**
 * @file
 * SPP process
 *
 * SPP component common function.
 */

#include <netinet/in.h>
#include "shared/common.h"

/* Max number of core status check */
#define SPP_CORE_STATUS_CHECK_MAX 5

/** The length of shortest character string */
#define SPP_MIN_STR_LEN   32

/** The length of NAME string */
#define SPP_NAME_STR_LEN  128

/** Maximum number of port abilities available */
#define SPP_PORT_ABILITY_MAX 4

/** Identifier string for each interface */
#define SPP_IFTYPE_NIC_STR   "phy"
#define SPP_IFTYPE_RING_STR  "ring"

/* TODO(yasufum) merge it to the same definition in shared/.../cmd_utils.h */
/* State on core */
enum sppwk_lcore_status {
	SPP_CORE_UNUSE,        /**< Not used */
	SPP_CORE_STOP,         /**< Stopped */
	SPP_CORE_IDLE,         /**< Idling */
	SPP_CORE_FORWARD,      /**< Forwarding  */
	SPP_CORE_STOP_REQUEST, /**< Request stopping */
	SPP_CORE_IDLE_REQUEST  /**< Request idling */
};

/* State on capture */
enum sppwk_capture_status {
	SPP_CAPTURE_IDLE,      /* Idling */
	SPP_CAPTURE_RUNNING     /* Running */
};

enum sppwk_return_val {
	SPPWK_RET_OK = 0,  /**< succeeded */
	SPPWK_RET_NG = -1, /**< failed */
};

/**
 * Port type (rx or tx) to indicate which direction packet goes
 * (e.g. receiving or transmitting)
 */
enum spp_port_rxtx {
	SPP_PORT_RXTX_NONE, /**< none */
	SPP_PORT_RXTX_RX,   /**< rx port */
	SPP_PORT_RXTX_TX,   /**< tx port */
	SPP_PORT_RXTX_ALL,  /**< rx/tx port */
};

/* TODO(yasufum) merge it to the same definition in shared/.../cmd_utils.h */
/* Type of SPP worker thread. */
enum sppwk_worker_type {
	SPPWK_TYPE_NONE,  /**< Not used */
	SPPWK_TYPE_CLS,  /**< Classifier_mac */
	SPPWK_TYPE_MRG,  /**< Merger */
	SPPWK_TYPE_FWD,  /**< Forwarder */
	SPPWK_TYPE_MIR,  /**< Mirror */
};

/**
 * Port ability operation which indicates vlan tag operation on the port
 * (e.g. add vlan tag or delete vlan tag)
 */
enum spp_port_ability_ope {
	SPP_PORT_ABILITY_OPE_NONE,        /**< none */
	SPP_PORT_ABILITY_OPE_ADD_VLANTAG, /**< add VLAN tag */
	SPP_PORT_ABILITY_OPE_DEL_VLANTAG, /**< delete VLAN tag */
};

/* getopt_long return value for long option */
enum SPP_LONGOPT_RETVAL {
	SPP_LONGOPT_RETVAL__ = 127,

	/*
	 * Return value definition for getopt_long()
	 * Only for long option
	 */
	SPP_LONGOPT_RETVAL_CLIENT_ID,  /* --client-id */
	SPP_LONGOPT_RETVAL_OUT_DIR,    /* --out-dir */
	SPP_LONGOPT_RETVAL_FILE_SIZE   /* --fsize */
};

/** VLAN tag information */
struct spp_vlantag_info {
	int vid; /**< VLAN ID */
	int pcp; /**< Priority Code Point */
	int tci; /**< Tag Control Information */
};

/**
 * Data for each port ability which indicates vlantag related information
 * for the port
 */
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

/* TODO(yasufum) confirm why vlantag is required for spp_pcap. */
/* Attributes for classifying . */
struct sppwk_cls_attrs {
	uint64_t mac_addr;  /**< Mac address (binary) */
	char mac_addr_str[SPP_MIN_STR_LEN];  /**< Mac address (text) */
	struct spp_vlantag_info vlantag;   /**< VLAN tag information */
};

/* Interface information structure */
struct sppwk_port_idx {
	enum port_type iface_type;  /**< phy, vhost or ring. */
	int iface_no;
};

/* Define detailed port params in addition to `sppwk_port_idx`. */
struct sppwk_port_info {
	enum port_type iface_type;  /**< phy, vhost or ring */
	int iface_no;
	int ethdev_port_id;  /**< Consistent ID of ethdev */
	struct sppwk_cls_attrs cls_attrs;
	struct spp_port_ability ability[SPP_PORT_ABILITY_MAX];
};

/* TODO(yasufum) merge it to the same definition in shared/.../cmd_utils.h */
/* Attributes of SPP worker thread named as `component`. */
struct sppwk_comp_info {
	char name[SPP_NAME_STR_LEN];  /**< Component name */
	enum sppwk_worker_type wk_type;  /**< Type of worker thread */
	unsigned int lcore_id;
	int comp_id;  /**< Component ID */
	int nof_rx;  /**< The number of rx ports */
	int nof_tx;  /**< The number of tx ports */
	struct sppwk_port_info *rx_ports[RTE_MAX_ETHPORTS]; /**< rx ports */
	struct sppwk_port_info *tx_ports[RTE_MAX_ETHPORTS]; /**< tx ports */
};

/* Manage given options as global variable */
struct startup_param {
	int client_id;		/* Client ID */
	char server_ip[INET_ADDRSTRLEN];
				/* IP address stiring of spp-ctl */
	int server_port;	/* Port Number of spp-ctl */
};

/* Manage interfaces and port information as global variable */
struct iface_info {
	int num_nic;            /* The number of phy */
	int num_ring;           /* The number of ring */
	struct sppwk_port_info nic[RTE_MAX_ETHPORTS];
				/* Port information of phy */
	struct sppwk_port_info ring[RTE_MAX_ETHPORTS];
				/* Port information of ring */
};

/* Manage core status and component information as global variable */
struct core_mng_info {
	/* Status of cpu core */
	volatile enum sppwk_lcore_status status;
};

/* TODO(yasufum) refactor name of func and vars, and comments. */
struct spp_iterate_core_params;
/**
 * definition of iterated core element procedure function
 * which is member of spp_iterate_core_params structure.
 * Above structure is used when listing core information
 * (e.g) create resonse to status command.
 */
typedef int (*spp_iterate_core_element_proc)(
		struct spp_iterate_core_params *params,
		const unsigned int lcore_id,
		const char *name,
		const char *type,
		const int num_rx,
		const struct sppwk_port_idx *rx_ports,
		const int num_tx,
		const struct sppwk_port_idx *tx_ports);

/* TODO(yasufum) refactor name of func and vars, and comments. */
/**
 * iterate core table parameters which is
 * used when listing core table content
 * (e.g.) create response to status command.
 */
struct spp_iterate_core_params {
	/** Output buffer */
	char *output;

	/** The function for creating core information */
	spp_iterate_core_element_proc element_proc;
};

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
 * Get status of lcore of given ID from global management info.
 *
 * @param[in] lcore_id Logical core ID.
 * @return Status of specified logical core.
 */
enum sppwk_lcore_status sppwk_get_lcore_status(unsigned int lcore_id);

/**
 * Run check_core_status() for SPP_CORE_STATUS_CHECK_MAX times with
 * interval time (1sec)
 *
 * @param status Wait check status.
 * @retval 0  If succeeded.
 * @retval -1 If failed.
 */
int check_core_status_wait(enum sppwk_lcore_status status);

/**
 * Set core status
 *
 * @param lcore_id Logical core ID.
 * @param status Set status.
 */
void set_core_status(unsigned int lcore_id, enum sppwk_lcore_status status);

/**
 * Set all core status to given
 *
 * @param status
 *  set status.
 *
 */
void set_all_core_status(enum sppwk_lcore_status status);

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
 * @retval !NULL  sppwk_port_info.
 * @retval NULL   failed.
 */
struct sppwk_port_info *
get_iface_info(enum port_type iface_type, int iface_no);

/**
 * Setup management info for spp_vf
 */
int init_mng_data(void);

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

/* Get core information which is in use */
struct core_info *get_core_info(unsigned int lcore_id);

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
 * @retval SPPWK_RET_OK succeeded.
 * @retval SPPWK_RET_NG failed.
 */
int
spp_format_port_string(char *port, enum port_type iface_type, int iface_no);

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
 * @retval SPPWK_RET_OK succeeded.
 * @retval SPPWK_RET_NG failed.
 */
int
spp_format_port_string(char *port, enum port_type iface_type, int iface_no);

/**
 * Set mange data address
 *
 * @param startup_param_addr
 *  g_startup_param address
 * @param iface_addr
 *  g_iface_info address
 * @param core_mng_addr
 *  g_core_info address
 * @param capture_request_addr
 *  g_capture_request address
 * @param capture_status_addr
 *  g_capture_status address
 * @param main_lcore_id
 *  main_lcore_id mask
 *
 * @retval SPPWK_RET_OK succeeded.
 * @retval SPPWK_RET_NG failed.
 */
int spp_set_mng_data_addr(struct startup_param *startup_param_addr,
			  struct iface_info *iface_addr,
			  struct core_mng_info *core_mng_addr,
			  int *capture_request_addr,
			  int *capture_status_addr,
			  unsigned int main_lcore_id);

/**
 * Get mange data address
 *
 * @param iface_addr
 *  g_startup_param write address
 * @param iface_addr
 *  g_iface_info write address
 * @param core_mng_addr
 *  g_core_mng_info write address
 * @param change_core_addr
 *  g_capture_request write address
 * @param change_component_addr
 *  g_capture_status write address
 */
void spp_get_mng_data_addr(struct startup_param **startup_param_addr,
			   struct iface_info **iface_addr,
			   struct core_mng_info **core_mng_addr,
			   int **capture_request_addr,
			   int **capture_status_addr);

#endif
