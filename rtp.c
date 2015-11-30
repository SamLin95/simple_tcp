#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "rtp.h"

/* GIVEN Function:
 * Handles creating the client's socket and determining the correct
 * information to communicate to the remote server
 */
CONN_INFO* setup_socket(char* ip, char* port){
	struct addrinfo *connections, *conn = NULL;
	struct addrinfo info;
	memset(&info, 0, sizeof(struct addrinfo));
	int sock = 0;

	info.ai_family = AF_INET;
	info.ai_socktype = SOCK_DGRAM;
	info.ai_protocol = IPPROTO_UDP;
	getaddrinfo(ip, port, &info, &connections);

	/*for loop to determine corr addr info*/
	for(conn = connections; conn != NULL; conn = conn->ai_next){
		sock = socket(conn->ai_family, conn->ai_socktype, conn->ai_protocol);
		if(sock <0){
			if(DEBUG)
				perror("Failed to create socket\n");
			continue;
		}
		if(DEBUG)
			printf("Created a socket to use.\n");
		break;
	}
	if(conn == NULL){
		perror("Failed to find and bind a socket\n");
		return NULL;
	}
	CONN_INFO* conn_info = (CONN_INFO*)malloc(sizeof(CONN_INFO));
	conn_info->socket = sock;
	conn_info->remote_addr = conn->ai_addr;
	conn_info->addrlen = conn->ai_addrlen;
	return conn_info;
}

void shutdown_socket(CONN_INFO *connection){
	if(connection)
		close(connection->socket);
}

/* 
 * ===========================================================================
 *
 *			STUDENT CODE STARTS HERE. PLEASE COMPLETE ALL FIXMES
 *
 * ===========================================================================
 */


/*
 *  Returns a number computed based on the data in the buffer.
 */
static int checksum(char *buffer, int length){

	/*  ----  FIXME  ----
	 *
	 *  The goal is to return a number that is determined by the contents
	 *  of the buffer passed in as a parameter.  There a multitude of ways
	 *  to implement this function.  For simplicity, simply sum the ascii
	 *  values of all the characters in the buffer, and return the total.
	 */
	 int sum = 0;
	 for (int i = 0; i < length; i++) {
	 	sum += (int)buffer[i];
	 }
	 return sum; 
}

/*
 *  Converts the given buffer into an array of PACKETs and returns
 *  the array.  The value of (*count) should be updated so that it 
 *  contains the length of the array created.
 */
static PACKET* packetize(char *buffer, int length, int *count){

	/*  ----  FIXME  ----
	 *  The goal is to turn the buffer into an array of packets.
	 *  You should allocate the space for an array of packets and
	 *  return a pointer to the first element in that array.  Each 
	 *  packet's type should be set to DATA except the last, as it 
	 *  should be LAST_DATA type. The integer pointed to by 'count' 
	 *  should be updated to indicate the number of packets in the 
	 *  array.
	 */
	int packet_num, last_size, i, j, start, max_offset, sum;
	PACKET *packets; 
	packet_num = length / MAX_PAYLOAD_LENGTH;
	last_size = length % MAX_PAYLOAD_LENGTH;
	if (last_size != 0) {
		packet_num += 1;
	} else {
		last_size = MAX_PAYLOAD_LENGTH;
	}
	*count = packet_num;
	packets = (PACKET*)calloc(packet_num, sizeof(PACKET));
	for (i = 0; i < packet_num; i++) {
		if (i == packet_num - 1) {
			packets[i].type = LAST_DATA;
			packets[i].payload_length = last_size;
		} else {
			packets[i].type = DATA;
			packets[i].payload_length = MAX_PAYLOAD_LENGTH;
		}
		start = i * MAX_PAYLOAD_LENGTH;
		max_offset = packets[i].payload_length;
		for (j = 0; j < max_offset; j++) {
			(packets[i].payload)[j] = buffer[j + start];
			sum += buffer[start + j];
		}
		packets[i].checksum = sum;
		(packets[i].payload)[j] = '\0';
	}
	return packets;

}

/*
 * Send a message via RTP using the connection information
 * given on UDP socket functions sendto() and recvfrom()
 */
int rtp_send_message(CONN_INFO *connection, MESSAGE*msg){
	/* ---- FIXME ----
	 * The goal of this function is to turn the message buffer
	 * into packets and then, using stop-n-wait RTP protocol,
	 * send the packets and re-send if the response is a NACK.
	 * If the response is an ACK, then you may send the next one
	 */
	 int *count = (int*)malloc(sizeof(int));
	 PACKET *packets = packetize(msg->buffer, msg->length, count);

	 PACKET* packet_buffer = (PACKET*)malloc(sizeof(PACKET));

	 int socket = connection -> socket;
	 struct sockaddr *src_addr = connection -> remote_addr;
	 socklen_t addrlen = connection -> addrlen;
	 socklen_t *addrlen_ptr = &addrlen;

	 int packet_rem = *count;

	 while(1) {
	 	sendto(socket, packets, sizeof(PACKET), 0, src_addr, addrlen);
	 	recvfrom(socket, packet_buffer, sizeof(PACKET), 0, src_addr, addrlen_ptr);
	 	if (packet_buffer -> type == ACK) {
	 		packets += 1;
	 		packet_rem--;
	 	} else if (packet_buffer -> type == NACK) {
	 		continue;		
	 	} else {
	 		fprintf(stderr, "%s\n", "return unknown type");
	 		return -1;
		}
		if (packet_rem == 0) {
			return 0;
		}
	 }

}

/*
 * Receive a message via RTP using the connection information
 * given on UDP socket functions sendto() and recvfrom()
 */
MESSAGE* rtp_receive_message(CONN_INFO *connection){
	/* ---- FIXME ----
	 * The goal of this function is to handle 
	 * receiving a message from the remote server using
	 * recvfrom and the connection info given. You must 
	 * dynamically resize a buffer as you receive a packet
	 * and only add it to the message if the data is considered
	 * valid. The function should return the full message, so it
	 * must continue receiving packets and sending response 
	 * ACK/NACK packets until a LAST_DATA packet is successfully 
	 * received.
	 */
	 int socket = connection->socket;
	 struct sockaddr* src_addr = connection->remote_addr;
	 socklen_t addrlen = connection -> addrlen;
	 socklen_t *addrlen_ptr = &(addrlen);
	 PACKET* packet_buffer = (PACKET*)malloc(sizeof(PACKET));

	 char* message_buffer = (char*)malloc(sizeof(char));
	 MESSAGE* message = (MESSAGE*)malloc(sizeof(MESSAGE));
	 message -> length = 0;
	 
	int cur_buffer_index;
	while (1) {
		recvfrom(socket, packet_buffer, sizeof(PACKET), 0, src_addr, addrlen_ptr);
		if (checksum(packet_buffer->payload, packet_buffer->payload_length) != packet_buffer->checksum) {
			PACKET* nak_pac = (PACKET*)malloc(sizeof(PACKET));
			nak_pac -> type = NACK;
			sendto(socket, nak_pac, sizeof(PACKET), 0, src_addr, addrlen); 
		} else {
			message_buffer = (char*)realloc(message_buffer, sizeof(char)*(packet_buffer->payload_length + message->length));
			for (int j = 0; j < packet_buffer -> payload_length;j++) {
				message_buffer[cur_buffer_index++] = (packet_buffer -> payload)[j];
			}
			message->length = message->length + packet_buffer -> payload_length;
			PACKET *ack_pac = (PACKET*)malloc(sizeof(PACKET));
			ack_pac -> type = ACK;
			sendto(socket, ack_pac, sizeof(PACKET), 0, src_addr, addrlen);
			if (packet_buffer -> type == LAST_DATA) {
				message_buffer[cur_buffer_index] = '\0';
				break;
			}

		}

	}
	message -> buffer = message_buffer;
	return message;	 
}
