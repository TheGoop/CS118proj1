#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <dirent.h>

#include <sys/stat.h>

#define MESSAGE_LENGTH 1024
#define MY_PORT "8080"
#define BACKLOG_NUMBER 5 // the number of connections allowed on queue for connect
// ususally 5 or 10 is acceptable, system limits silently on 20

#define SUCCESS_STATUS "HTTP/1.1 200 OK\n"
#define NOT_FOUND_HEADER "HTTP/1.1 404 Not Found\n\n<html><body><h1>404 Not Found</h1></body></html>"
#define CONTENT_TYPE "Content-Type: "
#define CONTENT_LENGTH "Content-Length: "

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
  if (end == NULL)
  {
    return -1;
  }
  *filename_length = end - start;
  return buffer - pch + strlen(prefix);
}

void get_file_type(char *filename, char **file_type)
{
  char *file_ext = strchr(filename, '.');
  if (file_ext == NULL)
  {
    *file_type = "application/octet-stream";
  }
  else
  {
    // plain text files
    if (strcmp(file_ext, ".txt") == 0)
    {
      *file_type = "text/plain";
    }
    else if (strcmp(file_ext, ".html") == 0 || strcmp(file_ext, ".htm") == 0)
    {
      *file_type = "text/html";
    }
    // static image files
    else if (strcmp(file_ext, ".png") == 0)
    {
      *file_type = "image/png";
    }
    else if (strcmp(file_ext, ".jpeg") == 0 || strcmp(file_ext, ".jpg") == 0)
    {
      *file_type = "image/jpeg";
    }
    else if (strcmp(file_ext, ".gif") == 0)
    {
      *file_type = "image/gif";
    }
    else
    {
      *file_type = "n/a";
    }
  }
}

int get_file_length(char *filename)
{
  struct stat st;
  stat(filename, &st);
  return st.st_size;
}

void *send_response(char *buffer, int socket_fd)
{
  // printf("construct_message()\n");
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
    // TODO: Implement case for no filename given
    // (just return error 404 message)
  }

  // printf("Filename starts at %d and is %d long.\n", filename_pos, filename_length);
  char filename[filename_length + 1];
  strncpy(filename, &buffer[filename_pos], filename_length);
  filename[filename_length] = '\0';
  // printf("File name is %s\n", filename);

  // now convert filename to lower case
  char *filename_lower = calloc(strlen(filename) + 1, sizeof(char));
  for (size_t i = 0; i < strlen(filename); ++i)
  {
    filename_lower[i] = tolower((unsigned char)filename[i]);
  }
  // printf("%s\n", filename_lower);

  struct dirent *de;
  DIR *dr = opendir(".");

  if (dr == NULL)
  {
    perror("Could not open current directory\n");
    exit(1);
  }

  int match = 0;
  char *matched_file_name = NULL;
  while ((de = readdir(dr)) != NULL && match == 0)
  {
    char *local_file = de->d_name;
    char *local_file_lower = calloc(strlen(local_file) + 1, sizeof(char));
    for (size_t i = 0; i < strlen(local_file); ++i)
    {
      local_file_lower[i] = tolower((unsigned char)local_file[i]);
    }
    // printf("%s %s\n", local_file, local_file_lower);
    //
    if (local_file[0] == '.')
    {
      // printf("Skipped\n");
    }
    else
    {
      char *pch = strstr(local_file_lower, ".");
      if (pch != NULL)
      {
        int pos = pch - local_file_lower;
        // printf("%s %d\n", local_file, pos);

        char local_file_lower_no_ext[pos + 1];

        strncpy(local_file_lower_no_ext, &local_file_lower[0], pos + 1);
        local_file_lower_no_ext[pos] = '\0';
        // printf("The original string is: %s\n", local_file);
        // printf("Without Extension is: %s\n", local_file_lower_no_ext);

        if (strcmp(filename_lower, local_file_lower) == 0 || strcmp(filename_lower, local_file_lower_no_ext) == 0)
        {
          match = 1;
          matched_file_name = local_file;
        }
      }
    }
    free(local_file_lower);
  }
  free(filename_lower);
  char *message;
  if (match == 0)
  {
    printf("Matches no file\n");
    // no files matched
    // send error 404
    message = malloc(strlen(NOT_FOUND_HEADER) + 1);
    strcpy(message, NOT_FOUND_HEADER);

    int bytes_written = send(socket_fd, message, MESSAGE_LENGTH, 0);
  }
  else
  {
    printf("MATCHED: %s\n", matched_file_name);

    char *file_type;
    get_file_type(matched_file_name, &file_type);

    int file_length_dec = get_file_length(matched_file_name);
    // printf("Length: %d\n", file_length_dec);

    char file_length[50];
    sprintf(file_length, "%d\n\n", file_length_dec);
    // printf("Length: %s\n", file_length);

    message = malloc(MESSAGE_LENGTH + 1);
    strcpy(message, SUCCESS_STATUS);
    strcat(message, CONTENT_TYPE);
    strcat(message, file_type);
    strcat(message, "\n");
    strcat(message, CONTENT_LENGTH);
    strcat(message, file_length);
    strcat(message, "<DATA>\n");

    printf("Sending Message: \n%s\n", message);
    int bytes_written = send(socket_fd, message, MESSAGE_LENGTH, 0);
    // strcat(message, filedata)
  }
  free(message);
}

void respond_to_client(int socket_fd)
{

  // printf("Respond_to_client()\n");
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
    // printf("Close connection...\n");
    return;
  }

  // printf("--------\nMessage Recieved: \n%s\n--------\n", buffer);

  // construct message here
  send_response(buffer, socket_fd);
  // printf("Message: %s\n", message);

  // send message
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
