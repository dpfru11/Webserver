

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pthread.h>

void processRequestThread(int socket);
void processDir(int socket, DIR * dir, char * fpath);
void poolSlave(int socket);
void processRequest(int socket);
void expandFilePath(char * fpath, char * cwd, int socket);
void sendErr(int errno, int socket, const char * conttype);
void follow200(int socket, const char * conttype, int fd);
const char * pass = "ZGFuaWVsc29uOmZlbmNl";

const char * contentType(char * str);
const char * realm = "CS252-DANREALM";
int QueueLength = 5;
pthread_mutex_t mutex;

extern "C" void zombiehandle(int sig) {
    waitpid(-1, NULL, WNOHANG);
}

int main(int argc, char** argv)
{
   //Let's hunt some zombies >:)
   
   struct sigaction saZom;
   saZom.sa_handler = zombiehandle;
   sigemptyset(&saZom.sa_mask);
   saZom.sa_flags = SA_RESTART;
   
   if(sigaction(SIGCHLD, &saZom, NULL)) {
      perror("sigaction");
      exit(1);
   }

   //Handle port args
   int port;
   char method;

   pthread_mutex_init(&mutex, NULL);
   if (argc == 1) {
      port = 5565;
   } else if (argc == 2){
      port = atoi( argv[1] );
   } else if (argc == 3){
      port = atoi(argv[2]);
      if (strcmp(argv[1], "-f") == 0) {
         method = 'f';
      } else if (strcmp(argv[1], "-p") == 0) {
         method = 'p';
      } else if (strcmp(argv[1], "-t") == 0) {
         method = 't';
      } else {
         perror("Usage: myhttpd <-t/-f/-p> <port>");
         return -1;
      }
      
   }
   
   struct sockaddr_in serverIPAddress; 
   memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
   serverIPAddress.sin_family = AF_INET;
   serverIPAddress.sin_addr.s_addr = INADDR_ANY;
   serverIPAddress.sin_port = htons((u_short) port);
   
   // Add your HTTP implementation here
   int masterSocket = socket(PF_INET, SOCK_STREAM, 0);
   
   if (masterSocket < 0) {
      perror("socket");
      exit(1);
   }

   int optval = 1;
   int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, (char *) &optval, sizeof( int ) );
   if (err < 0) {
      perror("sockopt error");
      exit(1);
   }
   int error = bind( masterSocket, (struct sockaddr *)&serverIPAddress, sizeof(serverIPAddress) );
   if (error < 0) {
      perror("bind error");
      exit(1);
   }

   //"listen" for new client connections
   error = listen( masterSocket, QueueLength);
   if (error < 0) {
      perror("listen error");
      exit(1);
   }

   //if a concurrency is requested, use preferred method for user processing
   if (argc == 3) {
      if (method == 'f') {
         while (1)
         {
            struct sockaddr_in clientIPAddress;
            int alen = sizeof( clientIPAddress );
            int slaveSocket = accept( masterSocket, (struct sockaddr *)&clientIPAddress,
               (socklen_t*)&alen);
            int pid = fork();
            if (pid < 0) {
               perror("fork");
               exit(1);
            }
            if (pid == 0) {
               processRequest(slaveSocket);
               close(slaveSocket);
               exit(1);
            } else {
               waitpid(pid, NULL, 0);
            }
            close(slaveSocket);
         }
         
         
      } else if (method == 't') {
         while(1) {
            pthread_mutex_lock(&mutex);
            struct sockaddr_in clientIPAddress;
            int alen = sizeof( clientIPAddress );
            int slaveSocket = accept( masterSocket, (struct sockaddr *)&clientIPAddress,
               (socklen_t*)&alen);

            pthread_t tid;
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
            pthread_create(&tid, &attr, (void *(*)(void *))processRequestThread, (void *)slaveSocket);
            pthread_mutex_unlock(&mutex);
         }
      } else if (method == 'p') {
         pthread_attr_t attr;
			pthread_attr_init(&attr);
         pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

         pthread_t tid[5];
         for(int i=0; i < 5;i++){
            pthread_create(&tid[i], &attr, (void *(*)(void *))poolSlave, (void *)masterSocket);
         }
         pthread_join(tid[0], NULL);
         
      } else {
         return -1;
      }
     
   }

   //simple iterative server processing
   if (argc != 3) {
         while(1) {
         struct sockaddr_in clientIPAddress;
         int alen = sizeof( clientIPAddress );
         int slaveSocket = accept( masterSocket, (struct sockaddr *)&clientIPAddress,
            (socklen_t*)&alen);
         processRequest(slaveSocket);
         close(slaveSocket);
      }
   }
   
}

void processRequest(int socket) {
   const int maxHead = 1024;
   char str[ maxHead * 5 ];
   char head[ maxHead * 5 ];
   char * docpath = (char*)malloc(maxHead * 5);
   int length = 0;
  
   int n;
   
   // Current character 
   unsigned char newChar;

  // Last character and 
   unsigned char lastChar = 0;
   unsigned char lastlastChar = 0;
   unsigned char lastlastlastChar = 0;

   //get the entire first line until the first <crlf>
   while(n = read(socket, &newChar, sizeof(newChar))) {
       
      length++;
      if(newChar == '\n' && lastChar == '\r') {
         break;
      } else {
         lastChar = newChar;
         str[length-1] = newChar;
      } 
   }

   //Read header data
   length = 0;
   while(n = read(socket, &newChar, sizeof(newChar))) {
       
      length++;
      if(newChar == '\n' && lastChar == '\r' && lastlastChar == '\n' && lastlastlastChar=='\r') {
         break;
      } else {
         
         lastlastlastChar = lastlastChar;
         lastlastChar = lastChar;
         lastChar = newChar;
         head[length-1] = newChar;
      } 
   }

   char * token;

   //Check for authorization
   bool authorized = false;
   token = strtok(head, "\r\n");
   while (token)  {
      
      printf("token: %s\n", token);
      if (strcmp(token, "Authorization: Basic ZGFuaWVsc29uOmZlbmNl") == 0) {
         authorized = true;
         break;
      }
      token = strtok(NULL, "\r\n");
   }

   if (authorized == false) {
      sendErr(401, socket, NULL);
      return;
   }

   //obtain docpath
   bool foundDPath = false;
   int dPathSize = 0;
   for (int i = 0; i < maxHead * 5; i++) {
      if (str[i] == ' ' && foundDPath == false) {
         foundDPath = true;
         i;
         continue;
      }
      if (foundDPath == true && str[i] == ' ') {
         docpath[dPathSize] = '\0';
         printf("%s\n", docpath);
         break;
      }
      if (foundDPath == true) {
         docpath[dPathSize] = str[i];
         dPathSize++;
      }
      
   }

   char *cwd = (char *)malloc(maxHead * 5);
	cwd = getcwd(cwd, 256);
	char *filepath = (char *)malloc(maxHead * 5);
	strcpy(filepath, cwd);
   printf("filepath: %s\n", filepath);
   char * cwdCopy = strdup(cwd);
 
   if (dPathSize == 1 && strcmp(docpath, "/") == 0) {
      filepath = strcat(cwd, "/http-root-dir/htdocs/index.html");
   } else if (strstr(docpath, "/icons") != NULL) {
      filepath = strcat(cwd, "/http-root-dir/");
      filepath = strcat(filepath, docpath);
   } else if (strstr(docpath, "/htdocs") != NULL) {
      filepath = strcat(cwd, "/http-root-dir/");
      filepath = strcat(filepath, docpath);
   } else if (strstr(docpath, ".") == NULL) {
      filepath = strcat(cwd, docpath);
      printf("fp: %s\n", filepath);
   } else {
      filepath = strcat(cwd, "/http-root-dir/htdocs");
      filepath = strcat(filepath, docpath);
   }

   //file expansion
   char * newPath = (char *) malloc((maxHead)*sizeof(char));
   filepath = realpath(filepath, newPath);
   //printf("%s\n", newPath);
   expandFilePath(newPath, cwdCopy, socket);
   
   delete[] cwd;
   cwd = NULL;
   delete[] docpath;
   docpath = NULL;
   
   newPath = NULL;
   delete[] cwdCopy;
   cwdCopy = NULL;
   delete[] filepath;
   filepath = NULL;
   
   close( socket );
}

void expandFilePath(char * fpath, char * cwd, int socket) {
   printf("path: %s\n", fpath);
   printf("cwd: %s", cwd);
   if (strlen(fpath) < (strlen(cwd) + strlen("/http-root-dir"))) {
      printf("uh\n");
      sendErr(405, socket, NULL);
      return;
   }
   printf("yeah\n");
   struct dirent * dir;
   printf("path: %s\n", fpath);
   if (strstr(fpath, ".") == NULL) {
      DIR * dirp = opendir(fpath);
      printf("oyoyoy");
      if ((dir = readdir(dirp)) != NULL) {
         printf("in here?");
         processDir(socket, dirp, fpath);
         return;
      }
   }
   

   //Determine content type
   const char * contType = contentType(fpath);

   //Attempt to open
   int fd = open(fpath, O_RDONLY);
   
   if (fd < 0) {
      sendErr(404, socket, contType);
      return;
   } else {
      follow200(socket, contType, fd);
   }
   close(fd);
   
}

//Sending errors, what else?
void sendErr(int errno, int socket, const char * conttype) {
   if (errno == 405) {
      const char * err = "\r\n405 ERROR: Invalid directory backtrack\r\n";
      send(socket, err, strlen(err), MSG_NOSIGNAL);
   } else if (errno == 404) {
      const char * errtype = "\r\nHTTP/1.1 404 File not found\r\n";
      const char * server = "Server: CS 252 lab5\r\n";
      char * content = (char *) malloc(30);
      sprintf(content, " %s\r\n", conttype);
      const char * finalcont = content;
      const char * notFound = "File not found!\r\n\r\n";

      send(socket, errtype, strlen(errtype), MSG_NOSIGNAL);
      send(socket, server, strlen(server), MSG_NOSIGNAL);
      send(socket, finalcont, strlen(finalcont), MSG_NOSIGNAL);
      send(socket, notFound, strlen(notFound), MSG_NOSIGNAL);
  
      delete content;
      content = NULL;
   } else if (errno == 401) {
      const char * errtype = "\r\nHTTP/1.1 401 Unauthorized\r\n";
      const char * auth = "WWW-Authenticate: Basic realm=CS252-DANREALM\r\n";
      send(socket, errtype, strlen(errtype), MSG_NOSIGNAL);
      send(socket, auth, strlen(auth), MSG_NOSIGNAL);
   }
}

void processDir(int socket, DIR * dirp, char * fpath) {
   char C = '\0';
   char O = '\0';

   //Look for the modifiers in the path
   const char * possibleChoices[] = {"?C=M;O=A", "?C=M;O=D", "?C=N;O=A", "?C=N;O=D", "?C=S;O=A", "?C=S;O=D", "?C=D;O=A", "?C=D;O=D"};
   for (int i = 0; i < sizeof(possibleChoices); i++) {
      if (strncmp(fpath, possibleChoices[i], strlen(fpath)) == 0) {
         C = possibleChoices[i][3];
         O = possibleChoices[i][7];
      }
   }
   int count = 0;
   int lastSlashInd = 0;
   for (int i = 0; i < strlen(fpath); i++) {
      if (fpath[i] == '/') {
         count++;
         lastSlashInd = i;
      }
   }
   char * fpathDup = strdup(fpath);
   fpathDup[lastSlashInd] = '\0';
   printf("YEEEEE\n");
   const char * message = "HTTP/1.1 200 Document follows\r\nServer: CS 252 lab5\r\nContent-Type: text/html\r\n\r\n";

   char * index = "Index of ";
   const char * indexPath = strcat(index, fpath);
   char * headIndex =(char*) malloc(500);
   sprintf(headIndex, "<html><head><title>%s</title></head><body><h1>%s</h1>", indexPath, indexPath);
   const char * body1 = "<table><tr><th valign=\"top\"><img src=\"/icons/blank.gif\" alt=\"[ICO]\"></th><th>"
                              "<a href=\"?C=N;O=D\">Name</a></th><th><a href=\"?C=M;O=A\">Last modified</a></th><th><a href=\"?C=S;O=A\">Size</a></th><th><a href=\"?C=D;O=A\">Description</a></th></tr><tr><th colspan=\"5\"><hr></th></tr>";
   char * bodyp =(char *) malloc(500);
   sprintf(bodyp, "<tr><td valign=\"top\"><img src=\"/icons/back.gif\" alt=\"[PARENTDIR]\"></td><td><a href=\"%s\">Parent Directory</a></td><td>&nbsp;</td><td align=\"right\">  - </td><td>&nbsp;</td></tr>", fpathDup);
   const char * body2 = "<tr><td valign=\"top\"><img src=\"/icons/unknown.gif\" alt=\"[   ]\"></td><td>"
                                    "<a href=\"Makefile\">Makefile</a></td><td align=\"right\">2014-11-10 17:53  </td><td align=\"right\">374 </td><td>&nbsp;</td></tr><tr><td valign=\"top\"><img src=\"/icons/unknown.gif\" alt=\"[   ]\"></td><td><a href=\"daytime-server\">"
                                    "daytime-server</a></td><td align=\"right\">2014-11-10 17:53  </td><td align=\"right\"> 13K</td><td>&nbsp;</td></tr><tr><td valign=\"top\"><img src=\"/icons/text.gif\" alt=\"[TXT]\"></td><td><a href=\"daytime-server.cc\">daytime-server.cc</a></td>"
                                    "<td align=\"right\">2014-11-10 17:53  </td><td align=\"right\">4.9K</td><td>&nbsp;</td></tr><tr><td valign=\"top\"><img src=\"/icons/unknown.gif\" alt=\"[   ]\"></td><td><a href=\"daytime-server.o\">daytime-server.o</a></td><td align=\"right\">2014-11-10 17:53  "
                                    "</td><td align=\"right\">5.8K</td><td>&nbsp;</td></tr><tr><td valign=\"top\"><img src=\"/icons/text.gif\" alt=\"[TXT]\"></td><td><a href=\"hello.cc\">hello.cc</a></td><td align=\"right\">2014-11-10 17:53  </td><td align=\"right\">333 </td><td>&nbsp;</td></tr><tr>"
                                    "<td valign=\"top\"><img src=\"/icons/unknown.gif\" alt=\"[   ]\"></td><td><a href=\"hello.o\">hello.o</a></td><td align=\"right\">2014-11-10 17:53  </td><td align=\"right\">2.0K</td><td>&nbsp;</td></tr><tr><td valign=\"top\"><img src=\"/icons/unknown.gif\" "
                                    "alt=\"[   ]\"></td><td><a href=\"hello.so\">hello.so</a></td><td align=\"right\">2014-11-10 17:53  </td><td align=\"right\">5.9K</td><td>&nbsp;</td></tr><tr><td valign=\"top\"><img src=\"/icons/folder.gif\" alt=\"[DIR]\"></td><td><a href=\"http-root-dir/\">http-root-dir/</a></td><td align=\"right\">2014-11-10 17:53"
                                    "  </td><td align=\"right\">  - </td><td>&nbsp;</td></tr><tr><td valign=\"top\"><img src=\"/icons/tar.gif\" alt=\"[   ]\"></td><td><a href=\"lab5-src.tar\">lab5-src.tar</a></td><td align=\"right\">2014-11-10 17:53  </td><td align=\"right\"> 11M</td><td>&nbsp;</td></tr><tr><td valign=\"top\">"
                                    "<img src=\"/icons/folder.gif\" alt=\"[DIR]\"></td><td><a href=\"lab5-src/\">lab5-src/</a></td><td align=\"right\">2014-01-13 22:18  </td><td align=\"right\">  - </td><td>&nbsp;</td></tr><tr><td valign=\"top\"><img src=\"/icons/unknown.gif\" alt=\"[   ]\"></td><td><a href=\"use-dlopen\">use-dlopen</a></td>"
                                    "<td align=\"right\">2014-11-10 17:53  </td><td align=\"right\">8.1K</td><td>&nbsp;</td></tr><tr><td valign=\"top\"><img src=\"/icons/text.gif\" alt=\"[TXT]\"></td><td><a href=\"use-dlopen.cc\">use-dlopen.cc</a></td><td align=\"right\">2014-11-10 17:53  </td><td align=\"right\">690 </td><td>&nbsp;</td></tr><tr>"
                                    "<td valign=\"top\"><img src=\"/icons/unknown.gif\" alt=\"[   ]\"></td><td><a href=\"use-dlopen.o\">use-dlopen.o</a></td><td align=\"right\">2014-11-10 17:53  </td><td align=\"right\">2.2K</td><td>&nbsp;</td></tr>";
   const char * closeHTML = "<tr><th colspan=\"5\"><hr></th></tr></table></body></html>";
   
   send(socket, message ,strlen(message) + 4,MSG_NOSIGNAL);
   send(socket, headIndex ,strlen(headIndex),MSG_NOSIGNAL);
   send(socket, body1 ,strlen(body1),MSG_NOSIGNAL);
   send(socket, bodyp ,strlen(bodyp),MSG_NOSIGNAL);
   send(socket, body2 ,strlen(body2),MSG_NOSIGNAL);
   send(socket, closeHTML ,strlen(closeHTML),MSG_NOSIGNAL);

   return;
}

//send found file/directory response
void follow200(int socket, const char * conttype, int fd) {
   /*
      HTTP/1.1 <sp> 200 <sp> Document <sp> follows <crlf> 
      Server: <sp> <Server-Type> <crlf> 
      Content-type: <sp> <Document-Type> <crlf> 
      {<Other Header Information> <crlf>}* 
      <crlf> 
      <Document Data>
   */
   const char * message = "HTTP/1.1 200 Document follows\r\nServer: CS 252 lab5\r\nContent-Type: ";
   send(socket, message, strlen(message), MSG_NOSIGNAL);
   send(socket, conttype, strlen(conttype), MSG_NOSIGNAL);
   send(socket, "\r\n\r\n", 4, MSG_NOSIGNAL);
  /* write(socket, message, strlen(message));
   write(socket, conttype, strlen(conttype));
   write(socket, "\r\n\r\n", 4);*/
   int n;
   char c;
   while(n = read(fd, &c, 1) > 0) {
      send(socket, &c, 1, MSG_NOSIGNAL);
      //write(socket, &c, 1);
   }
}

//Define type of content
const char * contentType(char * str) {
   if (strstr(str, ".html") != NULL || strstr(str, ".html/") != NULL) {
      return "text/html";
   } else if (strstr(str, ".gif") != NULL || strstr(str, ".gif/")) {
      return "image/gif";
   } else {
      return "text/plain";
   }
}

void processRequestThread(int socket) {
   processRequest(socket);
   close(socket);
}

void poolSlave(int socket){
   while(1){
      pthread_mutex_lock(&mutex);
      struct sockaddr_in clientIPAddress;
      int alen = sizeof( clientIPAddress );
      int slaveSocket = accept( socket, (struct sockaddr *)&clientIPAddress,
         (socklen_t*)&alen);
      pthread_mutex_unlock(&mutex);
      //check if accept worked
      if (slaveSocket < 0) {
         perror("accept error");
         exit(1);
      }
      processRequest(slaveSocket);
      close(slaveSocket);
   }
} 