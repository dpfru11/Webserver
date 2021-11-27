
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
void follow200(int socket, const char * conttype, int fd);
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

   char *cwd = (char *)malloc(maxHead * 5);
	cwd = getcwd(cwd, 256);
	char *filepath = (char *)malloc(maxHead * 5);
	strcpy(filepath, cwd);
   //printf("ya\n");
   //char* h = strstr(docpath, "/icons");
   printf("hmmm\n");
   
   //printf("%s\n", docpath);
   if (dPathSize == 1 && strcmp(docpath, "/") == 0) {
      printf("in here\n");
      filepath = strcat(cwd, "/http-root-dir/htdocs/index.html");
      printf("in here%s\n", filepath);
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
   printf("closed\n");
   close( socket );
}

void expandFilePath(char * fpath, char * cwd, int socket) {
   char * newPath = (char *) malloc(2000);
   fpath = realpath(fpath, newPath);

   if (strlen(newPath) < (strlen(cwd) + strlen("/http-root-dir"))) {
      sendErr(405, socket, NULL);
      return;
   }

   //Determine content type
   const char * contType = contentType(fpath);

   //Attempt to open
   int fd = open(fpath, O_RDONLY);
   printf("fd:%d\n", fd);
   if (fd < 0) {
      sendErr(404, socket, contType);
      return;
   } else {
      follow200(socket, contType, fd);
   }

}

//TODO: Not just for error, also for writing (200)
void sendErr(int errno, int socket, const char * conttype) {
   if (errno == 405) {
      const char * err = "\r\n405 ERROR: Invalid directory backtrack\r\n";
      write(socket, err, strlen(err));
   } else if (errno == 404) {
      const char * errtype = "\r\nHTTP/1.1 404 File not found\r\n";
      const char * server = "Server: CS 252 lab5\r\n";
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

void follow200(int socket, const char * conttype, int fd) {
   /*
      HTTP/1.1 <sp> 200 <sp> Document <sp> follows <crlf> 
      Server: <sp> <Server-Type> <crlf> 
      Content-type: <sp> <Document-Type> <crlf> 
      {<Other Header Information> <crlf>}* 
      <crlf> 
      <Document Data>
   */
  printf("conttype: %s\n", conttype);
   const char * message = "HTTP/1.1 200 Document follows\r\nServer: CS 252 lab5\r\nContent-Type: ";
   write(socket, message, strlen(message));
   write(socket, conttype, strlen(conttype));
   write(socket, "\r\n\r\n", 4);
   int n;
   char c;
   while(n = read(fd, &c, 1) > 0) {
      write(socket, &c, 1);
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

