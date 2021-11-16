
#include <pthread.h>

struct ThreadParams{
   const Server * server;
   Socket_t sock;
};

void dispatchThread( ThreadParams* params) {printf("Dispatch Thread\n");// Thread dispatching this request
   params->server->handle(params->sock);
   // Delete params struct 
   delete params;
}
main(int argc, char** argv)
{
   // Add your HTTP implementation here


}

void Server::run_thread() const {
   while (1) {
      // Accept request
      Socket_tsock = _acceptor.accept_connection();
      // Put socket in new ThreadParams struct
      ThreadParams* threadParams= new ThreadParams;
      threadParams->server = this;
      threadParams->sock = std::move(sock);
      // Create thread
      pthread_t thrID;
      pthread_attr_tattr;
      pthread_attr_init(&attr);
      pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
      pthread_create(&thrID, &attr, (void* (*)(void*) )dispatchThread, (void *) threadParams);
   }
}

