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

char *rewrite_target(char *target){
  if(strcmp("/",target)==0)return "/index.html";
  int i=0;
  while(target[i] != '?'){
    i++;
  }
  target[i]='\0';
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
  char *ret = fgets(buffer, size, stream);
  if (ret == NULL){
    if (ferror(stream)){
      perror("fgets");
      disconnect_client(stream);
      exit(1);
    }
    disconnect_client(stream);
    exit(0);
  }
  return ret;
}
void skip_headers(FILE *client){
  char buffer[256];
  char *comp=fgets_or_exit(buffer,255,client);
  while(strcmp(comp,"\r\n")!=0 && strcmp(comp,"\n")!=0){
    comp=fgets_or_exit(buffer,255,client);
  }
  return;
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
void send_response(FILE *client, int code, const char *reason_phrase, const char *message_body){
  send_status(client, code, reason_phrase);
  fprintf(client,"%s",message_body);
  fflush(client);
}

FILE *check_and_open(const char *target,const char *document_root){
  char *chemin = strdup(document_root);
  strcat(chemin,target);
  return fopen(chemin,"r");
}

int copy(FILE *in,FILE *out){
  if(in == NULL || out == NULL){
    fclose(in);
    fclose(out);
    return -1;
  }
  char ch;
   while( ( ch = fgetc(in) ) != EOF )fputc(ch, out);
   fclose(in);
   return 0;
}

int main(int argc, char **argv){
  DIR* directory=opendir(argv[argc-1]);
  if(directory==NULL){
    perror("Pas un répertoire");
    exit(1);
  }
  int socket_serveur=creer_serveur(8008);
  while(1){
    int socket_client;
    initialiser_signaux();
    socket_client = accept(socket_serveur, NULL, NULL);
    if(socket_client == -1){
      perror("accept");
    }
    FILE *client=fdopen(socket_client, "a+");
    if(fork()!=0){
      char buffer[256];
      http_request request;
      int rep=parse_http_request(fgets_or_exit(buffer,255,client),&request);
      if(rep == -1) {
	if(request.method == HTTP_UNSUPPORTED)
	  send_response(client, 405, "Method Not Allowed", "Method Not Allowed\r\n");
	else
	  send_response(client, 400, "Bad Request", "Bad request\r\n");
      }else{
	FILE *fichier=check_and_open(rewrite_target(request.target),argv[argc-1]);
	if(fichier==NULL){
	  send_response(client, 404, "Not Found", "Not Found\r\n");
	}else{
	  int taille=get_file_size(fileno(fichier));
	  send_response(client, 200, "OK", "OK\r\n");
	  fprintf(client, "Content-Length: %d\r\nContent-Type: text/html; charset=utf-8\r\n\r\n", taille);
	  copy(fichier,client);
	}
      }
      skip_headers(client);
      disconnect_client(client);
    }else{
      close(socket_client);
    }
  }
  return 0;
}


