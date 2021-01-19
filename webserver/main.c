#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include "socket.h"

void initialiser_signaux(void){
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
    const char*message_bienvenue = "Bonjour, bienvenue sur mon serveur\n";
    for(int i=0; i<10;i++){
      sleep(1);
      write(socket_client, message_bienvenue, strlen(message_bienvenue));
    }
  }
  return 0;
}


