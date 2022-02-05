# CS118 Project 1

This is the repo for winter22 cs118 project 1.

## Makefile

This provides a couple make targets for things.
By default (all target), it makes the `server` executables.

It provides a `clean` target, and `tarball` target to create the submission file as well.

You will need to modify the `Makefile` to add your userid for the `.tar.gz` turn-in at the top of the file.

## Academic Integrity Note

You are encouraged to host your code in private repositories on [GitHub](https://github.com/), [GitLab](https://gitlab.com), or other places.  At the same time, you are PROHIBITED to make your code for the class project public during the class or any time after the class.  If you do so, you will be violating academic honestly policy that you have signed, as well as the student code of conduct and be subject to serious sanctions.

## Provided Files

`server.c` is the entry points for the server part of the project.

## Testing

You can test your HTTP server directly using a browser, and/or a utility like `telnet` or `nc`. Your code will be graded using a script similar to the provided `test.sh`, and you should ensure that your code passes the provided tests. The final grading will use additional hidden tests to make sure your code follows the specification.

The output of `test.sh` would indicate which tests passed and failed along with the total number of passing tests. You can use this information to debug your code and make sure all the tests pass.

```
Checking HTTP status code ... pass
Checking content length ... pass
Checking if content is correct ... pass
Checking case insensitivity
Checking HTTP status code ... pass
Checking if content is correct ... pass
Checking GET without extension
Checking HTTP status code ... pass

Passed 6 tests, 0 failed
```

## TODO

#### High Level Design

For the server, I adopted much of the design given in Beej's Guide on web servers. The program starts off as a master process with a socket bound to local port 8080 waiting for requests. When there is a request, the master process spawns a child process that handles actually responding to the request, then continues waiting for more requests to spawn child processes for.

The child process when spawned reads the request, and then parses the request for the file name requested. Once it gets this name, it checks the local directory for the presence of such a file. If the file name requested has an extension, it looks for an exact match, if it has no extension, then the child just looks for a file with a matching name. If there is no matching file, the process immediately returns an error status 404. However, if it finds a match the process obtains the necessary information and sends it back with a status 200 in batches of 1024 bytes.

#### Problems

1. One cannot send a large file all at once. I fixed this by sending in batches of 1024 bytes as recommended by Beej's Guide.

2. String parsing the request proved to be very annoying. I didn't really have any fix for this except learning by trial and error with multiple char * approaches. My code is kind of messy because of this.

3. All libraries used are here:
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

All of these are standard libraries though, with no external dependencies.

#### Acknowledgements

Much code was directly paraphrased from Beej’s Guide to Network Programming Using Internet Sockets, a source provided by the Professor in the project spec. The header used in NOT_FOUND_HEADER was found online on StackOverflow.
