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
#include "wrapperTypes.h"
#include "controllerComm.h"

extern int num_of_controllers;
extern ctrl_info *controller_info;

int get_proper_http_header(char *httpResponse, char **httpHeader)
{
	const char *toRemove = "Transfer-Encoding: chunked\r\n";
	const char *toRemove2 = "Content-Length";
	
	int lengthToRemove = 0;
	int headerLength = 0;
	char *headerEnd = strstr(httpResponse, "\r\n\r\n");
	char *p, *p2;
	if (!headerEnd)
		return -1;
	
	headerEnd += strlen("\r\n\r\n") - 1;
	
	if ( (p = strstr(httpResponse, toRemove)) ) {
		memmove(p,p+strlen(toRemove),1+strlen(p+strlen(toRemove)));
		headerEnd -= strlen(toRemove);
	}
	
	if ( (p = strstr(httpResponse, toRemove2)) ) {
		p2 = strstr(p, "\r\n") + strlen("\r\n");
		memmove(p, p2, 1+strlen(p2));
		headerEnd -= p2 - p;
	}
	
	headerLength = headerEnd - httpResponse + 1;
	*httpHeader = (char *)malloc(sizeof(char) * (headerLength + 1));
	strncpy(*httpHeader, httpResponse, headerLength);
	(*httpHeader)[headerLength] = '\0';
	
	return headerLength;
}

// TODO: 1. add method parser if necessary (for commands from the applications)
//		 2. Json parser of final result => only return parts of the result to the app?
//		 3. Take care of empty responses.
int get_json_from_http_response(char *httpResponse, char **jsonMsg)
{
	char *httpMsg = strstr(httpResponse, "\r\n\r\n");
	if (!httpMsg)
		httpMsg = httpResponse;
	else
		httpMsg += strlen("\r\n\r\n");
	
	char *sq_pos = strchr(httpMsg, '[');
	char *curl_pos = strchr(httpMsg, '{');
	char *jsonRsp = 0;
	char *endOfJsonRsp = 0;
	
	if( (sq_pos == NULL) && (curl_pos == NULL) )
	{
		printf("Error in reading json response\n");
		return -1;
	}
	else if( (sq_pos == NULL) || (curl_pos != NULL && sq_pos > curl_pos) )
	{
		jsonRsp = curl_pos;
		endOfJsonRsp = strrchr(jsonRsp, '}'); // find the last curly brace
	}
	else
	{
		jsonRsp = sq_pos;
		endOfJsonRsp = strrchr(jsonRsp, ']'); // find the last square brace
	}
	
	int jsonMsgLength = endOfJsonRsp - jsonRsp + 1; // length with braces
	if (jsonMsgLength >= 0)
	{
		*jsonMsg = (char *)malloc(sizeof(char) * (jsonMsgLength + 1));
		strncpy(*jsonMsg, jsonRsp, jsonMsgLength); // copy with braces included
		(*jsonMsg)[jsonMsgLength] = '\0';
	}
	else
	{
		printf("Error in reading json response\n");
		return -1;
	}
	/*
	printf("jsonRsp: %s\n", jsonRsp);
	printf("endOfJsonRsp: %s\n", endOfJsonRsp);
	printf("length of jsonRsp: %ld\n", endOfJsonRsp - jsonRsp + 1);
	*/
	return jsonMsgLength;
}

// returns the socket of connection to controller
int connect_to_controller(char *ctrlHost, int ctrlPort)
{
	int sockfd_ctrl;
	int connect_return;
	struct sockaddr_in controllerAddr;
	struct timeval tv;
	
	bzero(&controllerAddr, sizeof(controllerAddr));
	controllerAddr.sin_family = AF_INET;
	controllerAddr.sin_port = htons(ctrlPort);
	
	sockfd_ctrl = socket(AF_INET, SOCK_STREAM, 0);
	
	// set timeout to 20s;
	tv.tv_sec = 20;
	tv.tv_usec = 0;
	setsockopt(sockfd_ctrl, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));
	
	inet_pton(AF_INET, ctrlHost, &controllerAddr.sin_addr);
	
	connect_return = connect(sockfd_ctrl, (struct sockaddr *) &controllerAddr, sizeof(controllerAddr));
	
	if (connect_return < 0){
		perror("Connection to controller failed");
		return connect_return;
	}
	//printf("Connection to controller succeeded\n");
	return sockfd_ctrl;
}

// TODO: dynamically allocate response in send_command_to_controller, do not need responseLength as argument
// 		 read constant amount each time, until the end of message (this shouldn't be hard for GET, but for POST and DELETE? should be okay? Response is also in JSON)

// TODO: fail if 404?
// TODO: what's the best way of reading a long response?
int send_command_to_controller(int socket, char *command, char **response)
{
	const int buf_size = 1024 * 16;
	int ret_val;
	int jsonMsgLength = 0;
	int overallLength = 0;
	char *ctrlReturn = (char *)malloc(sizeof(char) * buf_size);
	char *msgStart;
	
	ret_val = write(socket, command, strlen(command));
	if (ret_val < 0) {
		return ret_val;
	}
	
	if (strncmp(command, "POST /", strlen("POST /")) == 0 ||      // POST or DELETE requests
	    strncmp(command, "DELETE /", strlen("DELETE /")) == 0)
	{
		bzero(ctrlReturn, buf_size);
		ret_val = read(socket, ctrlReturn, buf_size);
		if (ret_val < 0) 
		{
			free(ctrlReturn);
			return ret_val;
		}	
		
		*response = (char *)malloc(sizeof(char) * buf_size);
		strncpy(*response, ctrlReturn, ret_val);
	}
	else  // GET requests
	{
		/*bzero(ctrlReturn, buf_size);
		ret_val = read(socket, ctrlReturn, buf_size);
		if (ret_val < 0) 
		{
			free(ctrlReturn);
			return ret_val;
		}	
		
		*response = (char *)malloc(sizeof(char) * buf_size);
		strncpy(*response, ctrlReturn, ret_val);*/
		bzero(ctrlReturn, buf_size);
		ret_val = read(socket, ctrlReturn, buf_size);
		do
		{	
			//printf("\nController's response: %s\n", ctrlReturn);
		
			if (ret_val < 0)
			{
				free(ctrlReturn);
				return ret_val;
			}
			
			if (overallLength == 0)
			{
				/*msgStart = strstr(ctrlReturn, "\r\n\r\n");
				if (!msgStart)
				{
					free(ctrlReturn);
					return -1;
				}
				msgStart = strstr(msgStart + 4, "\r\n");
				if (!msgStart)
				{
					free(ctrlReturn);
					return -1;
				}
				msgStart += 2;*/
				msgStart = ctrlReturn;
			}
			else
			{
				/*msgStart = strstr(ctrlReturn, "\r\n");
				if (!msgStart)
				{
					free(ctrlReturn);
					return -1;
				}
				//msgStart += 2;*/
				msgStart = ctrlReturn;
			}
			//printf("msgStart: %s\n", msgStart);
			jsonMsgLength = ctrlReturn + ret_val - msgStart;
			overallLength += jsonMsgLength;
			*response = (char *)realloc(*response, sizeof(char) * overallLength);
			strncpy(*response + overallLength - jsonMsgLength, msgStart, jsonMsgLength);
			
			bzero(ctrlReturn, buf_size);
			ret_val = read(socket, ctrlReturn, buf_size);
		} while (strcmp(ctrlReturn, "0\r\n\r\n") != 0); // TODO: sometimes it sends the null ending (0\r\n\r\n) together! handle this if possible
		
		overallLength += 1;
		*response = (char *)realloc(*response, sizeof(char) * overallLength);
		(*response)[overallLength - 1] = '\0';
		
		//printf("final response of a single controller: %s\n", *response);
	}
	
	free(ctrlReturn);
	close(socket);
	return ret_val;
}

int communicate_to_controller(char *command, char **response, char *ctrlHost, int ctrlPort)
{
	int ret_val;
	int fd = 0;
	int jsonMsgLength = -1;
	int httpHeaderLength = 0;
	char *jsonRsp = 0;
	char *ctrlResponse = 0;
	char *httpHeader = 0;
	
	//*response = (char *)malloc(sizeof(char) * 1024 * 16);
	fd = connect_to_controller(ctrlHost, ctrlPort);
	if (fd >= 0)
	{
		ret_val = send_command_to_controller(fd, command, &ctrlResponse);
		//printf("Response returned after sending command: %s\n", ctrlResponse);
		if (ret_val < 0)
		{
			perror("Communication to controller failed");
			free(ctrlResponse);
			return ret_val;
		}
		
		jsonMsgLength = get_json_from_http_response(ctrlResponse, &jsonRsp);
		//printf("Json message length: %d\n", jsonMsgLength);
		if (jsonMsgLength > 0) {
			*response = (char *)malloc(sizeof(char) * (jsonMsgLength + 1));
			
			strncpy(*response, jsonRsp, jsonMsgLength);
			(*response)[jsonMsgLength] = '\0';
			//printf("response msg in json: %s\n", *response);
		}
	}
	
	get_proper_http_header(ctrlResponse, &httpHeader);
	httpHeaderLength = strlen(httpHeader);
	//printf("Http header test: %s\n", httpHeader);
	
	//char *httpHeader = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nDate: Sat, 28 Nov 2015 00:49:10 GMT\r\nAccept-Ranges: bytes\r\nServer: Restlet-Framework/2.3.1\r\nVary: Accept-Charset, Accept-Encoding, Accept-Language, Accept\r\nConnection: keep-alive\r\n\r\n";
	char *responseBak = (char *)malloc(sizeof(char) * (jsonMsgLength + 1));
	strcpy(responseBak, *response);
	*response = (char *)realloc(*response, sizeof(char) * (jsonMsgLength + 1 + httpHeaderLength));
	bzero(*response, sizeof(char) * (jsonMsgLength + 1 + httpHeaderLength));
	(*response)[0] = '\0';
	strcat(*response, httpHeader);
	strcat(*response, responseBak);
	
	//printf("response after adding httpHeader: %s\n", *response);
	
	free(responseBak);
	free(httpHeader);
	free(jsonRsp);
	free(ctrlResponse);
	return jsonMsgLength + httpHeaderLength;
}

// TODO: keep HTTP header or not?
int communicate_to_all_controllers(char *command, char **overallResponse)
{
	int i = 0;
	int ret_val;
	int jsonMsgLength, lengthToCopy;
	int overallResponseLength = 0;
	int fd;
	int copy_comma = 0;
	char last_brace;
	char *httpHeader = 0;
	char *jsonRsp = 0;
	//char response[1024 * 16];
	char *response = 0;
	
	for (i = 0; i < num_of_controllers; i++)
	{
		jsonRsp = 0;
		fd = connect_to_controller(controller_info[i].ip, controller_info[i].port);
		if (fd >= 0)
		{
			ret_val = send_command_to_controller(fd, command, &response);
			if (ret_val < 0)
			{
				free(response);
				perror("Communication to controller failed");
				if (i == 0)
					return ret_val;
				continue;
			}
			
			jsonMsgLength = get_json_from_http_response(response, &jsonRsp);
			//printf("json msg: %s\n", jsonRsp);
			if (i == 0)
			{
				if (jsonMsgLength < 0) {
					free(response);
					return -1;
				}
				else if (jsonMsgLength > 2)
					copy_comma = 1;
				
				get_proper_http_header(response, &httpHeader);
				//printf("Http header test: %s\n", httpHeader);
				
				lengthToCopy = jsonMsgLength - 1;
				overallResponseLength += jsonMsgLength - 1;
				*overallResponse =  (char *)malloc(sizeof(char) * overallResponseLength);
				strncpy(*overallResponse, jsonRsp, lengthToCopy);
				
			}
			else
			{
				if (jsonMsgLength < 0)
					continue;
				else if (jsonMsgLength <= 2) {
					free(jsonRsp);
					free(response);
					jsonRsp = 0;
					response = 0;
					continue;
				}
				
				lengthToCopy = jsonMsgLength - 2;
				if (!copy_comma) {
					overallResponseLength += jsonMsgLength - 2; // including the comma
					*overallResponse =  (char *)realloc(*overallResponse, sizeof(char) * overallResponseLength);
					copy_comma = 1;
				}
				else {
					overallResponseLength += jsonMsgLength - 1; // including the comma
					*overallResponse =  (char *)realloc(*overallResponse, sizeof(char) * overallResponseLength);
					strncpy(*overallResponse + overallResponseLength - lengthToCopy - 1, ",", strlen(","));
				}
				//overallResponseLength += jsonMsgLength - 1; // including the comma
				//*overallResponse =  (char *)realloc(*overallResponse, sizeof(char) * overallResponseLength);
				//strncpy(*overallResponse + overallResponseLength - lengthToCopy - 1, ",", strlen(","));
				strncpy(*overallResponse + overallResponseLength - lengthToCopy, jsonRsp + 1, lengthToCopy);
			}
			free(jsonRsp);
			free(response);
			jsonRsp = 0;
			response = 0;
		}
	}
	
	if (*overallResponse[0] == '[')
		last_brace = ']';
	else
		last_brace = '}';
		
	overallResponseLength += 1;
	*overallResponse =  (char *)realloc(*overallResponse, sizeof(char) * (overallResponseLength + 1)); // need to include the last null character now
	(*overallResponse)[overallResponseLength - 1] = last_brace;
	(*overallResponse)[overallResponseLength] = '\0';
	
	// prepend HTTP header
	char *responseBak = (char *)malloc(sizeof(char) * overallResponseLength);
	strcpy(responseBak, *overallResponse);
	
	overallResponseLength += strlen(httpHeader);
	*overallResponse = (char *)realloc(*overallResponse, sizeof(char) * (overallResponseLength + 1));
	bzero(*overallResponse, sizeof(char) * overallResponseLength);
	(*overallResponse)[0] = '\0';
	strcat(*overallResponse, httpHeader);
	strcat(*overallResponse, responseBak);
	(*overallResponse)[overallResponseLength] = '\0';
	//printf("Overall Response after adding httpHeader: %s\n", *overallResponse);
	
	free(responseBak);
	free(httpHeader);
	
	//printf("\noverallResponse: %s\n", *overallResponse);
	
	return overallResponseLength;
}