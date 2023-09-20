#include "csapp.h"
#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

//아래 세개는 새로 만들 헤더에 쓰임.
static const char *host_header = "Host: %s\r\n"; // 호스트 이름
static const char *connection_header = "Connection: close\r\n"; // connection 헤더
static const char *proxy_connection_header = "Proxy-Connection: close\r\n";

//아래 네개는 헤더 추출할 때 쓰임.
static const char *user_agent_key = "User_Agent";
static const char *host_key = "Host";
static const char *connection_key = "Connection";
static const char *proxy_connection_key = "Proxy_Connection";

void doit(int fd);
void make_header(char *header, char *hostname, char *path, rio_t *rio, char* method);
int parse_uri(char *uri, char *hostname, char *port, char *filename);

// 아래 두 줄 빼고는 tiny의 main과 동일
int main(int argc, char **argv) {
  
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  printf("현재 port(argv[1]):%s\n",argv[1]);
  listenfd = Open_listenfd(argv[1]);

  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,&clientlen);  
    printf("connfd:%d\n",connfd); 
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   
    Close(connfd);  
  }

  print("%s", user_agent_hdr); //추가
  return 0; // 추가
}

/*
 doit(fd)
 한 개의 HTTP 트랜잭션을 처리한다.
 클라이언트의 요청 라인을 확인해서 정적, 동적 컨텐츠를 확인하고 돌려준다.
*/
void doit(int fd)
{
  int clientfd; // 클라이언트 파일 디스크립터
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], server_header[MAXLINE];
  char filename[MAXLINE], hostname[MAXLINE], port[MAXLINE];
  rio_t rio, server_rio;
  size_t n;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  // 요청 라인을 읽고 분석
  Rio_readlineb(&rio, buf, MAXLINE); 
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s", method, uri);

  parse_uri(uri, hostname, port, filename);

  make_header(server_header, hostname, filename, &rio, method); // 여기서 만들어진 헤더는 server_header에 저장됨

  clientfd = Open_clientfd(hostname, port); // 서버와 프록시랑 연결해주는 식별자(서버의 ip와 포트를 인자로 넣음)
  Rio_readinitb(&server_rio, clientfd); // 공유하는 버퍼 주소 초기 설정(공유하는 공간 만듦)
  Rio_writen(clientfd, server_header, strlen(server_header)); // 프록시에서 서버로 헤더를 보냄.

  //Rio_readlineb를 통해 서버로부터 응답을 기다림. 이후 응답이 오면 while문 진입

  //Rio_readlineb : 복사한 byte 수 리턴 / EOF 일 때 0 리턴
  while((n = Rio_readlineb(&server_rio, buf, MAXLINE))!=0) 
  { //응답이 오면
    printf("%s\n", buf); 
    Rio_writen(fd, buf, n); //응답을 클라이언트에게 보냄(fd). 이후 Rio_readlineb은 읽은걸 null로 만들기 때문에 EOF가 돼서 0 리턴되므로 while문 빠져나옴.
  }

  /*근데 응답이 언제 올 줄 알고 while문을 기다리냐.
  이는 운영체제에서 정해진 규칙에 따라 이벤트를 처리 하기 때문. 지금은 그냥 이런 이벤트가 있구나 하고 넘겨.*/

  Close(clientfd); 
}


/*
 make_header : 필요한 header들을 만들고 묶음
*/
void make_header(char *header, char *hostname, char *path, rio_t *rio, char* method)
{
  char buf[MAXLINE], r_host_header[MAXLINE], request_header[MAXLINE], other_header[MAXLINE];

  sprintf(request_header, "%s %s HTTP/1.0\r\n", method, path); // 요청 헤더 생성
  //요청 라인에 HTTP version이 1.1로 들어와도 end server에 보낼 때는 1.0으로 바꿔서 보낸다.
  

  //strcmp() 문자열 비교 함수,  같으면 0 출력
  //strncasecmp() 대소문자를 무시하고, 지정한 길이만큼 문자열을 비교하는 함수. 같으면 0 출력
  //Rio_readlineb 함수는 텍스트 줄을 파일 rio에서 부터 읽고, 읽은 것들을 메모리 위치 buf로 복사하고, **읽은 텍스트 라인을 널(0) 문자로 바꾸고** 종료시킨다.
  //Rio_readlineb : 복사한 byte 수 리턴 / EOF 일 때 0 리턴 / 에러일 때 -1 리턴

  while(Rio_readlineb(rio, buf, MAXLINE) > 0){ // 복사할 게 남아 있을 동안
    if(strcmp(buf, "\r\n")==0){ // 헤더 마지막 줄이면 break
      break;
    }

    /*
    r_host_header: 이 헤더는 클라이언트로부터 받은 요청 헤더 중에 "Host" 헤더를 가로채서 저장할 것이다. "Host" 헤더에는 요청한 호스트(웹 서버)의 이름이 들어있다. 
                    만약 클라이언트 요청 헤더에 이미 "Host" 헤더가 있다면 이 헤더를 그대로 사용하고(120라인), 없다면 프록시 서버가 직접 생성하여 사용한다. (137라인)
                    프록시 서버는 이 헤더를 서버 요청 헤더에 포함시켜야 하므로 따로 추출해두는 것이다.
    */
    
    if(!strncasecmp(buf, host_key, strlen(host_key))){ // *host_key = "HOST" (18번 라인에 정의) => 현재 buf가 HOST 헤더이면
      strcpy(r_host_header, buf); // host헤더를 그대로 사용하기.
      continue;
    } // 반복문을 돌면서 이 host 헤더를 발견하지 못했을 때 아래 'if(strlen(r_host_header) == 0)' 에서 걸려서 헤더를 새로 생성함.


    /*
    other_header: 이 헤더는 "Host," "Connection," "Proxy-Connection," "User-Agent,"와 같은 일부 헤더를 제외한 나머지 요청 헤더를 모두 저장하는 변수이다. 
    클라이언트로부터 받은 요청에서 이 헤더들을 추출하고, 이들을 서버 요청 헤더에 포함시키지 않아야 하는 경우 사용된다.
    */
   //abcde
    if(strncasecmp(buf, connection_key, strlen(connection_key)) // *connection_key = "Connection" (19번 라인)
        && strncasecmp(buf, proxy_connection_key, strlen(proxy_connection_key))  //*proxy_connection_key = "Proxy_Connection" (20번 라인)
        && strncasecmp(buf, user_agent_key, strlen(user_agent_key))){ //*user_agent_key = "User_Agent" (17번 라인)
      //위 세가지 헤더가 아닌 기타 나머지 요청 헤더들을 other_header에 저장한다.
      strcat(other_header, buf); // 문자열을 붙인다. strcat(최종문자열, 붙일문자열) => strcat을 사용하는 이유는 반복문을 돌면서 other_header들을 수집하기 때문.
    }
  }
  if(strlen(r_host_header) == 0) // host헤더를 발견하지 못했을 경우 새로 host 헤더 생성.
  {
    sprintf(r_host_header, host_header, hostname);
  }

  sprintf(header, "%s%s%s%s%s%s%s", 
          request_header, r_host_header, connection_header,
          proxy_connection_header, user_agent_hdr, other_header, "\r\n");
  return;
}

/*
parse_uri
uri를 파싱해서 나누어서 처리해줌
*/
//URI 예시 http://127.0.0.1:8000/htmls/index.html
int parse_uri(char *uri, char *hostname, char *port, char *filename)
{
  char *p;
  char arg1[MAXLINE], arg2[MAXLINE];

  if(p = strchr(uri, '/')) //문자열 uri 내에 일치하는 문자 '/' 가 있는지 검사하는 함수. 있으면 '/'를 가리키는 포인터 리턴
  { // http:   여기가 p-->//127.0.0.1:8000/htmls/index.html
    sscanf(p + 2, "%s", arg1); // 위에서 p+2 하면 127...부터 읽어와 arg1에 저장 => ex)127.0.0.1:8000/htmls/index.html
  }
  else
  {
    strcpy(arg1, uri); //문자열 내에 '/' 가 없으면 uri전체를 arg1에 저장 (무조건 들어오긴함)
  }

  if (p = strchr(arg1, ':')){ //문자열 uri 내에 일치하는 문자 ':' (포트번호) 가 있는지 검사하는 함수. 있으면 ':'를 가리키는 포인터 리턴 
    // 127.0.0.1   여기가 p-->:8000/htmls/index.html
    *p = '\0'; // :를 \0으로 바꿈 -> arg1은 이제 127.0.0.1 이 됨
    sscanf(arg1, "%s", hostname); // arg1을 hostname에 저장  => hostname 은 127.0.0.1
    sscanf(p+1, "%s", arg2); //포트(와 그 뒤)를 arg2에 저장 => arg2 는 8000/htmls/index.html

    p = strchr(arg2, '/'); //arg2 즉 포트 뒤에 '/'가 있다면 '/'를 가리키는 포인터를 p에 저장 => 8000  여기-->/  htmls/index.html
    *p = '\0'; // /를 \0으로 바꿈 --> arg2는 이제 8000이 됨
    sscanf(arg2, "%s", port); //port에 8000 을 저장함
    *p = '/'; // 여기가 p-->/htmls/index.html
    sscanf(p, "%s", filename); //filename은 /htmls/index.html 가 됨.

  }
  else{ //문자열 uri 내에 포트번호가 없다면 ex) uri가 http://127.0.0.1/htmls/index.html 라면 160~167줄을 통해 arg1은 127.0.0.1/htmls/index.html 
    p = strchr(arg1, '/'); //  127.0.0.1  여기가 p-->/htmls/index.html 
    *p = '\0'; //arg1에서 '/' 를 없애고 => 127.0.0.1 \0 htmls/index.html => arg1은 127.0.0.1 이 됨
    sscanf(arg1, "%s", hostname); //ag1을 hostname에 저장. hostname 은 127.0.0.1
    *p = '/';
    sscanf(p, "%s", filename); // filename은 /htmls/index.html 가 됨.

  }
  return;
}
