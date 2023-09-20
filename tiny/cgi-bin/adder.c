/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void)
{
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 35;

  /* Extract the two arguments */
  if ((buf = getenv("QUERY_STRING")) != NULL)
  {
  //예시 : buf => first=123&second=234 HTTP/1.1

    // p = strchr(buf, '&'); //문자열 buf 내에 '&' 문자가 있는지 검사.
    //*p = '\0'; // &를 \0으로 바꿔서 이 문자열의 끝을 정의함
    // strcpy(arg1, buf); // & 앞 문자열을 arg1 으로 복사 => first=123
    // strcpy(arg2, p + 1); // & 뒤 문자열을 arg2 로 복사 => second=234 HTTP/1.1
    // //atoi 는 앞에서부터 숫자를 읽다가 문자열이 나오면 거기서 끊어버리고 앞 숫자를 int형으로 변환
    // n1 = atoi(strchr(arg1, '=') + 1); // 123 (정수형)
    // n2 = atoi(strchr(arg2, '=') + 1); // 234 HTTP/1.1 => 234 (정수형)
    
    sscanf(buf,"input1=%d",&n1);
    sscanf(buf,"input2=%d",&n2);
  }

  /* Make the response body */
  // content 인자에 html body 를 담는다.
  sprintf(content, "<p>QUERY_STRING=%s</p>", buf);
  // sprintf(content, "%s<p>REQUEST_METHOD=%s</p>", content, getenv("REQUEST_METHOD"));
  sprintf(content, "%sWelcome to add.com: ",content);
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>",
          content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  /* Generate the HTTP response */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n"); // \r\n 때문에 여기까지가 헤더
  // 여기까지 가 헤더

  // 여기부터 바디 출력
  printf("%s", content);
  fflush(stdout); //출력 버퍼 안에 존재하는 데이터를 비우는 즉시 출력한다.(어차피 \n이라 상관 x)

  exit(0);
}
/* $end adder */