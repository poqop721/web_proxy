#include "csapp.h"
#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";


static const char *host_header = "Host: %s\r\n"; 
static const char *connection_header = "Connection: close\r\n"; 
static const char *proxy_connection_header = "Proxy-Connection: close\r\n";


static const char *user_agent_key = "User_Agent";
static const char *host_key = "Host";
static const char *connection_key = "Connection";
static const char *proxy_connection_key = "Proxy_Connection";

void doit(int fd);
void make_header(char *header, char *hostname, char *path, rio_t *rio, char* method);
int parse_uri(char *uri, char *hostname, char *port, char *filename);


/*
서버들은 대개 장시간 동안 돌아가므로 좀비 자식들을 청소하는 SIGCHLD 핸들러를 포함해야 한다.
SIGCHLD 시그널들은 SIGCHLD 핸들러가 돌고 있는 동안에는 블록되고, 
리눅스 시그널들은 큐에 들어가지 않기 때문에 SIGCLD 핸들러는 다수의 좀비 자식들을 청소할 준비를 해야한다.
*/
void sigchild_handler(int sig) 
{ // wiatpid 종료된 자식 프로세스의 리턴값을 요청하는 함수이며 성공시 자식 프로세스 id, 실패시 -1 리턴
  while (waitpid(-1, 0, WNOHANG) > 0)  //부모 프로세스는 waitpid 함수로 자식 프로세스의 정보를 받으면, 그제서야 자식 프로세스는 종료가 된다.
    ;
  return;
}


int main(int argc, char **argv) {
  
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  Signal(SIGCHLD, sigchild_handler); //좀비 자식들을 청소하는 SIGCHLD 핸들러

  printf("현재 port(argv[1]):%s\n",argv[1]);
  listenfd = Open_listenfd(argv[1]);

  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,&clientlen);  
    printf("connfd:%d\n",connfd); 
    //fork : 현재 프로세스의 복제본이 생성되며, 부모 프로세스와 자식 프로세스 두 개의 별도의 프로세스를 동시에 실행시키는 시스템 콜.
    if (Fork() == 0){ //fork를 이용해 자식 프로세스를 생성해 클라이언트를 서비스 하게 한다. => 동시성
      Close(listenfd); //자식 프로세서는 자신의 listening socket을 닫는다.
      doit(connfd);  //자식 프로세서는 자신이 맡은 클라이언트를 서비스한다.
      Close(connfd); // 자식 프로세서는 클라이언트와의 연결을 닫는다. => 메모리 누수를 피하기 자식은 자신의 connfd 사본을 닫는다.
      exit(0); // 자식 프로세서 끝냄.
    }
    Close(connfd);  //부모 프로세서는 연결된 소켓과의 연결을 끊는다. => 메모리 누수를 피하기 부모는 자신의 connfd 사본을 닫는다.
  }

  print("%s", user_agent_hdr); 
  return 0; 
}

void doit(int fd)
{
  int clientfd; 
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], server_header[MAXLINE];
  char filename[MAXLINE], hostname[MAXLINE], port[MAXLINE];
  rio_t rio, server_rio;
  size_t n;

  Rio_readinitb(&rio, fd);
  
  Rio_readlineb(&rio, buf, MAXLINE); 
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s", method, uri);

  parse_uri(uri, hostname, port, filename);

  make_header(server_header, hostname, filename, &rio, method); 

  clientfd = Open_clientfd(hostname, port); 
  Rio_readinitb(&server_rio, clientfd); 
  Rio_writen(clientfd, server_header, strlen(server_header)); 

  while((n = Rio_readlineb(&server_rio, buf, MAXLINE))!=0) 
  { 
    printf("%s\n", buf); 
    Rio_writen(fd, buf, n); 
  }

  Close(clientfd); 
}


void make_header(char *header, char *hostname, char *path, rio_t *rio, char* method)
{
  char buf[MAXLINE], r_host_header[MAXLINE], request_header[MAXLINE], other_header[MAXLINE];

  sprintf(request_header, "%s %s HTTP/1.0\r\n", method, path); 
  

  while(Rio_readlineb(rio, buf, MAXLINE) > 0){ 
    if(strcmp(buf, "\r\n")==0){ 
      break;
    }

    if(!strncasecmp(buf, host_key, strlen(host_key))){ 
      strcpy(r_host_header, buf); 
      continue;
    } 

    if(strncasecmp(buf, connection_key, strlen(connection_key)) 
        && strncasecmp(buf, proxy_connection_key, strlen(proxy_connection_key))  
        && strncasecmp(buf, user_agent_key, strlen(user_agent_key))){ 
      
      strcat(other_header, buf); 
    }
  }
  if(strlen(r_host_header) == 0) 
  {
    sprintf(r_host_header, host_header, hostname);
  }

  sprintf(header, "%s%s%s%s%s%s%s", 
          request_header, r_host_header, connection_header,
          proxy_connection_header, user_agent_hdr, other_header, "\r\n");
  return;
}

int parse_uri(char *uri, char *hostname, char *port, char *filename)
{
  char *p;
  char arg1[MAXLINE], arg2[MAXLINE];

  if(p = strchr(uri, '/')) 
  { 
    sscanf(p + 2, "%s", arg1); 
  }
  else
  {
    strcpy(arg1, uri); 
  }

  if (p = strchr(arg1, ':')){ 
    
    *p = '\0'; 
    sscanf(arg1, "%s", hostname); 
    sscanf(p+1, "%s", arg2); 

    p = strchr(arg2, '/');
    *p = '\0'; 
    sscanf(arg2, "%s", port); 
    *p = '/';
    sscanf(p, "%s", filename);

  }
  else{ 
    p = strchr(arg1, '/');
    *p = '\0'; 
    sscanf(arg1, "%s", hostname);
    *p = '/';
    sscanf(p, "%s", filename); 
  }
  return;
}
