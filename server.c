#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <errno.h>
#include <stdio.h>

#define MY_PORT "8080"
#define BACKLOG_NUMBER 5 // the number of connections allowed on queue for connect
// ususally 5 or 10 is acceptable, system limits silently on 20

// a sample call if you’re a server who wants to listen on
// your host’s IP address, port 8080
// just sets up structures to use later, no listening or network setup done
void setup_hints_and_servinfo()
{

  // setup general structs and stuff
  int status;
  struct addrinfo hints;
  struct addrinfo *servinfo; // points to results
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;     // we dont care to specify ipv4 or ipv6
  hints.ai_socktype = SOCK_STREAM; //tcp stream sockets
  hints.ai_flags = AI_PASSIVE;     // fill in my local IP for me

  //  node is NULL because we are not connecting to an external host or IP
  if ((status = getaddrinfo(NULL, MY_PORT, &hints, &servinfo)) != 0)
  {
    perror("setsockopt");
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
  int yes = 1;
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

  // start listening socket
  // if we were connecting to a remote host, use connect()
  // since we are using local host, we use listen()
  if (listen(sockfd, BACKLOG_NUMBER))
  {
    perror("listen error");
    exit(1);
  }

  while (1)
  {
  }

  freeaddrinfo(servinfo); // free the linked-list
}

int main(int argc, char *argv[])
{
  return 0;
}
