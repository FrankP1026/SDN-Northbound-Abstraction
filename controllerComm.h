int connect_to_controller(char *ctrlHost, int ctrlPort);
int send_command_to_controller(int socket, char *command, char **response);
int communicate_to_all_controllers(char *command, char **overallResponse);
int communicate_to_controller(char *command, char **response, char *ctrlHost, int ctrlPort);