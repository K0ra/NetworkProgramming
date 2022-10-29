// #include <iostream>
#include <stdio.h>
#include <stdlib.h>

#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "tcp-commn.h"

using namespace std;

#define DEFAULT_BUF_SIZE 512
#define DEFAULT_NICK_SIZE 32
#define DEFAULT_BYTES_SIZE 4

int sockfd = 0;  // server socket
char *nickname;  // client nickname
pthread_mutex_t msg_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Remove newline character from the string */
void str_trim_lf(char *arr, int length) {
  for (int i = 0; i < length; i++) {
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}

/*
  Sends the message by protocol standards:
  message size followed by message body
*/
int send_msg(char *msg) {
  uint32_t msg_size = htonl(strlen(msg));
  int res = tcp_send(sockfd, (char *)&msg_size, DEFAULT_BYTES_SIZE);
  if (res < 0) return -1;
  res = tcp_send(sockfd, msg, strlen(msg));
  if (res < 0) return -1;

  return 0;
}

void *send_msg_handler(void *args) {
  if (args != NULL) exit(EXIT_FAILURE);
  char msg[DEFAULT_BUF_SIZE] = {};
  memset(msg, 0, DEFAULT_BUF_SIZE);

  while (true) {
    printf("Enter a symbol 'm' to type a message:\n");

    char ch[5];
    // matches all characters not equal to '\n',
    // not exceeding buff_size - 1, and stores it
    if (scanf(" %4[^\n]%*c", ch) < 0) {
      perror("ERROR on scanf()");
      exit(EXIT_FAILURE);
    }
    fflush(stdin);

    if (strcmp(ch, "m\0") == 0) {
      pthread_mutex_lock(&msg_mutex);

      printf("Enter a message:\n");
      if (!fgets(msg, DEFAULT_BUF_SIZE, stdin)) {
        perror("ERROR on fgets()");
        exit(EXIT_FAILURE);
      }
      // printf("Message to send from client: %s\n", msg);

      pthread_mutex_unlock(&msg_mutex);

      str_trim_lf(msg, sizeof(msg));

      if (strlen(msg) == 0)
        printf("Empty message! There's nothing to send.\n");
      else {
        if ((send_msg(nickname) < 0) || (send_msg(msg) < 0)) {
          perror("ERROR writing to socket");
        }
      }
    }
    memset(msg, 0, DEFAULT_BUF_SIZE);
  }
  pthread_exit(NULL);
}

/* Just receive a message from a server and output it */
void *recv_msg_handler(void *args) {
  if (args != NULL) exit(EXIT_FAILURE);

  int leave_flag = 0;
  uint32_t nickname_size = 0;
  uint32_t msg_size = 0;
  uint32_t date_size = 0;
  char *nick_buf = NULL;
  char *msg_buf = NULL;
  char *date_buf = NULL;

  while (true) {
    if (leave_flag) break;

    // Get the nick size
    int receive = tcp_recv(sockfd, (char *)&nickname_size, DEFAULT_BYTES_SIZE);

    if (receive == 0 && nickname_size > 0) {
      // If no error, get the nickname
      nickname_size = ntohl(nickname_size);
      // printf("Nickname size=%d\n", nickname_size);

      nick_buf = (char *)malloc(sizeof(char) * (nickname_size + 1));
      memset(nick_buf, 0, (nickname_size + 1));
      receive = tcp_recv(sockfd, nick_buf, nickname_size);

      if (receive == 0) {
        // printf("Nickname: %s\n", nick_buf);
        // If no error, get the message size
        receive = tcp_recv(sockfd, (char *)&msg_size, DEFAULT_BYTES_SIZE);

        if (receive == 0 && msg_size > 0) {
          // If no error, get the message
          msg_size = ntohl(msg_size);
          // printf("Message size=%d\n", msg_size);

          msg_buf = (char *)malloc(sizeof(char) * (msg_size + 1));
          memset(msg_buf, 0, (msg_size + 1));
          receive = tcp_recv(sockfd, msg_buf, msg_size);

          if (receive == 0) {
            // printf("Message: %s\n", msg_buf);

            // If no error, get the date size
            receive = tcp_recv(sockfd, (char *)&date_size, DEFAULT_BYTES_SIZE);

            if (receive == 0 && date_size > 0) {
              // If no error, get the date
              date_size = ntohl(date_size);
              date_buf = (char *)malloc(sizeof(char) * (date_size + 1));
              memset(date_buf, 0, (date_size + 1));
              receive = tcp_recv(sockfd, date_buf, date_size);

              if (receive == 0) {
                pthread_mutex_lock(&msg_mutex);
                printf("{%s} [%s] %s\n", date_buf, nick_buf, msg_buf);
                pthread_mutex_unlock(&msg_mutex);
              }
              free(date_buf);
            }
          }
          free(msg_buf);
        }
      }
      free(nick_buf);
    } else {
      printf("ERROR: -1\n");
      leave_flag = 1;
    }
  }

  pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  uint16_t portno;
  char hostname[50];
  struct sockaddr_in serv_addr;
  struct hostent *server;

  if (argc < 3) {
    fprintf(stderr,
            "Enter the server hostname, port and your nickname for"
            "%s\n",
            argv[0]);
    exit(EXIT_SUCCESS);
  }

  strncpy(hostname, argv[1], sizeof(hostname) - 1);
  portno = (uint16_t)atoi(argv[2]);
  nickname = argv[3];

  /* Create a socket point */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0) {
    perror("ERROR opening socket");
    exit(EXIT_FAILURE);
  }

  /* Get a server structure of type hostent for the given host name. */
  server = gethostbyname(hostname);

  if (server == NULL) {
    fprintf(stderr, "ERROR, no such host\n");
    exit(EXIT_FAILURE);
  }

  /* Setup the server sockaddr_in */
  bzero((char *)&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy(server->h_addr, (char *)&serv_addr.sin_addr.s_addr,
        (size_t)server->h_length);
  serv_addr.sin_port = htons(portno);

  // cout << serv_addr.sin_addr.s_addr << ": " << serv_addr.sin_port << endl;

  printf("Connecting...\n");

  /* Connect to the server */
  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("ERROR connecting");
    exit(EXIT_FAILURE);
  }

  printf("Successfully Connected\n");

  printf(">>>> WELCOME TO THE CHATROOM <<<<\n");

  pthread_t send_msg_thread = pthread_t();
  if (pthread_create(&send_msg_thread, NULL, send_msg_handler, NULL) != 0) {
    perror("ERROR : pthread");
    return EXIT_FAILURE;
  }

  pthread_t recv_msg_thread = pthread_t();
  if (pthread_create(&recv_msg_thread, NULL, recv_msg_handler, NULL) != 0) {
    perror("ERROR : pthread");
    return EXIT_FAILURE;
  }

  pthread_join(send_msg_thread, NULL);
  pthread_join(recv_msg_thread, NULL);

  pthread_exit(NULL);
  close(sockfd);

  return 0;
}
