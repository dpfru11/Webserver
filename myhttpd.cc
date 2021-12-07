

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
#include <ctime>
#include <pthread.h>

void processRequestThread(int socket);
void processDir(int socket, DIR * dir, char * fpath, char * docpath);
void poolSlave(int socket);
void processLoad(int socket, char * realpath);
void processRequest(int socket);
void processCGI(int socket, char * realpath, char * docpath, char * args);
void displayLog(int socket, char * realpath);
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
	//strcpy(filepath, cwd);
   printf("cwd early: %s\n", cwd);
   char * cwdCopy = strdup(cwd);
   int isCGI = 0;
 
   if (dPathSize == 1 && strcmp(docpath, "/") == 0) {
      filepath = strcat(cwd, "/http-root-dir/htdocs/index.html");
   } else if (strstr(docpath, "/icons") != NULL) {
      filepath = strcat(cwd, "/http-root-dir/");
      filepath = strcat(filepath, docpath);
   } else if (strstr(docpath, "/htdocs") != NULL) {
      filepath = strcat(cwd, "/http-root-dir");
      filepath = strcat(filepath, docpath);
   } else if (strstr(docpath, "/cgi-bin") != NULL) {
      printf("in hereyess");
      filepath = strcat(cwd, "/http-root-dir");
      filepath = strcat(filepath, docpath);
      isCGI = 1;
   } else {
      filepath = strcat(cwd, "/http-root-dir/htdocs");
      filepath = strcat(filepath, docpath);
   }
   
   if (isCGI == 1) {
      printf("hereyes\n");
      char * args = (char*)malloc(500);
      int conArgs = 0;
      int index = 0;
      int startArgs = 0;
      for (int i = 0; i < strlen(docpath); i++) {
         if (docpath[i] == '?') {
            args = (char *) malloc(strlen(docpath));
            conArgs = 1;
            continue;
         }
         if (conArgs == 1) {
            args[index] = docpath[i];
            index++;
            if (index == strlen(docpath)) {
               args[index] = '\0';
               break;
            }
         }
         if (conArgs == 0) {
            startArgs++;
         }
      }
      docpath[startArgs] = '\0';
      
      processCGI(socket, filepath, docpath, args);
      return;

   }

   //file expansion
   char * newPath = (char *) malloc((maxHead)*sizeof(char));
   realpath(filepath, newPath);

   if (strstr(newPath, "/stats") != NULL) {
      displayLog(socket, newPath);
   }
   printf("newPath: %s", newPath);
   printf("oh? youre approaching me?\n");
   DIR * dirp = opendir(newPath);
   if (dirp == NULL) {
      printf("empty\n");
   }
   if (dirp != NULL) {
      printf("in here?");
      processDir(socket, dirp, newPath, docpath);
      return;
   }

   
   expandFilePath(newPath, cwdCopy, socket);
   
   delete[] cwd;
   cwd = NULL;
   printf("here?\n");
   delete[] docpath;
   docpath = NULL;
   
   newPath = NULL;
   delete[] cwdCopy;
   cwdCopy = NULL;
   delete[] newPath;
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

void processDir(int socket, DIR * dirp, char * fpath, char * docpath) {
   char C = '\0';
   char O = '\0';
   printf("MODIFIERS\n");
   //Look for the modifiers in the path
   /*const char * possibleChoices[] = {"?C=M;O=A", "?C=M;O=D", "?C=N;O=A", "?C=N;O=D", "?C=S;O=A", "?C=S;O=D", "?C=D;O=A", "?C=D;O=D"};
   for (int i = 0; i < sizeof(possibleChoices); i++) {
      printf("it");
      if (strncmp(fpath, possibleChoices[i], strlen(fpath)) == 0) {
         C = possibleChoices[i][3];
         O = possibleChoices[i][7];
      }
   }*/
   
   //Get Parent Dir
   int count = 0;
   int lastSlashInd = 0;
   for (int i = 0; i < strlen(docpath); i++) {
      if (docpath[i] == '/' && i != strlen(docpath) - 1) {
         count++;
         lastSlashInd = i;
      }
   }
   char * fpathDup = strdup(docpath); //Parent directory
   fpathDup[lastSlashInd + 1] = '\0';
   const char * message = "HTTP/1.1 200 Document follows\r\nServer: CS 252 lab5\r\nContent-Type: text/html\r\n\r\n";
   send(socket, message ,strlen(message),MSG_NOSIGNAL);

   char * index = "Index of ";
   
   char * indexPath = (char *) malloc(200);
   sprintf(indexPath, "%s%s", index, fpath);

   char * headIndex =(char*) malloc(500);
   printf("ope");
   sprintf(headIndex, "<html><head><title>%s</title></head><body><h1>%s</h1>", indexPath, indexPath);
   send(socket, headIndex, strlen(headIndex), MSG_NOSIGNAL);

   const char * body1 = "<table><tr><th valign=\"top\"><img src=\"/icons/menu.gif\" alt=\"[ICO]\"></th><th>"
                              "<a href=\"?C=N;O=D\">Name</a></th><th><a href=\"?C=M;O=A\">Last modified</a></th><th><a href=\"?C=S;O=A\">Size</a></th><th><a href=\"?C=D;O=A\">Description</a></th></tr><tr><th colspan=\"5\"><hr></th></tr>";
   send(socket, body1, strlen(body1), MSG_NOSIGNAL);

   char * bodyp =(char *) malloc(500);
   sprintf(bodyp, "<tr><td valign=\"top\"><img src=\"/icons/index.gif\" alt=\"[PARENTDIR]\"></td><td><a href=\"%s\">Parent Directory</a></td><td>&nbsp;</td><td align=\"right\">  - </td><td>&nbsp;</td></tr>", fpathDup);
   send(socket, bodyp, strlen(bodyp), MSG_NOSIGNAL);

   int nentries = 0;
   struct dirent * d;
   //const char * tableEnt = "";
   while ((d = readdir(dirp)) != NULL) {
		
      char* code = (char*) malloc(2000);
		char *path = (char *)malloc(500);
		strcpy(path, docpath);
      if (path[strlen(path) - 1] != '/') {
			strcat(path, "/");
		}
		strcat(path, d->d_name);
		if (d->d_type == DT_DIR) {
         sprintf(code, "<tr><td valign=\"top\"><img src=\"/icons/telnet.gif\""
						" alt=\"[DIR]\"></td><td><a href=\"%s\">", fpathDup);
      } else if (strstr(path, ".gif") != NULL) {
         sprintf(code, "<tr><td valign=\"top\"><img src=\"/icons/red_ball.gif\""
						" alt=\"[   ]\"></td><td><a href=\"%s\">", path);
      } else if (strstr(path, ".html") != NULL) {
         sprintf(code, "<tr><td valign=\"top\"><img src=\"/icons/text.gif\""
						" alt=\"[   ]\"></td><td><a href=\"%s\">", path);
      } else if (strstr(path, ".svg") != NULL) {
         sprintf(code, "<tr><td valign=\"top\"><img src=\"/icons/image.gif\""
						" alt=\"[   ]\"></td><td><a href=\"%s\">", path);
      } else if (strstr(path, ".xbm") != NULL) {
         sprintf(code, "<tr><td valign=\"top\"><img src=\"/icons/binary.gif\""
						" alt=\"[   ]\"></td><td><a href=\"%s\">", path);
      } else {
         sprintf(code, "<tr><td valign=\"top\"><img src=\"/icons/unknown.gif\""
						" alt=\"[   ]\"></td><td><a href=\"%s\">", path);
      }
      send(socket, code, strlen(code), MSG_NOSIGNAL);
      send(socket, d->d_name, strlen(d->d_name), MSG_NOSIGNAL);
      
      struct stat st;
		stat(path, &st);

      char * mOne = "</a></td><td align=\"right\">";
      char * mTwo = ctime(&(st.st_mtime));
      char * mThree = "</td><td align=\"right\">";
      int size = st.st_size;
      char * mFour =(char*) malloc(150); 
      sprintf(mFour, "%d</td><td>&nbsp;</td></tr>\n", size);
      
      send(socket, mOne, strlen(mOne), MSG_NOSIGNAL);
      send(socket, mTwo, strlen(mTwo), MSG_NOSIGNAL);
      send(socket, mThree, strlen(mThree), MSG_NOSIGNAL);
      send(socket, mFour, strlen(mFour), MSG_NOSIGNAL);

		nentries++;
      
	}

   const char * finalMess1 = "<tr><th colspan=\"5\"><hr></th></tr></table></body></html>";
   send(socket, finalMess1, strlen(finalMess1), MSG_NOSIGNAL);

   free(headIndex);
   free(fpathDup);
   return;
}

//Send stats and logs to user
void displayLog(int socket,char * realpath) {
   const char * message = "HTTP/1.1 200 Document follows\r\nServer: CS 252 lab5\r\nContent-Type: text/html\r\n\r\n";
   send(socket, message, strlen(message), MSG_NOSIGNAL);
   const char* nameHead = "<title><head>Daniel (Daniel Son's) Realm Stats</title></head>";
   const char * nameBody = "<h2>Daniel Fruland's (Daniel Son's) Realm Stats</h2>\n";

   send(socket, nameHead, strlen(nameBody), MSG_NOSIGNAL);
   send(socket, nameBody, strlen(nameBody), MSG_NOSIGNAL);


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

//Process requests for CGI bins
void processCGI(int socket, char * realpath, char * docpath, char * args) {
   const char * message = "HTTP/1.1 200 Document follows\r\nServer: CS 252 lab5\r\n";
   send(socket, message, strlen(message), MSG_NOSIGNAL);
   printf("deezargs: %s\ndocp: %s\n", args, realpath);
   char * newPath =(char *) malloc(500);
   for(int i = 0; i < strlen(realpath); i++) {
      if (realpath[i] == '?') {
         newPath[i] = '\0';
         break;
      }
      newPath[i] = realpath[i];
   }
   if (docpath[strlen(docpath) - 3] == '.' && docpath[strlen(docpath) - 2] == 's' 
                                                   && docpath[strlen(docpath) - 1] == 'o') {
      processLoad(socket, realpath);
      return;
   }
   int pid = fork();
   if (pid < 0) {
      perror("fork");
      exit(1);
   }
   if (pid == 0) {
      if (args != NULL) {
         setenv("REQUEST_METHOD", "GET", 1);
         setenv("QUERY_STRING", args, 1);
      }

      dup2(socket, 1);
      close(socket);
      
      execl(newPath, args,0,0);
      
      exit(1);
   } else {
      waitpid(pid, NULL, 0);
   }

   
   
}

void processLoad(int socket, char * realpath) {
   //dlopen(realpath, RTLD_NOW);

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