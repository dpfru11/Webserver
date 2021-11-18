
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
expandFilePath(char * fpath, int socket);
sendErr(int errno, int socket);
int QueueLength = 5;


int main(int argc, char** argv)
{
   if (argc != 2) {
      perror("Usage: Port");
      exit(0);
   }
   
   int port = atoi( argv[1] );
   
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
   }
   
   if (strncmp(docpath, "/htdocs", 8) == 0) {
      filepath = strcat(cwd, "http-root-dir/");
      filepath = strcat(filepath, docpath);
   } 
   if (strlen(docpath) == 1 && docpath[0] == '/') {
      filepath = strcat(cwd, "http-root-dir/htdocs/index.html");
   } else {
      filepath = strcat(cwd, "http-root-dir/htdocs");
      filepath = strcat(filepath, docpath);
   }
   
   //file expansion
   expandFilePath(filepath, socket);
}

expandFilePath(char * fpath, int socket) {
   char * newPath = (char *) malloc(strlen(fpath) + 10);
   realpath(fpath, newPath);
   if (strlen(newPath) < )

}

sendErr(int errno, int socket) {
   const char * err = "Invalid directory backtrack";
   write(socket, err, strlen(err))
}

