#include <iostream>
#include <stdio.h>
#include <stdlib.h>

#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

using namespace std;

#define DEFAULT_BUF_SIZE 512
#define DEFAULT_NICK_SIZE 32

int sockfd = 0;
char nickname[32];
pthread_mutex_t msg_mutex = PTHREAD_MUTEX_INITIALIZER;

void str_trim_lf(char *arr, int length) {
  int i;
  for (i = 0; i < length; i++) {  // trim \n
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
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
    if (scanf(" %4[^\n]", ch) < 0) {
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

      pthread_mutex_unlock(&msg_mutex);

      str_trim_lf(msg, sizeof(msg));

      if (strlen(msg) == 0)
        printf("Empty message! There's nothing to send.\n");
      else {
        send(sockfd, msg, strlen(msg), 0);
      }
    }
    memset(msg, 0, DEFAULT_BUF_SIZE);
  }
// return NULL;
  pthread_exit(NULL);
}

// Just receive a message from a server and output it
void *recv_msg_handler(void *args) {
  if (args != NULL) exit(EXIT_FAILURE);
  char msg[DEFAULT_BUF_SIZE] = {};
  while (true) {
    int receive = recv(sockfd, msg, DEFAULT_BUF_SIZE, 0);
    if (receive > 0) {
      printf("%s", msg);
    } else {
      break;
    }
    bzero(msg, DEFAULT_BUF_SIZE);
  }
  // return NULL;
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

  strncpy(hostname, argv[1], sizeof(hostname));
  portno = (uint16_t)atoi(argv[2]);
  strncpy(nickname, argv[3], sizeof(nickname));

  if (strlen(nickname) < 2 || strlen(nickname) >= DEFAULT_NICK_SIZE - 1) {
    perror(
      "The nickname has inappropriate size. "
      "It should be 2 to 32 characters.\nQuitting...\n");
    exit(EXIT_FAILURE);
  }

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

  cout << serv_addr.sin_addr.s_addr << ": " << serv_addr.sin_port << endl;


  printf("Connecting...\n");

  /* Connect to the server */
  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("ERROR connecting");
    exit(EXIT_FAILURE);
  }

  printf("Successfully Connected\n");

  // Send nickname
  send(sockfd, nickname, DEFAULT_NICK_SIZE, 0);

  printf(">>>> WELCOME TO THE CHATROOM <<<<\n");

  pthread_t send_msg_thread = pthread_t();
  if (pthread_create(&send_msg_thread, NULL, send_msg_handler, NULL) != 0) {
    perror("ERROR : pthread");
    return EXIT_FAILURE;
  }

  pthread_t recv_msg_thread = pthread_t();
  if (pthread_create(&send_msg_thread, NULL, recv_msg_handler, NULL) != 0) {
    perror("ERROR : pthread");
    return EXIT_FAILURE;
  }

  pthread_join(send_msg_thread, NULL);
  pthread_join(recv_msg_thread, NULL);

  pthread_exit(NULL);
  close(sockfd);

  return 0;
}
