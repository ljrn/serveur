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
char *renvoie_reponse(int bad){
  if(bad==400){
    return "HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 17\r\n\r\n400 Bad request\r\n";
  }else if(bad==404){
    return "HTTP/1.1 404 Not Found\r\n";
  }
  return "HTTP/1.1 200 OK\r\nContent-Length: 20\r\n";
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
      int ligne=0;
      int nonVide=0;
      int bad = 0;
      while(nonVide!=1 && fgets(buffer, 255, client)!=NULL){
	if(strcmp(buffer,"\r\n")!=0){
	  char *err400="GET / HTTP/1.1\r\n";
	  char *err404="GET /inexistant HTTP/1.1\r\n";
	  if(ligne==0){
	    if(strcmp(buffer,err404)==0){
	      bad=404;
	    }
	    else if(strcmp(buffer,err400)!=0){
	      bad=400;
	    }
	    ligne++;
	  }
	}else{
	  nonVide=1;
	}
      }
      char *rep=renvoie_reponse(bad);
      fprintf(client, "%s\n", rep);
      fflush(client);
    }else{
      close(socket_client);
    }
  }
  return 0;
}


