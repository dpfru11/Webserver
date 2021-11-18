
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


void processRequest(int socket);
void expandFilePath(char * fpath, char * cwd, int socket);
void sendErr(int errno, int socket, char * conttype);
const char * contentType(char * str);
int QueueLength = 5;


int main(int argc, char** argv)
{
   int port;

   if (argc == 1) {
      port = 15000;
   } else {
      int port = atoi( argv[1] );
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
      processRequest(slaveSocket);
      close(slaveSocket);
   }
}

void processRequest(int socket) {
   const int maxHead = 1024;
   char head[ maxHead + 1 ];
   char docpath[maxHead * 2];
   int length = 0;
   int n;
   int gotGet = 0;

   // Current character 
   unsigned char newChar;

  // Last character
   unsigned char lastChar = 0;

   while(n = read(socket, &newChar, sizeof(newChar))) {
      length++;
      if(newChar == ' ') {
         if (gotGet == 0) {
            gotGet = 1;
         } else {
            head[length-1]=0;
            strcpy(docpath, head);
         }
      } else if(newChar == '\n' && lastChar == '\r') {
         break;
      } else {
         lastChar = newChar;
         head[length-1] = newChar;
      } 
   }
   
   char * cwd = (char *)malloc(256);
   char * filepath = (char *)malloc(4000);
   cwd = getcwd(cwd, sizeof(cwd));
   if (strncmp(docpath, "/icons", 7) == 0) {
      filepath = strcat(cwd, "http-root-dir/");
      filepath = strcat(filepath, docpath);
   } else if (strncmp(docpath, "/htdocs", 8) == 0) {
      filepath = strcat(cwd, "http-root-dir/");
      filepath = strcat(filepath, docpath);
   } else if (strlen(docpath) == 1 && docpath[0] == '/') {
      filepath = strcat(cwd, "http-root-dir/htdocs/index.html");
   } else {
      filepath = strcat(cwd, "http-root-dir/htdocs");
      filepath = strcat(filepath, docpath);
   }
   
   //file expansion
   expandFilePath(filepath, cwd, socket);
   
   //Determine content type
   const char * cont = contentType(filepath);

}

void expandFilePath(char * fpath, char * cwd, int socket) {
   char * newPath = (char *) malloc(strlen(fpath) + 10);
   realpath(fpath, newPath);
   if (strlen(newPath) < strlen(cwd) + strlen("/http-root-dir")) {
      sendErr(405, socket, NULL);
   }

}

void sendErr(int errno, int socket, char * conttype) {
   if (errno == 405) {
      const char * err = "\r\n405 ERROR: Invalid directory backtrack\r\n";
      write(socket, err, strlen(err));
   } else if (errno == 404) {
      const char * errtype = "\r\nHTTP/1.1 200 Document follows \r\n";
      const char * server = "Server: \r\n CS252 lab5 \r\n";
      const char * content = malloc(30);
      sprintf(content, " %s ", conttype);
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

