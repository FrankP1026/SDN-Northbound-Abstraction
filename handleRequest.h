int validate_http_request(char* request, int requestLength);
int process_command(int appSocket, char *appRequest);
void *handle_app_request(void *appSocket);