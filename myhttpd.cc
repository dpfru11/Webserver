
#include <pthread.h>
#include <stdio.h>

void processRequest(int socket);


main(int argc, char** argv)
{
   // Add your HTTP implementation here
   int masterSocket = socket(PF_INET, SOCK_STREAM, 0);
   if (masterSocket < 0) {
      perror("socket");
      exit(1);
   }

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

   while(1){
      int slaveSocket = accept( masterSocket, (struct sockaddr *)&clientIPAddress,
      (socklen_t*)&alen)
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

   while((n = read(socket, &newChar, sizeof(newChar)) {
      length++;
      if(newChar == ' '){
         if gotGet == 0 {
            gotGet = 1;
         } else {
            head[length-1]=0;
            strcpy(docpath, head);
         }
      } else if(newChar == '\n' && oldChar == '\r') {
         break;
      } else{
         oldChar = newChar;
         if (gotGet == 1) {
            head[length-1] = newChar;
         }
         
      }
   }
}

