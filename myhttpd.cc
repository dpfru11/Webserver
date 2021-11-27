
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>


void processRequest(int socket);
void expandFilePath(char * fpath, char * cwd, int socket);
void sendErr(int errno, int socket, const char * conttype);
const char * pass = "ZGFuaWVsc29uOmZlbmNl";
const char * contentType(char * str);
const char * realm = "CS252-DANREALM";
int QueueLength = 5;


int main(int argc, char** argv)
{
   int port;

   if (argc == 1) {
      port = 5565;
   } else {
      port = atoi( argv[1] );
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

   error = listen( masterSocket, QueueLength);

   while(1) {
      struct sockaddr_in clientIPAddress;
      int alen = sizeof( clientIPAddress );
      int slaveSocket = accept( masterSocket, (struct sockaddr *)&clientIPAddress,
      (socklen_t*)&alen);
      //printf("kajhgjagl");
      processRequest(slaveSocket);
      close(slaveSocket);
   }
}

void processRequest(int socket) {
   const int maxHead = 1024;
   char str[ maxHead * 100 ];
   char head[ maxHead * 100 ];
   char * docpath = (char*)malloc(maxHead * 10);
   int length = 0;
  
   int n;
   
   // Current character 
   unsigned char newChar;

  // Last character
   unsigned char lastChar = 0;
   unsigned char lastlastChar = 0;
   unsigned char lastlastlastChar = 0;
   //printf("made it");
   while(n = read(socket, &newChar, sizeof(newChar))) {
       
      length++;
      if(newChar == '\n' && lastChar == '\r') {
         //printf("made it");
         break;
      } else {
         printf("%c", lastChar);
         lastChar = newChar;
         str[length-1] = newChar;
      } 
   }
   length = 0;
   while(n = read(socket, &newChar, sizeof(newChar))) {
       
      length++;
      if(newChar == '\n' && lastChar == '\r' && lastlastChar == '\n' && lastlastlastChar=='\r') {
         //printf("made it\n");
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
      
      //printf("token: %s\n", token);
      if (strcmp(token, "Authorization: Basic ZGFuaWVsc29uOmZlbmNl") == 0) {
         printf("madeitoaihgia\n");
         authorized = true;
         break;
      }
      token = strtok(NULL, "\r\n");
   }

   if (authorized == false) {
      printf("AYAYAYAYAYA\n");
      sendErr(401, socket, NULL);
      return;
   }
   printf("madeitoaihgia\n");
   //obtain docpath
   bool foundDPath = false;
   int dPathSize = 0;
   for (int i = 0; i < maxHead * 10; i++) {
      if (str[i] == ' ' && foundDPath == false) {
         foundDPath = true;
         i;
         continue;
      }
      if (foundDPath == true && str[i] == ' ') {
         docpath[dPathSize] = '\0';
         printf("%s", docpath);
         break;
      }
      if (foundDPath == true) {
         docpath[dPathSize] = str[i];
         dPathSize++;
      }
   }
   //int i = 0;
   /*token = strtok(str, " ");
   while (token)
   {
      printf("token: %s\n", token);
      
      if (i == 1) {
         docpath =(char *) malloc(strlen(token) + 1);
         strcpy(docpath, token);
         //printf("\n%s\n", docpath);
         break;
      }
      token = strtok(NULL, " ");
      i++;
   }*/
   char * cwd = (char *)malloc(1000);
   char * filepath = (char *)malloc(4000);
   printf("yes\n");
   cwd = getcwd(cwd, sizeof(cwd));
   //printf("ya\n");
   //char* h = strstr(docpath, "/icons");
   printf("hmmm\n");
   
   //printf("%s\n", docpath);
   if (dPathSize == 1 && strcmp(docpath, "/") == 0) {
      printf("in here\n");
      filepath = strcat(cwd, "http-root-dir/htdocs/index.html");
      if (filepath == NULL) {
         printf("oops\n");
      }
   } else if (strstr(docpath, "/icons") != NULL) {
      filepath = strcat(cwd, "http-root-dir/");
      filepath = strcat(filepath, docpath);
   } else if (strstr(docpath, "/htdocs") != NULL) {
      filepath = strcat(cwd, "http-root-dir/");
      filepath = strcat(filepath, docpath);
   } else {
      filepath = strcat(cwd, "http-root-dir/htdocs");
      filepath = strcat(filepath, docpath);
   }
   printf("missed?\n");
   //file expansion
   expandFilePath(filepath, cwd, socket);
   
   close( socket );
}

void expandFilePath(char * fpath, char * cwd, int socket) {
   char * newPath = (char *) malloc(strlen(fpath) + 10);
   char * finalPath = realpath(fpath, newPath);

   if (strlen(newPath) < strlen(cwd) + strlen("/http-root-dir")) {
      sendErr(405, socket, NULL);
      return;
   }

   //Determine content type
   const char * contType = contentType(finalPath);

   //Attempt to open
   int fd = open(finalPath, O_RDONLY);

   if (fd < 0) {
      sendErr(404, socket, contType);
      return;
   }


}

//TODO: Not just for error, also for writing (200)
void sendErr(int errno, int socket, const char * conttype) {
   if (errno == 405) {
      const char * err = "\r\n405 ERROR: Invalid directory backtrack\r\n";
      write(socket, err, strlen(err));
   } else if (errno == 404) {
      const char * errtype = "\r\nHTTP/1.1 404 File not found\r\n";
      const char * server = "Server: CS252 lab5\r\n";
      char * content = (char *) malloc(30);
      sprintf(content, " %s\r\n", conttype);
      const char * finalcont = content;
      const char * notFound = "File not found!\r\n\r\n";

      write(socket, errtype, strlen(errtype));
      write(socket, server, strlen(server));
      write(socket, finalcont, strlen(finalcont));
      write(socket, notFound, strlen(notFound));
      delete content;
   } else if (errno == 401) {
      const char * errtype = "\r\nHTTP/1.1 401 Unauthorized\r\n";
      const char * auth = "WWW-Authenticate: Basic realm=CS252-DANREALM\r\n";
      write(socket, errtype, strlen(errtype));
      write(socket, auth, strlen(auth));
   }
   
}

const char * contentType(char * str) {
   if (strstr(str, ".html") != NULL || strstr(str, ".html/") != NULL) {
      return "text/html";
   } else if (strstr(str, ".gif") != NULL || strstr(str, ".gif/")) {
      return "image/gif";
   } else {
      return "text/plain";
   }
}

