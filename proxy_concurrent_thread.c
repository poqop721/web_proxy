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
void *thread(void *vargp);


int main(int argc, char **argv) {
  
  int listenfd, *connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  pthread_t tid; // 피어 쓰레드의 쓰레드 ID를 저장할 변수

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  printf("현재 port(argv[1]):%s\n",argv[1]);
  listenfd = Open_listenfd(argv[1]);

  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Malloc(sizeof(int));
    *connfd = Accept(listenfd, (SA *)&clientaddr,&clientlen);  
    printf("connfd:%d\n",*connfd); 

    Pthread_create(&tid, NULL, thread, connfd); // 새로운 피어 쓰레드 생성 -> thread 함수를 돌게 함.
    //위 함수가 리턴될 때, 메인 쓰레드와 새로 생성된 피어 쓰레드는 동시에 돌고 있으며, tid는 새로운 쓰레드의 ID를 갖고있다.
  }

  print("%s", user_agent_hdr); 
  return 0; 
}

/* //malloc을 안 쓰기 위한 방법 (위험성 있음)
  while (1) {
    clientlen = sizeof(clientaddr);
    int cfd = Accept(listenfd, (SA *)&clientaddr,&clientlen);  
    Pthread_create(&tid, NULL, thread, (void*)cfd);
  }

  print("%s", user_agent_hdr); 
  return 0; 
}

void *thread(void *vargp)
{
  int connfd = (int)vargp;
  Pthread_detach(pthread_self()); 
*/

// 피어 쓰레드를 위한 쓰레드 루틴
void *thread(void *vargp)
{
  int connfd = *((int*)vargp);
  Pthread_detach(pthread_self()); 
  // pthread_self 로 자신의 쓰레드 ID 결정됨. 이후 ID는 tid값으로 리턴됨.
  // pthread_self 로 리턴받은 자신의 ID가 담긴 tid를 pthread_detach 에 넣으므로써 자신을 분리함.
  
  // 이렇게 분리 하는 이유는 기본적으로 쓰레드는 연결 가능 상태로 생성되며, 
  // 연결 가능 상태인 쓰레드는 다른 쓰레드에 의해 청소되기 전까지 자신의 메모리 자원들이 남아있기 때문에
  // pthread_exit로 삭제시키거나 이나 pthread_detach 로 분리해야 한다.
  // 분리된 쓰레드는 쓰레드가 종료할 때 자신의 자원들 또한 반환된다.
  Free(vargp);
  doit(connfd);
  Close(connfd);
  return NULL;
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
