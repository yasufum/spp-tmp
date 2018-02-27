#ifndef _COMMAND_DEC_H_
#define _COMMAND_DEC_H_

/* max number of command per request */
#define SPP_CMD_MAX_COMMANDS 32

/* maximum number of parameters per command */
#define SPP_CMD_MAX_PARAMETERS 8

/* command name string buffer size (include null char) */
#define SPP_CMD_NAME_BUFSZ  32

/* command value string buffer size (include null char) */
#define SPP_CMD_VALUE_BUFSZ 128

/* string that specify unused */
#define SPP_CMD_UNUSE "unuse"

/* decode error code */
enum spp_command_decode_error_code {
	/* not use 0, in general 0 is OK */
	SPP_CMD_DERR_BAD_FORMAT = 1,
	SPP_CMD_DERR_UNKNOWN_COMMAND,
	SPP_CMD_DERR_NO_PARAM,
	SPP_CMD_DERR_BAD_TYPE,
	SPP_CMD_DERR_BAD_VALUE,
};

/**
 * spp command type.
 *
 * @attention This enumerated type must have the same order of command_list
 *            defined in command_dec.c
 */
enum spp_command_type {
	SPP_CMDTYPE_CLASSIFIER_TABLE,
	SPP_CMDTYPE_FLUSH,
	SPP_CMDTYPE_CLIENT_ID,
	SPP_CMDTYPE_STATUS,
	SPP_CMDTYPE_EXIT,
	SPP_CMDTYPE_COMPONENT,
	SPP_CMDTYPE_PORT,
	SPP_CMDTYPE_CANCEL,
};

/* "classifier_table" command specific parameters */
struct spp_command_classifier_table {
	/* Action identifier (add or del) */
	enum spp_command_action action;

	/* Classify type (currently only for mac) */
	enum spp_classifier_type type;

	/* Value to be classified */
	char value[SPP_CMD_VALUE_BUFSZ];

	/* Destination port type and number */
	struct spp_port_index port;
};

/* "flush" command specific parameters */
struct spp_command_flush {
	/* nothing specific */
};

/* "component" command parameters */
struct spp_command_component {
	enum spp_command_action action; /* Action identifier (start or stop) */
	char name[SPP_CMD_NAME_BUFSZ];  /* Component name */
	unsigned int core;              /* Logical core number */
	enum spp_component_type type;   /* Component type */
};

/* "port" command parameters */
struct spp_command_port {
	enum spp_command_action action; /* Action identifier (add or del) */
	struct spp_port_index port;     /* Port type and number */
	enum spp_port_rxtx rxtx;        /* rx/tx identifier */
	char name[SPP_CMD_NAME_BUFSZ];  /* Attached component name */
};

/* command parameters */
struct spp_command {
	enum spp_command_type type; /* Command type */

	union {
		/* Structured data for classifier_table command  */
		struct spp_command_classifier_table classifier_table;

		/* Structured data for flush command  */
		struct spp_command_flush flush;

		/* Structured data for component command  */
		struct spp_command_component component;

		/* Structured data for port command  */
		struct spp_command_port port;
	} spec;
};

/* request parameters */
struct spp_command_request {
	int num_command;                /* Number of accepted commands */
	int num_valid_command;          /* Number of executed commands */
	struct spp_command commands[SPP_CMD_MAX_COMMANDS];
					/* Information of executed commands */

	int is_requested_client_id;     /* Id for get_client_id command */
	int is_requested_status;        /* Id for status command */
	int is_requested_exit;          /* Id for exit command */
};

/* decode error information */
struct spp_command_decode_error {
	int code;                            /* Error code */
	char value_name[SPP_CMD_NAME_BUFSZ]; /* Error value name */
	char value[SPP_CMD_VALUE_BUFSZ];     /* Error value */
};

/* decode request from no-null-terminated string */
int spp_command_decode_request(struct spp_command_request *request,
		const char *request_str, size_t request_str_len,
		struct spp_command_decode_error *error);

#endif /* _COMMAND_DEC_H_ */
