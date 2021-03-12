#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <wait.h>
#include <dirent.h>
#include "socket.h"
#include "http_parse.h"
#include "stats.h"
#include <semaphore.h>
#include <pthread.h>

web_stats *statistiques;
char *rewrite_target(char *target){
  if(strcmp("/",target)==0)return "/index.html";
  int i=0;
  while(target[i]!='\0' && target[i] != '?'){
    i++;
  }
  target[i]='\0';
 if(strstr(target,"../")!=NULL){
    return NULL;
  }
  return target;
}

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

void disconnect_client(FILE *client){
  shutdown(fileno(client), SHUT_RDWR);
  fclose(client);
}
char *fgets_or_exit(char *buffer, int size, FILE *stream){
  if(fgets(buffer, size, stream) == NULL) {
		exit(0);
	}
	return buffer;
}
void skip_headers(FILE *client){
  char buffer[256];
  while(strcmp(buffer,"\r\n")!=0 && strcmp(buffer,"\n")!=0){
    if(fgets(buffer,255,client)==NULL){
      exit(0);
    }
  }
}
int get_file_size(int fd){
  struct stat statbuf;
  int res=fstat(fd,&statbuf);
  if(res==-1){
    perror("stat");
  }
  return statbuf.st_size;
}

void send_status(FILE *client, int code, const char* reason_phrase){
  fprintf(client, "HTTP/1.1 %d: %s\r\n",code, reason_phrase);
  fflush(client);
}
void send_response(FILE *client, int code, const char *reason_phrase, const char *message_body, int length){
  send_status(client, code, reason_phrase);
  fprintf(client, "Content-Length: %d\r\nContent-Type: text/html; charset=utf-8\r\n\r\n", length);
  fprintf(client,"%s",message_body);
  fflush(client);
}

FILE *check_and_open(const char *target, const char *document_root)
{
	char * chemin = malloc(sizeof(target)+sizeof(document_root)+128);
	strcpy(chemin, document_root);
	strcat(chemin, target);
	printf("%s\n",chemin);
	FILE *f;
	f = fopen(chemin, "rb+");
	free(chemin);
	if(f == NULL) {
		perror("erreur d'ouverture du fichier");
		return NULL;
	}
	return f;

}


int copy(FILE *in, FILE *out)
{
	char c;
	while(fread(&c, 1, 1, in) == 1) {
		fwrite(&c, 1, 1, out);
	}
	return 0;
}


void send_stats(FILE *client){
  web_stats *stat=get_stats();
  char chaine[1000];
  sprintf(chaine,"Served Connections: %d\r\nServed Requests: %d\r\nOK 200: %d\r\nKO 400:%d\r\nKO 403: %d\r\nKO 404: %d\r\n", stat->served_connections, stat->served_requests,stat->ok_200,stat->ko_400,stat->ko_403, stat->ko_404);
  send_response(client, 200, "OK", "", strlen(chaine));
  fprintf(client,"%s", chaine);
}

int main(int argc, char **argv){
  DIR* directory=opendir(argv[argc-1]);
  if(directory==NULL){
    perror("Pas un répertoire");
    exit(1);
  }
  int socket_serveur=creer_serveur(8004);
  init_stats();
  statistiques=get_stats();
  sem_t semaphore;
  sem_init(&semaphore, 1, 1);
  initialiser_signaux();
  while(1){
    int socket_client;
    socket_client = accept(socket_serveur, NULL, NULL);
    if(socket_client == -1){
      perror("accept");
    }
    FILE *client=fdopen(socket_client, "a+");
    printf("nouveau client\n");
    sem_wait(&semaphore);
    statistiques->served_connections+=1;
    sem_post(&semaphore);
    if(fork()==0){
      printf("nouvelle requête\n");
      sem_wait(&semaphore);
      statistiques->served_requests+=1;
      sem_post(&semaphore);
      char buffer[256];
      http_request request;
      int rep=parse_http_request(fgets_or_exit(buffer,255,client),&request);
	 printf("rep\n");
      if(rep == -1) {
	if(request.method == HTTP_UNSUPPORTED){
	  printf("405\n");
	  send_response(client, 405, "Method Not Allowed", "Method Not Allowed\r\n", strlen("Method Not Allowed\r\n"));
	}else{
	  sem_wait(&semaphore);
	  statistiques->ko_400+=1;
	  sem_post(&semaphore);
	  printf("400\n");
	  send_response(client, 400, "Bad Request", "Bad Request\r\n", strlen("Bad Request\r\n"));
	}
      }else{
	char *target=rewrite_target(request.target);
	if(target ==NULL){
	  sem_wait(&semaphore);
	  statistiques->ko_403+=1;
	  sem_post(&semaphore);
	  printf("403\n");
	  send_response(client, 403, "Forbidden", "Forbidden\r\n", strlen("Forbidden\r\n"));
	}else if(strcmp(target,"/stats")==0){
	  printf("stats\n");
	  send_stats(client);
	}else{
	  FILE *fichier=check_and_open(target,argv[argc-1]);
	  if(fichier==NULL){
	    sem_wait(&semaphore);
	    statistiques->ko_404+=1;
	    sem_post(&semaphore);
	    printf("404\n");
	    send_response(client, 404, "Not Found", "Not Found\r\n", strlen("Not Found"));
	  }else{
	    int taille=get_file_size(fileno(fichier));
	    sem_wait(&semaphore);
	    statistiques->ok_200+=1;
	    sem_post(&semaphore);
	    printf("%d\n", statistiques->ok_200);
	    send_response(client, 200, "OK", "", taille);
	    copy(fichier,client);
	  }
	}
      }
      skip_headers(client);
    }
    fclose(client);
  }
  return 0;
}

  
