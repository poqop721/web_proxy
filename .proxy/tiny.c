/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method); //11.11번 수정 (method 추가)
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs ,char *method); //11.11번 수정 (method 추가)
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

//argc 는 argv 인자의 크기.
// 입력 ./tiny 8000 / argc = 2, argv[0] = tiny, argv[1] = 8000
int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  //sockaddr 구조체는 소켓의 주소를 담는 기본 구조체 역할을 한다.
  //멤버변수 1: sa_family : 주소체계를 구분하기 위한 변수이며, 2 bytes 이다.
  //멤버변수 2: sa_data : 실제 주소를 저장하기 위한 변수다. 14 bytes 이다.
  //즉, 이 구조체는 16 bytes 의 와꾸(틀, 크기)를 잡아주는 녀석이다.

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  /* Open_listenfd 함수를 호출해서 듣기 소켓을 오픈한다. 인자로 포트번호를 넘겨준다. */
  // Open_listenfd는 위 포트번호에 연결 요청받을 준비가 된 듣기 식별자를 리턴한다 = listenfd
  printf("현재 port(argv[1]):%s\n",argv[1]);
  listenfd = Open_listenfd(argv[1]);

  /* 전형적인 무한 서버 루프를 실행*/
  while (1)
  {
  	//accpet 함수 인자에 넣기위한 주소길이를 계산
    clientlen = sizeof(clientaddr);
    
    /* 반복적으로 연결 요청을 접수 */
    // accept 함수는 1. 듣기 식별자, 2. 소켓주소구조체의 주소, 3. 주소(소켓구조체)의 길이를 인자로 받는다.
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // line:netp:tiny:accept
    /* client 소켓은 server 소켓의 주소를 알고 있으니까 
    client에서 server로 넘어올 때 add정보를 가지고 올 것이라고 가정*/
    printf("connfd:%d\n",connfd); //connfd 확인용

    // Getaddrinfo는 호스트 이름: 호스트 주소, 서비스 이름: 포트 번호의 스트링 표시를 소켓 주소 구조체로 변환
    // Getnameinfo는 위를 반대로 소켓 주소 구조체에서 스트링 표시로 hostname, port 변환.
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    
    printf("Accepted connection from (%s, %s)\n", hostname, port); //어떤 client가 들어왔는지 알려줌
    
    /* 트랜젝션을 수행 */
    doit(connfd); // line:netp:tiny:doit
    /* 트랜잭션이 수행된 후 자신 쪽의 연결 끝 (소켓) 을 닫는다. */
    Close(connfd); // line:netp:tiny:close
  }
}

// 클라이언트의 요청 라인을 확인해 정적, 동적 콘텐츠를 확인하고 돌려줌
void doit(int fd) //fd는 connfd
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  
  //rio - Robust I/O
  /* Rio 패키지는 짧은 카운트(short count)를 자동으로 처리한다 (Unix I/O 와 차이) .
   * 짧은 카운트가 발생할 수 있는 네트워크 프로그램 같은 응용에서 편리하고 안정적이고 효율적인 I/O 패키지이다
   *
   * 짧은 카운트란 일부의 경우에 unix io의 read, write 함수는 응용이 요청하는 것보다 더 적은 바이트를 전송한다. 
   * 이를 짧은 카운트라고 명시한다. 짧은 카운트는 에러를 나타내는 것은 아니다!
  */

  // rio_readlineb를 위해 rio_t 타입(구조체)의 읽기 버퍼를 선언
  rio_t rio;

  /* Read request line and headers */
  // rio_t 구조체를 초기화 해준다.
  Rio_readinitb(&rio, fd);

  /* Rio_readlineb를 통해 요청 라인을 읽어들이고, 분석한다. */
  Rio_readlineb(&rio, buf, MAXLINE); // buf : GET /test.mp4 HTTP/1.1
/*
Rio_readlineb 함수는 텍스트 줄을 파일 rio에서 부터 읽고, 읽은 것들을 메모리 위치 buf로 복사하고, 읽은 텍스트 라인을 널(0) 문자로 바꾸고 종료시킨다. 
rio_readlineb 함수는 최대 MAXLINE-1개의 바이트를 읽는다. MAXLINE-1를 넘는 텍스트 라인들은 잘라서 널 문자로 종료시킨다.
그리고 '\n' 개행 문자를 만날 경우 MAXLINE 전에 break 된다.!
이는 위에서 rio_writen를 통해 서버면 클라이언트, 클라이언트면 서버에서 보내진 정보를 읽을 때 사용한다.
*/

  printf("Request headers:\n");
  printf("%s", buf);
  //설명 2.2
  sscanf(buf, "%s %s %s", method, uri, version);
  /*
    method : GET
    uri : /test.mp4
    version : HTTP/1.1
  */
  //sscanf : buf에서 데이터를 "%s %s %s" 형식에 따라 읽어와 각각 데이터를 method, uri, version 에 저장하게 된다.


  /* Tiny 는 GET method 만 지원하기에 클라이언트가 다른 메소드 (Post 같은)를 
   *요청하면 에러메세지를 보내고, main routin으로 돌아온다
   */
   
  //strcmp() 문자열 비교 함수, 
  //strcasecmp() 대소문자를 무시하는 문자열 비교 함수 
  //strncasecmp() 대소문자를 무시하고, 지정한 길이만큼 문자열을 비교하는 함수
  //같으면 0(if문 무시) => method 가 GET이나 HEAD 가 아니먄 clienterror
  if (!(strcasecmp(method, "GET") == 0 || strcasecmp(method, "HEAD") == 0)) // 11.11 문제 수정
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  
  
  /* GET method라면 읽어들이고, 다른 요청 헤더들을 무시한다. */
  read_requesthdrs(&rio);


  /* Parse URI form GET request */
  /* URI 를 파일 이름과 비어 있을 수도 있는 CGI 인자 스트링으로 분석하고, 요청이 정적 또는 동적 컨텐츠를 위한 것인지 나타내는 플래그를 설정한다.  */
  is_static = parse_uri(uri, filename, cgiargs);
  // static 일땐 1, dynamic 일땐 0


  /* 만일 파일이 디스크상에 있지 않으면, 에러메세지를 즉시 클라아언트에게 보내고 메인 루틴으로 리턴*/
  // filename 경로에서 파일을 찾아서, 그 파일의 stat 구조체를 sbuf의 주소에 저장. 단 호출 실패시 -1 리턴.
  if (stat(filename, &sbuf) < 0) // filename 호출 실패
  {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }
  
  
  /* Serve static content */
  if (is_static)
  {
    /* 정적 컨텐츠이고 (위if), 이 파일이 보통 파일인지(S_ISREG), 읽기 권한을 가지고 있는지(S_IRUSR)) 검증한다. */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read this file");
      return;
    }
    /* 그렇다면 정적 컨텐츠를 클라이언트한테 제공. */
    serve_static(fd, filename, sbuf.st_size, method); // sbuf.st_size : 찾은 파일의 stat 사이즈, 즉 파일사이즈
  }// 11.11 문제 수정
  
  
  else /* Serve dynamic content */
  {
    /* 실행 가능한 파일인지 검증*/
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      printf("line160\n");
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read this file");
      return;
    }
    /* 그렇다면 동적 컨텐츠를 클라이언트에게 제공 */
    serve_dynamic(fd, filename, cgiargs, method);
  }// 11.11 문제 수정
}

/* 명백한 오류에 대해서 클라이언트에게 보고하는 함수. 
 * HTTP응답을 응답 라인에 적절한 상태 코드와 상태 메시지와 함께 클라이언트에게 보낸다. */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* BUILD the HTTP response body */
  /* 브라우저 사용자에게 에러를 설명하는 응답 본체에 HTML도 함께 보낸다 */
  /* HTML 응답은 본체에서 컨텐츠의 크기와 타입을 나타내야하기에, HTMl 컨텐츠를 한 개의 스트링으로 만든다. */
  /* 이는 sprintf를 통해 body는 인자에 스택되어 하나의 긴 스트리잉 저장된다. */
  // sprintf는 출력하는 결과 값을 변수에 저장하게 해주는 기능있음

  //str라는 (첫번째 인자)문자열 배열에 (두번째 인자)format(형식 문자열)에서 지정한 방식 대로 (세번째인자~끝까지)... 에 들어갈 인자들을 넣어준다. 
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web Server</em>\r\n", body);
  // body 배열에 차곡차곡 html 을 쌓아 넣어주고

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  
  //rio_writen 은 서버 -> 클라이언트, 클라이언트 -> 서버로 데이터를 전송 할 때 사용한다.
  //rio_writen은 (인자2)buf에서 식별자 (인자1)fd로 (인자3)n바이트를 전송한다.
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  // buf에 넣고 보내고 넣고 보내고
  
  // sprintf로 쌓아놓은 길쭉한 배열(body, buf)를 fd로 보내준다.
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

/* Tiny는 요청 헤더 내의 어떤 정보도 사용하지 않는다
 * 단순히 이들을 읽고 무시한다. 
 */
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  
  /* strcmp 두 문자열을 비교하는 함수, 같을 경우 0 반환 -> 이 경우만 탈출 */
  /* 헤더의 마지막 줄은 비어있기에 \r\n 만 buf에 담겨있다면 while문을 탈출한다.  */
  // buf가 '\r\n'이 되는 경우는 모든 줄을 읽고 나서 마지막 줄에 도착한 경우이다.
  while (strcmp(buf, "\r\n"))
  {
  	//rio 설명에 나와있다시피 rio_readlineb는 \n를 만날때 멈춘다.
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    // 멈춘 지점 까지 출력하고 다시 while
  }
  return;
}

  /* Parse URI form GET request */
  /* URI 를 파일 이름과 비어 있을 수도 있는 CGI 인자 스트링으로 분석하고, 요청이 정적 또는 동적 컨텐츠를 위한 것인지 나타내는 플래그를 설정한다.  */
  // Tiny는 정적 컨텐츠를 위한 홈 디렉토리가 자신의 현재 디렉토리이고,
  // 실행파일의 홈 디렉토리는 /cgi-bin이라고 가정한다.
  // URI 예시 static: /mp4sample.mp4 , / , /adder.html 
  //         dynamic: /cgi-bin/adder?first=1213&second=1232 
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  /* strstr 으로 cgi-bin이 들어있는지 확인하고 양수값을 리턴하면 dynamic content를 요구하는 것이기에 조건문을 탈출 */
  // strstr : 문자열 안에서 문자열로 검색하기 -> strstr(대상문자열, 검색할 문자열) -> 찾았으면 문자열로 시작하는 문자열의 포인터를 반환, 없으면 NULL
  if (!strstr(uri, "cgi-bin")) /*NULL 반환됨.  Static content */
  {
    strcpy(cgiargs, "");//cgiargs 인자 string을 지운다.
    strcpy(filename, ".");// 상대 리눅스 경로이름으로 변환 ex) '.'
    strcat(filename, uri); // 문자열을 붙인다. strcat(최종문자열, 붙일문자열)
    //결과 cgiargs = "" 공백 문자열, filename = "./~~ or ./home.html

	// uri 문자열 끝이 / 일 경우 허전하지 말라고 home.html을 filename에 붙혀준다.
    if (uri[strlen(uri) - 1] == '/')
      strcat(filename, "home.html");
    return 1;
  }

// uri 예시: dynamic: /cgi-bin/adder?first=1213&second=1232 
  else /* Dynamic content */
  {
    //index 함수는 문자열에서 특정 문자의 위치를 반환한다.
    ptr = index(uri, '?');
    
    //?가 존재한다면
    if (ptr)
    {
      // 인자로 주어진 값들을 cgiargs 변수에 넣는다. 
      strcpy(cgiargs, ptr + 1); // ptr + 1 부터 끝까지.(\0을 만날 때 까지). 즉 cgiargs => first=1213&second=1232
      
      /* uri에서 방금 복사한 값 제거 
      uri 예시: dynamic: /cgi-bin/adder*/
      *ptr = '\0';
    }
    else // ?없으면 빈칸으로 둔다
      strcpy(cgiargs, "");
    // 나머지 URI 부분을 상대 리눅스 파일이름으로 변환
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

// static content를 요청하면 서버가 disk에서 파일을 찾아서 메모리 영역으로 복사하고, 복사한 것을 client fd로 복사
void serve_static(int fd, char *filename, int filesize, char *method) //문제 11.11 수정
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype); //무슨 파일형식인지 검사해서 filetype을 채워넣음
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf); // while을 한번돌면 close가 되고, 새로 연결하더라도 새로 connect하므로 close가 default가됨
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype); //여기 \r\n 빈줄 하나가 헤더종료표시

  /* writen = client 쪽에 buf에서 strlen(buf) 바이트만큼 fd로 전송한다. */
  Rio_writen(fd, buf, strlen(buf));

  /* 서버 쪽에 출력 */
  printf("Response headers:\n"); 
  printf("%s", buf);

  
   //문제 11.11 수정
   if(strcasecmp(method, "HEAD") == 0) //head메소드면 return해서 header값만 보여주게 하라.
    return;

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0); // unix i/o에서 open 함수는 열려고 하는 파일의 식별자 번호를 리턴
    // open(열려고 하는 대상 파일의 이름, 파일을 열 때 적용되는 열기 옵션, 파일 열 때의 접근 권한 설명)
    // return 파일 디스크립터
    // O_RDONLY : 읽기 전용으로 파일 열기
    // 즉, filename의 파일을 읽기 전용으로 열어서 식별자를 받아온다.


  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // mmap함수를 이용해 바로 메모리를 할당하며 srcfd의 파일 값을 배정한다. 
  // //malloc이랑 유사한데 값도 복사해준다
  // Close(srcfd); //생성한 파일 식별자 번호인 srcfd를 close 해줌
  // Rio_writen(fd, srcp, filesize); // Rio_writen 함수(시스템 콜)을 통해 클라이언트에게 전송한다.
  // Munmap(srcp, filesize); //할당된 가상 메모리 해제 (free) - srcp의 주소부터 filesize 만큼 해제

  /* 숙제문제 11.9 */
  /*malloc의 경우 filesize 만큼의 가상 메모리를 할당한 후, Rio_readn으로 할당된 가상 메모리 공간의 시작점인 srcp를 기준으로 srcfd 파일을 읽어 복사해 넣는다.*/
  srcp = (char*)malloc(filesize);
  Rio_readn(srcfd, srcp, filesize); //writen 이 쓰기라면 readn은 읽기이다.
  Close(srcfd); //생성한 파일 식별자 번호인 srcfd를 close 해줌
  Rio_writen(fd, srcp, filesize);
  free(srcp);
}

/*
 * get_filetype - Derive file type from filename
 * HTML 서버가 처리할 수 있는 파일 타입을 이 함수를 통해 제공한다.
 * strstr 두번쨰 인자가 첫번째 인자에 들어있는지 확인
 */
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  /* 11.7 숙제 문제 - Tiny 가 MP4  비디오 파일을 처리하도록 하기.  */
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}


/*
serve_dynamic 함수는 HTTP 요청을 받아 CGI 프로그램을 실행하고,
CGI 프로그램의 출력을 클라이언트에게 전달하는 역할을 한다. 
이를 통해 동적인 콘텐츠를 생성하고 웹 서버를 동작시킬 수 있다.
*/
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method) //문제 11.11 수정
{
  char buf[MAXLINE], *emptylist[] = {NULL}; // empty : 포인터 배열

  /* Return first part of HTTP response */
  // 클라이언트에 성공을 알려주는 응답 라인을 보내는 것으로 시작
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));


//fork : 현재 프로세스의 복제본이 생성되며, 부모 프로세스와 자식 프로세스 두 개의 별도의 프로세스를 동시에 실행시키는 시스템 콜.
  if (Fork() == 0) /* Child process 생성 - 부모 프로세스(지금) 을 복사한다.*/
  {
    // 이때 부모 프로세스는 자식의 PID(Process ID)를, 자식 프로세스는 0을 반환받는다.
    // Real server would set all CGI vars here(실제 서버는 여기서 다른 CGI 환경변수도 설정)
    // QUERY_STRING 환경변수를 요청 URI의 CGI 인자들을 넣겠다. 
    // 세 번째 인자는 기존 환경 변수의 유무에 상관없이 값을 변경하겠다면 1, 아니라면 0
    setenv("QUERY_STRING", cgiargs, 1);   // 환경변수 설정

    // 문제 11.11번 수정
    // REQUEST_METHOD 환경변수를 요청 URI의 CGI 인자들로 초기화
    setenv("REQUEST_METHOD", method, 1);

    Dup2(fd, STDOUT_FILENO);              /* Redirect stdout to client, 자식 프로세스의 표준 출력을 연결 파일 식별자로 재지정 (클라이언트에게 바로 출력되게)*/
    //-> CGI 프로그램이 표준 출력으로 쓰는 모든것은 클라이언트로 바로 감(부모프로세스의 간섭 없이)
    Execve(filename, emptylist, environ); /* Run CGI program */
    // 그 후에 cgi프로그램을 로드하고 실행한다. 프로그램의 출력은 바로 클라이언트로 감.
  }
  Wait(NULL); /* Parent waits for and reaps child 부모 프로세스가 자식 프로세스가 종료될떄까지 대기하는 함수*/
}