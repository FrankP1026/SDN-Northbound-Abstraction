typedef struct {
	char *ip;
	int port;
	int num_switch_connected;
	char **switch_dpid;
} ctrl_info;