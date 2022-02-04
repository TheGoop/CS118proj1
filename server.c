#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MESSAGE_LENGTH 1024
#define MY_PORT "8080"
#define BACKLOG_NUMBER 5 // the number of connections allowed on queue for connect
// ususally 5 or 10 is acceptable, system limits silently on 20

#define SUCCESS_STATUS "HTTP/1.1 200 OK\n"
#define NOT_FOUND_HEADER "HTTP/1.1 404 Not Found\n\n<html><body><h1>404 Not Found</h1></body></html>"

void sigchild_handler(int s)
{
  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0)
  {
  };
  errno = saved_errno;
}

int find_filename(char *buffer, int *filename_length)
{
  // find beginning of filename
  char *prefix = "GET /";
  char *pch = strstr(buffer, prefix);
  if (pch == NULL)
  {
    return -1;
  }
  // int pos = pch - buffer + strlen(prefix) - 1;
  // printf("Filename starts at %d\n", pos);
  // printf("%s\n", buffer);
  char *start = pch + strlen(prefix);
  char *end = strchr(start, ' ');
  *filename_length = end - start;
  return buffer - pch + strlen(prefix);
}

char *construct_message(char *buffer)
{
  printf("construct_message()\n");
  /*
    GET /test1.jpeg HTTP/1.1
    Host: localhost:8080
    User-Agent: curl/7.65.2
    Accept: Asterix/Asterix
  */

  printf("--------\nMessage Recieved: \n%s\n--------\n", buffer);

  int filename_length;
  int filename_pos = find_filename(buffer, &filename_length);
  if (filename_pos == -1)
  {
    fprintf(stderr, "Bad filename\n");
    exit(1);
    // TODO: Implement case for bad filename given
    // (just return error 404 message)
  }

  printf("Filename starts at %d and is %d long.\n", filename_pos, filename_length);

  char filename[filename_length + 1];
  strncpy(filename, &buffer[filename_pos], filename_length);
  filename[filename_length] = '\0';
  printf("Filename is %s\n", filename);

  return "";
}

void respond_to_client(int socket_fd)
{

  // printf("Respond_to_client()\n");
  char buffer[MESSAGE_LENGTH];
  char message_buffer[MESSAGE_LENGTH] = NOT_FOUND_HEADER;

  memset(buffer, 0, sizeof(buffer));

  int bytes_read = recv(socket_fd, buffer, MESSAGE_LENGTH, 0);
  if (bytes_read == -1)
  {
    perror("recv error");
    exit(1);
  }

  // means that the connection has closed
  else if (bytes_read == 0)
  {
    if (close(socket_fd) == -1)
    {
      perror("Close error");
      exit(1);
    }
    // printf("Close connection...\n");
    return;
  }

  // printf("--------\nMessage Recieved: \n%s\n--------\n", buffer);

  // construct message here
  char *message = construct_message(buffer);

  // send message
  int bytes_written = send(socket_fd, message_buffer, MESSAGE_LENGTH, 0);
  // printf("--------\nMessage Sent: \n%s\n--------\n", message_buffer);

  memset(buffer, 0, sizeof(buffer));
}

void web_server()
{
  // setup general structs and stuff
  int status;
  struct addrinfo hints;
  struct addrinfo *servinfo;      // points to results
  struct sockaddr_in server_addr; // connector's address information
  struct sockaddr_in host_addr;

  socklen_t addr_len = sizeof(struct sockaddr_storage);
  int yes = 1;

  struct sigaction sa; // used for sigchild handling

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;     // we dont care to specify ipv4 or ipv6
  hints.ai_socktype = SOCK_STREAM; //tcp stream sockets
  hints.ai_flags = AI_PASSIVE;     // fill in my local IP for me

  //  node is NULL because we are not connecting to an external host or IP
  if ((status = getaddrinfo(NULL, MY_PORT, &hints, &servinfo)) != 0)
  {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    exit(1);
  }
  // hints parameter points to a struct addrinfo that you’ve already
  // filled out with relevant information w.r.t the conneciton we are making

  // servinfo now points to a linked list of 1 or more struct addrinfos

  // setup socket
  int sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  if (sockfd == -1)
  {
    perror("socket setup");
    exit(1);
  }

  //prevent address in use error by allowing reusal of socket
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1)
  {
    perror("setsockopt");
    exit(1);
  }

  // bind socket - associate it with an ip and port (this case local machine)
  if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
  {
    perror("bind error");
    exit(1);
    // “Address already in use.” means that a little socket is hogging port
  }

  freeaddrinfo(servinfo); // free the linked-list since we don't need it now

  // start listening socket
  // if we were connecting to a remote host, use connect()
  // since we are using local host, we use listen()
  if (listen(sockfd, BACKLOG_NUMBER) == -1)
  {
    perror("listen error");
    exit(1);
  }

  sa.sa_handler = sigchild_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1)
  {
    perror("sigaction");
    exit(1);
  }

  int new_fd;
  int pid;
  int c = 0;
  // printf("server: waiting for connections...\n");
  while (1)
  {
    new_fd = accept(sockfd, (struct sockaddr *)&host_addr, &addr_len);
    if (new_fd == -1)
    {
      perror("Accept");
      exit(1);
    }

    pid = fork();
    // we must fork and create a child process to handle connection
    // parent process will continue waiting for new connections and creating new children
    if (pid == -1)
    {
      perror("Fork");
      exit(1);
    }

    if (pid == 0) // if we are the child
    {
      close(sockfd); // child doesn't need the listener
      respond_to_client(new_fd);
      exit(0);
    }
    else
    {
      close(new_fd);
    }
  }
}

int main(int argc, char *argv[])
{
  web_server();
  return 0;
}
