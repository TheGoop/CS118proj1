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

void sigchild_handler(int s)
{
  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0)
  {
  };
  errno = saved_errno;
}

void respond_to_client(int socket_fd)
{
  char buffer[MESSAGE_LENGTH];

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
    return;
  }

  else
  {
    fwrite(buffer, MESSAGE_LENGTH, 1, stdout);
  }
}

void web_server()
{
  printf("Starting Web Server...\n");
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
  printf("server: waiting for connections...\n");
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

    else if (pid == 0) // if we are the child
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
  printf("Starting Program...\n");
  web_server();
  return 0;
}
