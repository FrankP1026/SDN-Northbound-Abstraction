#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include "handleRequest.h"
#include "controllerComm.h"
#include "wrapperTypes.h"

extern int num_of_controllers;
extern ctrl_info *controller_info;
extern pthread_mutex_t lock;

// returns 0 if request valid and truncation is succeeded
// TODO: more strict error checking?
int validate_http_request(char* request, int requestLength)
{
	int ret_val = 0;
	char *requestBak = (char *)malloc(sizeof(char) * requestLength);
	strcpy(requestBak, request);
	char *p = strstr(requestBak, "\r\n");
	if (!p)
	{
		printf("Bad request from application\n");
		return 1;
	}
	
	bzero(p, requestLength - (p - requestBak));
	
	if ( ( strncmp(requestBak, "GET /", strlen("GET /")) != 0 && 
	       strncmp(requestBak, "POST /", strlen("POST /")) != 0 &&
	       strncmp(requestBak, "DELETE /", strlen("DELETE /")) != 0 ) ||
		 ( strncmp(p - strlen("HTTP/1.1"), "HTTP/1.1", strlen("HTTP/1.1")) != 0 &&
		   strncmp(p - strlen("HTTP/1.0"), "HTTP/1.0", strlen("HTTP/1.0")) != 0 ) )
	{
		printf("Bad request from application\n");
		ret_val = 1;
	}
	
	free(requestBak);
	return ret_val;
}

int process_command(int appSocket, char *appRequest)
{
	int i,j;
	char *reqStartPt = strchr(appRequest, '/');
	char *controllerResponse = 0;
	int responseLength = 0;
	int match_indicator = 0;
	//printf("First 5 charcters: %.*s\n", 5, appRequest);
	
	//clock_t start = clock();
	
	pthread_mutex_lock(&lock);
	if (strncmp(reqStartPt, "/wm/core/controller/switches/json", strlen("/wm/core/controller/switches/json")) == 0)
	{
		responseLength = communicate_to_all_controllers(appRequest, &controllerResponse);
		match_indicator = 1;
	}
	else if (strncmp(reqStartPt, "/wm/topology/links/json", strlen("/wm/topology/links/json")) == 0)
	{
		responseLength = communicate_to_all_controllers(appRequest, &controllerResponse);
		match_indicator = 1;
	}
	else if (strncmp(reqStartPt, "/wm/core/switch", strlen("/wm/core/switch")) == 0)
	{
		if (strncmp( reqStartPt + strlen("/wm/core/switch") , "/all", 4) == 0)
		{
			responseLength = communicate_to_all_controllers(appRequest, &controllerResponse);
			match_indicator = 1;	
		}
		else
		{
			for(i=0; i<num_of_controllers; i++)
			{
				for(j=0; j<controller_info[i].num_switch_connected; j++)
				{
					
					if (strncmp( reqStartPt + strlen("/wm/core/switch/") , controller_info[i].switch_dpid[j] , strlen(controller_info[i].switch_dpid[j])) == 0)
					{
						if(strncmp( reqStartPt + strlen("/wm/core/switch/") + 23 , "/aggregate/json", strlen("/aggregate/json" )) == 0)
						{
							responseLength = communicate_to_controller(appRequest, &controllerResponse, controller_info[i].ip, controller_info[i].port);
							match_indicator = 1;
						}
						else if(strncmp( reqStartPt + strlen("/wm/core/switch/") + 23 , "/desc/json", strlen("/desc/json" )) == 0)
						{
							responseLength = communicate_to_controller(appRequest, &controllerResponse, controller_info[i].ip, controller_info[i].port);
							match_indicator = 1;
						}
						else if(strncmp( reqStartPt + strlen("/wm/core/switch/") + 23 , "/flow/json", strlen("/flow/json" )) == 0)
						{
							responseLength = communicate_to_controller(appRequest, &controllerResponse, controller_info[i].ip, controller_info[i].port);
							match_indicator = 1;
						}
						else if(strncmp( reqStartPt + strlen("/wm/core/switch/") + 23, "/port/json", strlen("/port/json" )) == 0)
						{
							responseLength = communicate_to_controller(appRequest, &controllerResponse, controller_info[i].ip, controller_info[i].port);
							match_indicator = 1;
						}
						else if(strncmp( reqStartPt + strlen("/wm/core/switch/") + 23 , "/queue/json", strlen("/queue/json" )) == 0)
						{
							responseLength = communicate_to_controller(appRequest, &controllerResponse, controller_info[i].ip, controller_info[i].port);
							match_indicator = 1;
						}
						else if(strncmp( reqStartPt + strlen("/wm/core/switch/") + 23 , "/features/json", strlen("/features/json" )) == 0)
						{
							responseLength = communicate_to_controller(appRequest, &controllerResponse, controller_info[i].ip, controller_info[i].port);
							match_indicator = 1;
						}
					}
				}
			}
		}
	}
	else if (strncmp(reqStartPt, "/wm/topology/external-links/json", strlen("/wm/topology/external-links/json")) == 0)
	{
		responseLength = communicate_to_all_controllers(appRequest, &controllerResponse);
		match_indicator = 1;
	}
	else if (strncmp(reqStartPt, "/wm/staticflowpusher/list/", strlen("/wm/staticflowpusher/list/"))==0)
	{
		if (strncmp( reqStartPt + strlen("/wm/staticflowpusher/list/") , "all", 3) == 0)
		{
			responseLength = communicate_to_all_controllers(appRequest, &controllerResponse);
			match_indicator = 1;
		}
		else
		{
			for(i=0; i<num_of_controllers; i++)
			{
				for(j=0; j<controller_info[i].num_switch_connected; j++)
				{	
					if (strncmp( reqStartPt + strlen("/wm/staticflowpusher/list/") , controller_info[i].switch_dpid[j] , strlen(controller_info[i].switch_dpid[j])) == 0)
					{
						if(strncmp( reqStartPt + strlen("/wm/staticflowpusher/list/") + 23 , "/json", 5) == 0)
						{
							responseLength = communicate_to_controller(appRequest, &controllerResponse, controller_info[i].ip, controller_info[i].port);
							match_indicator = 1;
						}
					}
				}
			}
		}
	}
	else if(strncmp(reqStartPt, "/wm/topology/route/", strlen("/wm/topology/route/"))==0)
	{
		for(i=0; i<num_of_controllers; i++)
		{
			for(j=0; j<controller_info[i].num_switch_connected; j++)
			{	
				if (strncmp( reqStartPt + strlen("/wm/topology/route/") , controller_info[i].switch_dpid[j] , strlen(controller_info[i].switch_dpid[j])) == 0)
				{
					if(strncmp( reqStartPt + strlen("/wm/topology/route/") + 23 , "/", 1) == 0)
					{
						if(strncmp( reqStartPt + strlen("/wm/topology/route/") + 24 , controller_info[i].switch_dpid[j] , 1) == 0)
						{
							responseLength = communicate_to_controller(appRequest, &controllerResponse, controller_info[i].ip, controller_info[i].port);
							match_indicator = 1;
						}
					}
				}
			}
		}
	}
	else if(strncmp(reqStartPt, "/wm/device/", strlen("/wm/device/"))==0)
	{
		responseLength = communicate_to_all_controllers(appRequest, &controllerResponse);
		match_indicator = 1;
	}
	else if(strncmp(reqStartPt, "/wm/staticflowpusher/json", strlen("/wm/staticflowpusher/json"))==0)
	{
		if(strncmp(appRequest, "POST", 4)==0)
		{
			char *switchpos = strstr(appRequest, "\"switch\"");
			char dpid[23];
			strncpy(dpid, strchr(switchpos+8, '\"')+1, 23);
			//printf("DPID = : %s\n", dpid);
			for(i=0; i<num_of_controllers; i++)
			{
				for(j=0; j<controller_info[i].num_switch_connected; j++)
				{
					if(strncmp(dpid, controller_info[i].switch_dpid[j], 23) == 0)
					{
						responseLength = communicate_to_controller(appRequest, &controllerResponse, controller_info[i].ip, controller_info[i].port);
						match_indicator = 1;
						break;
					}
				}
				if(match_indicator == 1)
					break;
			}
		}
		else if (strncmp(appRequest, "DELETE", 6)==0)
		{
			responseLength = communicate_to_all_controllers(appRequest, &controllerResponse);
			match_indicator = 1;
		}
	}
	pthread_mutex_lock(&lock);
	
	if (match_indicator == 0)
	{
		printf("Request %s is not supported\n", appRequest);
		// TODO: send this message to the application
		return -1;
	}
	
	if (responseLength <= 0)
	{
		//printf("Response length: %d\n", responseLength);
		printf("Failed to get response from the system\n");
		return -1;
	}
	
	//printf("Final controller response:%s\n", controllerResponse);
	write(appSocket, controllerResponse , responseLength);
	
	//printf("Time elapsed: %f\n", (double)(clock() - start) / CLOCKS_PER_SEC);
	
	free(controllerResponse);
	return 0;
}

void *handle_app_request(void *appSocket)
{
	int appSock = *(int*)appSocket;
    int read_size;
    char *message;
	//printf("Received an app request.\n");
	
	/*message = "You can start!\n";
    write(appSock, message , strlen(message));*/
    
    char appRequest[1024];
    bzero(appRequest, 1024);
    read(appSock, appRequest, 1024);
    
    //printf("msg from application: %s\n", appRequest);
    
    if (validate_http_request(appRequest, sizeof(appRequest)) != 0)
    {
    	perror("Request from application is not valid");
    	close(appSock);
    	return 0;
    }
	process_command(appSock, appRequest);
    
    /*int ctrlSock = connect_to_controller("127.0.0.1", 8080);
    char response[1024];
    bzero(response, sizeof(response));
    
    if (send_command_to_controller(ctrlSock, appRequest, response, sizeof(response)) != 0) {
    	perror("sending command to controller failed");
    	return 0;
    }
    
    printf("%s\n", response);
    close(ctrlSock); 
    write(appSock, response, sizeof(response));*/
    
    close(appSock);
	return 0;
}
