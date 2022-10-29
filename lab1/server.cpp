#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

#include <pthread.h>
#include <string.h>
#include <time.h>

#include "tcp-commn.h"

using namespace std;

#define DEFAULT_BUF_SIZE 512
#define DEFAULT_NICK_SIZE 32
#define DEFAULT_BYTES_SIZE 4
#define MAX_CLIENTS 10

struct client_t {
  int id;
  int sockfd;
  char nickname[DEFAULT_NICK_SIZE];
};

static unsigned int clients_num = 0;
client_t *clients[MAX_CLIENTS];  // ID-s start from 1
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Add clients to queue */
void add_client(client_t *cli) {
  pthread_mutex_lock(&clients_mutex);

  for (int i = 0; i < MAX_CLIENTS; ++i) {
    if (!clients[i]) {
      clients[i] = cli;
      clients[i]->id = i + 1;
      break;
    }
  }

  pthread_mutex_unlock(&clients_mutex);
}

/* Remove clients from queue */
void remove_client(int id) {
  pthread_mutex_lock(&clients_mutex);

  for (int i = 0; i < MAX_CLIENTS; ++i) {
    if (clients[i]) {
      if (clients[i]->id == id) {
        clients[i] = NULL;
        break;
      }
    }
  }

  pthread_mutex_unlock(&clients_mutex);
}

/*
  Sends the message by protocol standards:
  message size followed by message body
*/
int send_msg(int sockfd, char *msg) {
  uint32_t msg_size = htonl(strlen(msg));
  // printf("From Server :: Message: %s, msg_size=%li\n", msg, strlen(msg));
  int res = tcp_send(sockfd, (char *)&msg_size, DEFAULT_BYTES_SIZE);
  if (res == 0) {
    res = tcp_send(sockfd, msg, strlen(msg));
    if (res < 0) return -1;
  } else
    return -1;

  return 0;
}

/* Send message to all clients */
void broadcast_message(char *s) {
  pthread_mutex_lock(&clients_mutex);

  for (int i = 0; i < MAX_CLIENTS; ++i) {
    if (clients[i]) {
      if (send_msg(clients[i]->sockfd, s) < 0) {
        perror("ERROR: send descriptor failed");
        continue;
      }
    }
  }

  pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg) {
  char buffer[DEFAULT_BUF_SIZE];
  int leave_flag = 0;
  uint32_t nickname_size = 0;
  uint32_t msg_size = 0;

  clients_num++;
  client_t *cli = (client_t *)arg;

  while (true) {
    if (leave_flag) {
      break;
    }
    // Get the nick size
    int receive =
        tcp_recv(cli->sockfd, (char *)&nickname_size, DEFAULT_BYTES_SIZE);

    if (receive == 0 && nickname_size > 0) {
      // If no error, get the nickname
      nickname_size = ntohl(nickname_size);
      // printf("Nickname size=%d\n", nickname_size);

      char *nick_buf = (char *)malloc(sizeof(char) * (nickname_size + 1));
      memset(nick_buf, 0, (nickname_size + 1));
      receive = tcp_recv(cli->sockfd, nick_buf, nickname_size);

      if (receive == 0) {
        // printf("Nickname: %s\n", nick_buf);

        // If no error, assign nickname to client object
        strncpy(cli->nickname, nick_buf, nickname_size + 1);
        // Get the message size
        receive = tcp_recv(cli->sockfd, (char *)&msg_size, DEFAULT_BYTES_SIZE);

        if (receive == 0 && msg_size > 0) {
          // If no error, get the message
          msg_size = ntohl(msg_size);
          // printf("Message size=%d\n", msg_size);

          char *msg_buf = (char *)malloc(sizeof(char) * (msg_size + 1));
          memset(msg_buf, 0, (msg_size + 1));
          receive = tcp_recv(cli->sockfd, msg_buf, msg_size);

          if (receive == 0) {
            // printf("Message: %s\n", msg_buf);
            time_t t = time(NULL);
            struct tm *lt = localtime(&t);
            sprintf(buffer, "%02d:%02d", lt->tm_hour, lt->tm_min);

            broadcast_message(nick_buf);
            broadcast_message(msg_buf);
            broadcast_message(buffer);
          }
          free(msg_buf);
        }
      }
      free(nick_buf);
    } else if (receive == 0) {
      sprintf(buffer, "Client with id %d has left\n", cli->id);
      printf("%s", buffer);
      leave_flag = 1;
    } else {
      printf("ERROR: -1\n");
      sprintf(buffer, "Client with id %d has left\n", cli->id);
      printf("%s", buffer);
      leave_flag = 1;
    }
    memset(buffer, 0, DEFAULT_BUF_SIZE);
  }

  /* Delete client from queue and yield thread */
  close(cli->sockfd);
  remove_client(cli->id);
  free(cli);
  clients_num--;
  pthread_detach(pthread_self());

  return NULL;
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  int server_sockfd, newsockfd;
  uint16_t portno;
  struct sockaddr_in serv_addr, cli_addr;
  pthread_t tid;

  // Check arguments
  if (argc < 2) {
    fprintf(stderr, "You forgot to enter the port number for %s.\n", argv[0]);
    exit(EXIT_SUCCESS);
  }

  portno = (uint16_t)atoi(argv[1]);

  // Initialize socket
  server_sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (server_sockfd < 0) {
    perror("ERROR opening a server socket");
    exit(EXIT_FAILURE);
  }

  // Initialize socket structure
  bzero((char *)&serv_addr, sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;  // inet_addr(ip.c_str());
  serv_addr.sin_port = htons(portno);

  // Bind
  bind(server_sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

  // Listen for up to 5 requests at a time
  printf("Listening...\n");
  listen(server_sockfd, 5);

  printf(">>>> WELCOME TO THE CHATROOM <<<<\n");

  // Accept actual connections from the clients
  while (true) {
    unsigned int clilen = sizeof(cli_addr);
    newsockfd = accept(server_sockfd, (struct sockaddr *)&cli_addr, &clilen);

    if (newsockfd < 0) {
      perror("ERROR on accepting a client socket.");
      continue;
    }

    if (clients_num < MAX_CLIENTS) {
      // Add a client
      client_t *cli = (client_t *)malloc(sizeof(client_t));
      cli->sockfd = newsockfd;
      add_client(cli);
      printf("Client with id %d has joined the chat.\n", cli->id);

      // Create a thread process for that client
      pthread_create(&tid, NULL, &handle_client, (void *)cli);
      pthread_detach(tid);
    } else {
      printf("Server is full.\n");
      close(newsockfd);
      continue;
    }
  }

  // Close listening socket
  close(server_sockfd);
  pthread_exit(NULL);

  printf("Program has ended successfully.\n");

  return 0;
}
