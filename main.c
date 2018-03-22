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
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include "wrapperTypes.h"
#include "handleRequest.h"

//TODO: move those to header files
#define	PORT_RANGE_LO	10000
#define PORT_RANGE_HI	10100

int delete_controller_info();
int update_controller_info(char *str);
int mybind(int sockfd, struct sockaddr_in *addr);
void *handle_app_request(void *); //the thread function for processing application commands
void *receive_reg_update(void *);

const int SWITCH_DPID_LEN = 23;
unsigned short usable_port_lo;

pthread_mutex_t lock;
int num_of_controllers = 0;
ctrl_info *controller_info;

int test_init_ctrl_info()
{
	controller_info = (ctrl_info*)malloc(sizeof(ctrl_info) * 3);
	int i = 0;
	
	num_of_controllers = 1;
	
	for (i = 0; i < 1; i++)
	{
		controller_info[i].ip = (char*)malloc(sizeof(char) * 16);
		strcpy(controller_info[i].ip, "142.150.235.18");
	}
	//strcpy(controller_info[2].ip, "142.150.235.19");
	
	controller_info[0].num_switch_connected = 5;
	controller_info[0].switch_dpid = (char**)malloc(sizeof(char*) * 5);
	/*controller_info[1].num_switch_connected = 2;
	controller_info[1].switch_dpid = (char**)malloc(sizeof(char*) * 2);
	controller_info[2].num_switch_connected = 2;
	controller_info[2].switch_dpid = (char**)malloc(sizeof(char*) * 2);*/
	
	int j = 0;
	for (j= 0; j < 5; j++)
	{
		controller_info[0].switch_dpid[j] = (char*)malloc(sizeof(char) * SWITCH_DPID_LEN);
	}
	/*for (j= 0; j < 2; j++)
	{
		controller_info[1].switch_dpid[j] = (char*)malloc(sizeof(char) * SWITCH_DPID_LEN);
	}
	for (j= 0; j < 2; j++)
	{
		controller_info[2].switch_dpid[j] = (char*)malloc(sizeof(char) * SWITCH_DPID_LEN);
	}*/
	
	
	controller_info[0].port = 8080;
	//controller_info[1].port = 8082;
	//controller_info[2].port = 8080;
	
	controller_info[0].switch_dpid[0] = "00:00:00:00:00:00:00:01";
	controller_info[0].switch_dpid[1] = "00:00:00:00:00:00:00:02";
	controller_info[0].switch_dpid[2] = "00:00:00:00:00:00:00:03";
	controller_info[0].switch_dpid[3] = "00:00:00:00:00:00:00:04";
	controller_info[0].switch_dpid[4] = "00:00:00:00:00:00:00:05";
	/*controller_info[1].switch_dpid[0] = "00:00:00:00:00:00:00:02";
	controller_info[1].switch_dpid[1] = "00:00:00:00:00:00:00:03";
	controller_info[2].switch_dpid[0] = "00:00:00:00:00:00:00:04";
	controller_info[2].switch_dpid[1] = "00:00:00:00:00:00:00:05";*/
	
	//printf("controller_info[0].switch_dpid[0]: %s\n", controller_info[0].switch_dpid[0]);
	
	return 0;
}

// TODO: have a way to exit safely
int main(int argc, char *argv[])
{
	//update_controller_info("[127.15.12.14\\8090\\{00:00:00:00:00:00:00:00/00:00:00:00:00:00:00:02/}], [137.15.12.54\\8092\\{00:00:00:00:00:00:00:03/}], [147.15.12.54\\10000\\{00:00:00:00:00:00:00:05/}]");
	usable_port_lo = PORT_RANGE_LO;

	int listenfd, connfd;
	pthread_t thread_id;
	struct sockaddr_in wrapperAddr, appAddr;
	socklen_t length;
	
	length = sizeof(appAddr);
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	bzero(&wrapperAddr, sizeof(wrapperAddr));
	wrapperAddr.sin_family = AF_INET;
	wrapperAddr.sin_port = htons(0);
	wrapperAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	test_init_ctrl_info();
	
	if (mybind(listenfd, &wrapperAddr) == -1)
	  return 1;
	
	printf("Port: %u\n", htons(wrapperAddr.sin_port));
	usable_port_lo = htons(wrapperAddr.sin_port) + 1;
	
	//int registry_fd = socket(AF_INET, SOCK_DGRAM, 0);

	listen(listenfd, 5);
	
	printf("Waiting for connection\n");
	
	// TODO: having a thread of receiving info from the registry so it can update the controller info data
	if( pthread_create( &thread_id, NULL, receive_reg_update, (void*)0 ) < 0 ){
        perror("could not create thread for receiving controller info update!");
        return 1;
    }
		
	while (1) {
		connfd = accept(listenfd, (struct sockaddr *)&appAddr, &length);
		
		if (connfd < 0)
			perror("Failed to accept app connection");
		else if( pthread_create( &thread_id, NULL, handle_app_request, (void*)&connfd ) < 0 ){
            perror("could not create thread");
		}
	}
	
	//close(connfd);
	return 0;
}

int delete_controller_info()
{
	int i = 0;
	pthread_mutex_lock(&lock);
	for (i = 0; i < num_of_controllers; i++)
	{
		free(controller_info[i].ip);
		
		int j = 0;
		for (j= 0; j < controller_info[i].num_switch_connected; j++)
		{
			free(controller_info[i].switch_dpid[j]);
		}
		free(controller_info[i].switch_dpid);
	}
	free(controller_info);
	pthread_mutex_unlock(&lock);
	return 0;
}

int copy_controller_info(ctrl_info *info2, int total_ctrl_num)
{
	int i, j;
	pthread_mutex_lock(&lock);
	controller_info = (ctrl_info*)malloc(sizeof(ctrl_info) * total_ctrl_num);
	for (i = 0; i < total_ctrl_num; i++)
	{
		controller_info[i].ip = (char*)malloc(sizeof(char) * 16);
		strcpy(controller_info[i].ip, info2[i].ip);
		controller_info[i].port = info2[i].port;
		
		controller_info[i].num_switch_connected = info2[i].num_switch_connected;
		controller_info[i].switch_dpid = (char**)malloc(sizeof(char*) * controller_info[i].num_switch_connected);
		
		printf("Main controller %d ip: %s\n", i, controller_info[i].ip);
		printf("Main controller %d port: %d\n", i, controller_info[i].port);
		printf("Main controller %d number of switches: %d\n", i, controller_info[i].num_switch_connected);
			
		for (j= 0; j < info2[i].num_switch_connected; j++)
		{
			controller_info[i].switch_dpid[j] = (char*)malloc(sizeof(char) * SWITCH_DPID_LEN);
			strcpy(controller_info[i].switch_dpid[j], info2[i].switch_dpid[j]);
			
			printf("Main controller %d switch %d dpid: %s\n", i, j, controller_info[i].switch_dpid[j]);
		}
	}
	pthread_mutex_unlock(&lock);
	return 0;
}

int update_controller_info(char *str)
{
	// TODO: back up before update, create a function of copying the controller info
	char *p1, *p2, *p3, *p4;
	int total_controller_num = 0;
	int switch_count = 0;
	int i, j;
	
	p2 = str;
	
	while ((p1 = strchr(p2, '[')))
	{
		if (p1)
			p2 = strchr(p1, ']');
		
		if (p2)
			total_controller_num++;
		else
		{
			printf("Update controller info failed: error in message format 1.\n");
			return 1;
		}
	}
	
	printf("total controller num: %d\n", total_controller_num);
	ctrl_info *info = (ctrl_info*)malloc(sizeof(ctrl_info) * total_controller_num);
	
	// NOTE: this is not secure
	p2 = str;
	for (i = 0; i < total_controller_num; i++)
	{
		p1 = strchr(p2, '[');
		p3 = strchr(p2, ']');
		p2 = strchr(p1, '\\');
		
		printf("p1 : %s\n", p1);
		printf("p2 : %s\n", p2);
		printf("p3 : %s\n", p3);
		
		p4 = p3; // p4: location of ']'
		if (p2 && p2 < p3) {
			p3 = strchr(p2+1, '\\');
		}
		else {
			// TODO: ideally free all the allocated info
			printf("Update controller info failed: error in message format 2.\n");
			return 1;
		}
		
		if (p3 && p3 < p4) {	
			info[i].ip = (char*)malloc(sizeof(char) * 16);
			bzero(info[i].ip, sizeof(char) * 16);
			strncpy(info[i].ip, p1 + 1, p2 - p1 - 1);
			info[i].ip[16] = '\0';
			info[i].port = atoi(p2+1);		// TODO: make sure this works
		}
		else {
			// TODO: ideally free all the allocated info
			printf("Update controller info failed: error in message format 3.\n");
			return 1;
		}
		
		printf("Before switch count\n");
		
		switch_count = 0;
		p1 = strchr(p3, '{');	// p3: '\' after port number, p4: location of ']' 
		if (p1 && p1 < p4) {
			p2 = strchr(p1, '}');
			
			if (!p2 || p2 > p4) {
				// TODO: ideally free all the allocated info
				printf("Update controller info failed: error in message format 4.\n");
				return 1;
			}
		
			p3 = p1 + 1;	// p1: '{';
			p3 = strchr(p3, '/');
		
			switch_count = 0;
			while (p3 && p3 < p2){		// p2: '}'
				switch_count++;
				p3 = strchr(p3 + 1, '/');
			}
		
			info[i].num_switch_connected = switch_count;
			info[i].switch_dpid = (char**)malloc(sizeof(char*) * switch_count);
		
			p1 += 1;
			for (j= 0; j < switch_count; j++)
			{
				p3 = strchr(p1, '/');
				info[i].switch_dpid[j] = (char*)malloc(sizeof(char) * (SWITCH_DPID_LEN + 1));
				strncpy(info[i].switch_dpid[j], p1, SWITCH_DPID_LEN); // NOTE: assumes the length matches
				info[i].switch_dpid[j][SWITCH_DPID_LEN] = '\0';
				p1 = p3 + 1;
			}
		}
		
		p2 = p4 + 1;
		printf("End of loop\n");
	}
	
	delete_controller_info();
	copy_controller_info(info, total_controller_num);
	
	for (i = 0; i < total_controller_num; i++)
	{
		printf("Controller %d ip: %s\n", i, info[i].ip);
		printf("Controller %d port: %d\n", i, info[i].port);
		printf("Controller %d number of switches: %d\n", i, info[i].num_switch_connected);
		
		free(info[i].ip);
		
		for (j= 0; j < info[i].num_switch_connected; j++)
		{
			printf("Controller %d switch %d dpid: %s\n", i, j, info[i].switch_dpid[j]);
			free(info[i].switch_dpid[j]);
		}
		free(info[i].switch_dpid);
	}
	free(info);
	
	return 0;
}

void *receive_reg_update(void *args)
{
	//int listenfd = *(int*)listenSock;
	const int buf_size = 1024 * 16;
	int ret_val;
	int listenfd, connfd, hellofd;
	int conn_return = -1;
	char hi_message[30];
	char registry_msg[buf_size];
	char ip_addr[NI_MAXHOST];
	socklen_t length;
	struct sockaddr_in receiverAddress, regAddress;
	
	hellofd = socket(AF_INET, SOCK_STREAM, 0);
	bzero(&regAddress, sizeof(regAddress));
	regAddress.sin_family = AF_INET;
	regAddress.sin_port = htons(7070);
	
	inet_pton(AF_INET, "100.64.222.209", &regAddress.sin_addr);
	
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	bzero(&receiverAddress, sizeof(receiverAddress));
	receiverAddress.sin_family = AF_INET;
	receiverAddress.sin_port = htons(0);
	receiverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	length = sizeof(receiverAddress);
	
	if (mybind(listenfd, &receiverAddress) == -1)
	{
		printf("Initializing socket for receiving updates from registry failed\n");
		exit(1); // TODO: does this work?
	}
	printf("TCP receiving update port: %u\n", htons(receiverAddress.sin_port));
	listen(listenfd, 1);
	
	while (conn_return != 0)
		conn_return = connect(hellofd, (struct sockaddr *) &regAddress, sizeof(regAddress));
		
	printf("connected to the registry\n");
	ret_val = write(hellofd, "whi:100.64.188.102,10001\n", strlen("whi:100.64.188.102,10001\n"));
	printf("Return of sending message: %d\n", ret_val);
	close(hellofd);
	
	while (1) {
		/*connfd = accept(listenfd, (struct sockaddr *)&regAddress, &length);
		
		if (connfd < 0)
			perror("Failed to accept registry connection");*/
		

		
		connfd = accept(listenfd, (struct sockaddr *)&receiverAddress, &length);
		
		if (connfd < 0)
			perror("Failed to accept registry connection");
		
		while (1) {
			bzero(registry_msg, buf_size);
			ret_val = read(connfd, registry_msg, buf_size);
			if (ret_val == 0)
				continue;
			printf("Read registry message, ret_val: %d\n", ret_val);
			printf("Registry msg: %s\n", registry_msg);
			
			if (ret_val > 0 && !strcmp(registry_msg, "notify")) {
				write(connfd, "pull", strlen("pull"));
			}
			else if (ret_val > 0 && !strcmp(registry_msg, "n")){	// TODO: why only receiving the 1st character in 1st read
				bzero(registry_msg, buf_size);
				read(connfd, registry_msg, buf_size);
				printf("Registry msg: %s\n", registry_msg);
				write(connfd, "pull\n", strlen("pull\n"));	// TODO: why write does not work?
			}
			else
				continue;
			
			bzero(registry_msg, buf_size);	
			ret_val = read(connfd, registry_msg, buf_size);
			if (ret_val > 0) {
				printf("Registry msg: %s\n", registry_msg);
				update_controller_info(registry_msg);
			}
		}
	}
	
	return 0;
}

// TODO: currently does not work on U of T wireless network
int get_ip_addr(char *ip_addr)
{
	int i = 0;
	int sa_family;
	//char host[NI_MAXHOST];
    struct ifaddrs *if_addr, *addr;
    
	if (getifaddrs(&if_addr) == -1)
	{
	  perror("getifaddrs");
	  return -1;
	}
	
	// get ipv4 of server
	for (addr = if_addr, i = 0; addr != NULL; addr = addr->ifa_next, i++)
	{
	  sa_family = addr->ifa_addr->sa_family;
	  if (strcmp(addr->ifa_name, "lo") != 0 && sa_family == AF_INET)
	  {
		int r;
	    r = getnameinfo(addr->ifa_addr, sizeof(struct sockaddr_in), ip_addr, 
		            NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
		
		if (r != 0)
		{
		  perror("getnameinfo");
		  return r;
		}
		
		break;
	  }
	}
	freeifaddrs(if_addr);
	
	return 0;
}

/* 
 * mybind() -- a wrapper to bind that tries to bind() to a port in the
 * range PORT_RANGE_LO - PORT_RANGE_HI, inclusive.
 *
 * Parameters:
 *
 * sockfd -- the socket descriptor to which to bind
 *
 * addr -- a pointer to struct sockaddr_in. mybind() works for AF_INET sockets only.
 * Note that addr is and in-out parameter. That is, addr->sin_family and
 * addr->sin_addr are assumed to have been initialized correctly before the call.
 * Also, addr->sin_port must be 0, or the call returns with an error. Up on return,
 * addr->sin_port contains, in network byte order, the port to which the call bound
 * sockfd.
 *
 * returns int -- negative return means an error occurred, else the call succeeded.
 */
int mybind(int sockfd, struct sockaddr_in *addr) {
    if(sockfd < 1) {
		fprintf(stderr, "mybind(): sockfd has invalid value %d\n", sockfd);
		return -1;
    }

    if(addr == NULL) {
		fprintf(stderr, "mybind(): addr is NULL\n");
		return -1;
    }

    if(addr->sin_port != 0) {
		fprintf(stderr, "mybind(): addr->sin_port is non-zero. Perhaps you want bind() instead?\n");
		return -1;
    }

    unsigned short p;
    for(p = usable_port_lo; p <= PORT_RANGE_HI; p++) {
		addr->sin_port = htons(p);
		int b = bind(sockfd, (const struct sockaddr *)addr, sizeof(struct sockaddr_in));
		if(b < 0) {
		    continue;
		}
		else {
	    	break;
		}
    }

    if(p > PORT_RANGE_HI) {
		fprintf(stderr, "mybind(): all bind() attempts failed. No port available...\n");
		return -1;
    }

    /* Note: upon successful return, addr->sin_port contains, in network byte order, the
     * port to which we successfully bound. */
    return 0;
}
