#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <wait.h>
#include "socket.h"

void traitement_signal(int sig){
  int status;
  waitpid(sig,&status,0);
}
void initialiser_signaux(void){
  struct sigaction sa;
  sa.sa_handler = traitement_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if(sigaction(SIGCHLD, &sa, NULL) == -1){
    perror("sigaction(SIGCHLD)");
  }
  if(signal(SIGPIPE, SIG_IGN) == SIG_ERR){
    perror("signal");
  }
}

int main(void){
  int socket_serveur=creer_serveur(8080);
  while(1){
    int socket_client;
    initialiser_signaux();
    socket_client = accept(socket_serveur, NULL, NULL);
    if(socket_client == -1){
      perror("accept");/* traitement d'erreur */
    }/* On peut maintenant dialoguer avec le client */
    FILE *client=fdopen(socket_client, "a+");
    if(fork()!=0){
      char buffer[256];
      while(fgets(buffer, 255, client)!=NULL){
	buffer[strlen(buffer)-1]='\0';
        fprintf(client, "%s Pawnee", buffer);
      }
    }else{
      close(socket_client);
    }
  }
  return 0;
}


