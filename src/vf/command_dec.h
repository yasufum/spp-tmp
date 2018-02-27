#ifndef _COMMAND_DEC_H_
#define _COMMAND_DEC_H_

/**
 * @file
 * SPP command decode
 *
 * Decode and validate the command message string.
 */

/** max number of command per request */
#define SPP_CMD_MAX_COMMANDS 32

/** maximum number of parameters per command */
#define SPP_CMD_MAX_PARAMETERS 8

/** command name string buffer size (include null char) */
#define SPP_CMD_NAME_BUFSZ  32

/** command value string buffer size (include null char) */
#define SPP_CMD_VALUE_BUFSZ 128

/** string that specify unused */
#define SPP_CMD_UNUSE "unuse"

/** decode error code */
enum spp_command_decode_error_code {
	/* not use 0, in general 0 is OK */
	SPP_CMD_DERR_BAD_FORMAT = 1,  /**< Wrong format */
	SPP_CMD_DERR_UNKNOWN_COMMAND, /**< Unknown command */
	SPP_CMD_DERR_NO_PARAM,        /**< No parameters */
	SPP_CMD_DERR_BAD_TYPE,        /**< Wrong data type */
	SPP_CMD_DERR_BAD_VALUE,       /**< Wrong value */
};

/**
 * spp command type.
 *
 * @attention This enumerated type must have the same order of command_list
 *            defined in command_dec.c
 */
enum spp_command_type {
	SPP_CMDTYPE_CLASSIFIER_TABLE, /**< classifier_table command */
	SPP_CMDTYPE_FLUSH,            /**< flush command */
	SPP_CMDTYPE_CLIENT_ID,        /**< get_client_id command */
	SPP_CMDTYPE_STATUS,           /**< status command */
	SPP_CMDTYPE_EXIT,             /**< exit command */
	SPP_CMDTYPE_COMPONENT,        /**< component command */
	SPP_CMDTYPE_PORT,             /**< port command */
	SPP_CMDTYPE_CANCEL,           /**< cancel command */
};

/** "classifier_table" command specific parameters */
struct spp_command_classifier_table {
	/** Action identifier (add or del) */
	enum spp_command_action action;

	/** Classify type (currently only for mac) */
	enum spp_classifier_type type;

	/** Value to be classified */
	char value[SPP_CMD_VALUE_BUFSZ];

	/** Destination port type and number */
	struct spp_port_index port;
};

/* "flush" command specific parameters */
struct spp_command_flush {
	/* nothing specific */
};

/** "component" command parameters */
struct spp_command_component {
	/**< Action identifier (start or stop) */
	enum spp_command_action action;

	/**< Component name */
	char name[SPP_CMD_NAME_BUFSZ];

	/**< Logical core number */
	unsigned int core;

	/**< Component type */
	enum spp_component_type type;
};

/** "port" command parameters */
struct spp_command_port {
	enum spp_command_action action; /**< Action identifier (add or del) */
	struct spp_port_index port;     /**< Port type and number */
	enum spp_port_rxtx rxtx;        /**< rx/tx identifier */
	char name[SPP_CMD_NAME_BUFSZ];  /**< Attached component name */
};

/** command parameters */
struct spp_command {
	enum spp_command_type type; /**< Command type */

	union {
		/** Structured data for classifier_table command  */
		struct spp_command_classifier_table classifier_table;

		/** Structured data for flush command  */
		struct spp_command_flush flush;

		/** Structured data for component command  */
		struct spp_command_component component;

		/** Structured data for port command  */
		struct spp_command_port port;
	} spec;
};

/** request parameters */
struct spp_command_request {
	int num_command;                /**< Number of accepted commands */
	int num_valid_command;          /**< Number of executed commands */
	struct spp_command commands[SPP_CMD_MAX_COMMANDS];
					/**< Information of executed commands */

	int is_requested_client_id;     /**< Id for get_client_id command */
	int is_requested_status;        /**< Id for status command */
	int is_requested_exit;          /**< Id for exit command */
};

/** decode error information */
struct spp_command_decode_error {
	int code;                            /**< Error code */
	char value_name[SPP_CMD_NAME_BUFSZ]; /**< Error value name */
	char value[SPP_CMD_VALUE_BUFSZ];     /**< Error value */
};

/**
 * decode request from no-null-terminated string
 *
 * @param request
 *  The pointer to struct spp_command_request.@n
 *  The result value of decoding the command message.
 * @param request_str
 *  The pointer to requested command message.
 * @param request_str_len
 *  The length of requested command message.
 * @param error
 *  The pointer to struct spp_command_decode_error.@n
 *  Detailed error information will be stored.
 *
 * @retval 0  succeeded.
 * @retval !0 failed.
 */
int spp_command_decode_request(struct spp_command_request *request,
		const char *request_str, size_t request_str_len,
		struct spp_command_decode_error *error);

#endif /* _COMMAND_DEC_H_ */
