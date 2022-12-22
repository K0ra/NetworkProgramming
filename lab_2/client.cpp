#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>

#include "dns_protocol.h"

const int SIZE = 256;
const int val = 49152;  // 1100 0000 0000 0000

/*  Process the NAME section of Question and Answer packets
    'label' buffer strores the data, 'length' is the buffer's length.  */
void decompress(char* msg, char* label, int msg_length, int* length) {
  int i = msg_length, new_line = *length;

  while (msg[i] != '\0') {
    u_int ptr;
    memcpy(&ptr, msg + i, 2);
    ptr = ntohs(ptr);
    // If the mark starts with b11
    if (val <= ptr) {
      // Take the reference to the necessary byte of data
      int offset = ptr - val;
      *length = new_line;
      decompress(msg, label, offset, &new_line);
      *length += 2;
      return;
    } else {  // If the mark starts with b00
              // Take the length of current bytes to read
      int mark = msg[i++];
      // Record bytes to the label buffer
      for (int j = 0; j < mark; ++j) {
        label[new_line++] = msg[i++];
      }
      label[new_line++] = '.';
    }
  }

  *length = new_line + 1;
  label[new_line] = 0;
}

int main(int argc, char* argv[]) {
  // ./client dns_name [server] [port]
  // dns_name mandatory option
  // server by default localhost
  // port by default 53

  char domain_name[SIZE];
  char server[SIZE] = "localhost";
  char service_port[SIZE] = "53";

  if (argc < 2) {
    fprintf(stderr, "Usage: %s dns_name [server] [port]\n", argv[0]);
    return 1;
  }
  strncpy(domain_name, argv[1], strlen(argv[1]));

  if (argc > 2) {
    memset(server, 0, SIZE);
    strncpy(server, argv[2], strlen(argv[2]));
  }

  if (argc > 3) strncpy(service_port, argv[3], strlen(argv[3]));

  char send_message[2 * SIZE], recv_message[4 * SIZE];
  char buffer[SIZE];
  DNS_HEADER header;
  QUESTION question;

  memset(&header, 0, sizeof(header));

  header.q_count = htons(1);
  header.rcode = header.aa = header.tc = 0;
  header.rd = 1;  // ask to send only an IP address
  header.id = htons(0);
  header.opcode = header.qr = header.z = header.ra = 0;
  header.ans_count = header.auth_count = header.add_count = 0;

  memset(buffer, 0, SIZE);
  memset(send_message, 0, sizeof(send_message));
  memcpy(send_message, &header, sizeof(DNS_HEADER));
  int len_send = sizeof(DNS_HEADER);

  strncpy(buffer + 1, domain_name, strlen(argv[1]));

  // QNAME
  buffer[0] = '.';
  char count = 0;
  int length = strlen(buffer);
  for (int i = length - 1; i > -1; --i)
    if (buffer[i] == '.') {
      sprintf(buffer + i, "%c%s", count, buffer + i + 1);
      count = 0;
    } else {
      count++;
    }
  buffer[strlen(buffer)] = '\0';

  memcpy(send_message + len_send, buffer, length + 1);  // QNAME
  len_send += length;
  ++len_send;

  question.qclass = htons(1);   // QCLASS - IN
  question.qtype = htons(T_A);  // QTYPE - A
  memcpy(send_message + len_send, &question, sizeof(QUESTION));
  len_send += sizeof(QUESTION);

  struct addrinfo hints = {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  struct addrinfo* result = NULL;

  int rc = 0;
  if ((rc = getaddrinfo(server, service_port, &hints, &result)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
    return 1;
  }

  int sockfd = -1;
  struct addrinfo* p = NULL;
  for (p = result; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("client: socket");
      continue;
    }
    break;
  }
  if (!p) {
    perror("Addrinfo is NULL.\n");
    return 1;
  }

  int numbytes = 0;
  if ((numbytes = sendto(sockfd, send_message, len_send, 0, p->ai_addr,
                         p->ai_addrlen)) == -1) {
    perror("client: sendto");
    return 1;
  }

  // Set the timeout for 1 sec
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  // SO_RCVTIMEO - a flag specifying the receiving timeout
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  struct sockaddr_storage serv_addr;
  unsigned int serv_addr_len = sizeof(serv_addr);

  int received =
      recvfrom(sockfd, &recv_message, sizeof(recv_message), 0,
               (struct sockaddr*)&serv_addr, (socklen_t*)&serv_addr_len);

  if (received < 0) {
    perror("clent: recvfrom");
    return 1;
  }

  freeaddrinfo(result);
  close(sockfd);

  memcpy(&header, recv_message, sizeof(DNS_HEADER));
  if (header.rcode != 0) {
    fprintf(stderr, "Server did not response!\n");
    return 1;
  }

  int ans_count = ntohs(header.ans_count);

  // Skip the Header and Question sections
  int offset = sizeof(DNS_HEADER), qname_len = 0;

  memset(buffer, 0, sizeof(buffer));
  decompress(recv_message, buffer, offset, &qname_len);
  offset += qname_len + sizeof(QUESTION);

  // Process an Answer section
  if (ans_count > 0) {
    R_DATA rr;
    char name[SIZE];
    int length = 0;

    decompress(recv_message, name, offset, &length);
    offset += length;
    memcpy(&rr, recv_message + offset, sizeof(R_DATA));
    offset += sizeof(R_DATA);
    rr.type = ntohs(rr.type);

    if (rr.type != T_A) {
      fprintf(stderr, "Not an A-type record!\n");
      return 1;
    }

    printf("%hhu.%hhu.%hhu.%hhu\n", recv_message[offset],
           recv_message[offset + 1], recv_message[offset + 2],
           recv_message[offset + 3]);
  }

  return 0;
}
