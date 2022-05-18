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
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

// Echo server의 메인 루틴: 듣기 식별자를 오픈한 후에 무한 루프에 진입한다.
// 각각의 반복실행은 클라이언트로부터 연결 요청을 기다리며, 도메인이름과 연결된 클라이언트의 포트를 출력하고
// 클라이언트를 서비스하는 에코 함수를 호출한다. 에코 루틴이 리턴한 후에 메인 루틴은 연결 식별자를 닫아준다. 
// 클라이언트와 서버가 자신들의 식별자를 닫은 후에 연결은 종료된다. 

int main(int argc, char **argv) {//argumentcount
  int listenfd, connfd; //듣기소켓, 연결소켓 초기화 
  char hostname[MAXLINE], port[MAXLINE]; 
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;  //socket address structure passed to accept

  /* Check command line args */
  
  if (argc != 2) {//argv를 두개를 받는지 확인, argv[0]=파일명+argv[1]=포트
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); // opening listening socket by calling the open_listenfd function 
  while (1) { 
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,  //execute infinite server loop, accepting a connection request 
                    &clientlen);  // line:netp:tiny:accept
    
    //client 구조체에서 정보를 빼내서 hostname, port 저장함 
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit // perform transaction 
    Close(connfd);  // line:netp:tiny:close //close at the end of the connection //close- 기본함수를 wrapping함
  }
}
// 한 개의 HTTP 트랜잭션을 처리한다. 요청 라인을 읽고 분석한다
void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];//buf: 
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /*   Read request line and headers*/ 
  Rio_readinitb(&rio, fd);//rio주소와 fd 연결

  //요청 라인을 읽고 분석한다. 
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n"); 
  printf("%s", buf); 

  //****처음에 소통을 여는게 세개값을 주면서 소통을 연다 method, uri, version
  //sscanf: buf 값을 %s%s%s로 읽어서 method uri version 저장한다.
  sscanf(buf, "%s %s %s", method, uri, version);

  if (strcasecmp(method, "GET")){ //strcasecmp: 대소문자를 무시하는 문자열 비교 함수, get아니면 양수가 나와서 true값 실행, 같으면 0이나와서 False 
    clienterror(fd, method, "501", "Not implemented",
    "Tiny does not implement this method");
    return;
  }
  ////
  read_requesthdrs(&rio);//그렇지 않으면 읽어들이고 다른 요청 헤더들을 무시한다 

  /* Parse URI from GET request */
  //0 혹은 1반환 
  is_static = parse_uri(uri, filename, cgiargs);
  if(stat(filename, &sbuf)< 0) {
    clienterror(fd, filename, "404","Not found", "Tiny couldn't find this file");
    return;
  }
  if (is_static) { /* Serve static content  */
  // S_ISREG -> isregular - 일반 파일인지 체크하는 macro 
  //st_mode는 파일의 유형값으로 bit& 파일의 유형을 확인 가능함
  //S_IXUSR ->실행권한이 있는지/ S_IRUSR ->읽기 권한이 있는지 
  //sbuf의 st_mode를 확인하는 것 
  //
    if (!(S_ISREG(sbuf.st_mode))|| !(S_IRUSR & sbuf.st_mode)){//regular 파일이고 read permission이 있어야만 serve_static으로 넘어감 //
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    // fd 목적값(fd에 계속 담아서 주고받음), 읽을값이 filename, sbuf.st_size- 사이즈, 
    //사이즈만큼 filename 읽어와서 fd담는다 
    serve_static(fd, filename, sbuf.st_size);//serve static content
  }
  else { /* Serve dynamic content */ 
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs); //serve dynamic static content
  }
}

/* 에러 메세지를 클라이언트에게 보낸다. */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];
 //sprintf: body에 담아줌 
/*   Build the HTTP response body*/
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"")\r\n",body);
  sprintf(body, "%s%s: %s\r\n", body, errnum,shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body,longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

/* Print the HTTP response *///client가 받고 있는 것 
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));//버프의 길이만큼 fd담는 것 
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, body, strlen(buf));
  Rio_writen(fd, body, strlen(body));//fd를 쏘는거고, 우리가 그걸 다 볼 수 있는 거고 
  }
  
/*   요청 헤더를 읽고(프린트) 무시한다. 실제 서버는 읽어서 처리하지*/
//우리가 만드는 웹서버는 요청 헤더 내의 아무런 정보도 사용하지 않는다. 요청 헤더를 종료하는 빈 텍스트줄("\r\n")이 나올 때까지 요청 헤더를 모두 읽어들인다.
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];
//maxline만큼 buf를 rp에 담는다
  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")){ //요청 헤더를 종료하는 빈 텍슽줄이 나올 때까지 
    Rio_readlineb(rp, buf, MAXLINE); //읽어들여라
    printf("%s", buf);
  }
  return;
} 

// uri를 도메인, 파일명, cgiargs로 나눈다. cgiargs란 동적 컨텐츠의 실행파일에 들어갈 인자다. 
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;
//strstr(string1, string2) 함수는 string1에서 string2의 첫 번째 표시 시작 위치에 대한 포인터를 리턴한다. 
  if(!strstr(uri, "cgi-bin")){/*  Static content */ //정적 컨텐츠를 위한 것이라면//strstr: uri에서 "cgi-bin"의 첫번째 표시 시작 위치 찾음
  //The strcpy(destination,source) function copies the string pointed by source (including the null character) to the destination.
  strcpy(cgiargs, ""); //uri에 cgi-bin과 일치하는 문자열이 없다면 cgiargs에는 빈 문자열을 저장
  strcpy(filename, ".");//상대경로 넣으니까 ./ 지금 내위치에서 부터 찾을꺼야
  strcat(filename, uri); //아래 줄과 더불어 상대 리눅스 경로이름으로 변환(./index.html과 같은)

  if(uri[strlen(uri)-1] == '/')//uri가 '/'문자로 끝난다면 
    strcat(filename, "home.html"); //기본 파일 이름을 추가한다. //strcat: filename 뒤에 home.html을 붙이는 함수//filename뒤에 \0이 사라지고 그 위치에 바로 home.html이 붙음
    return 1;
}
else { /* Dynamic content  */ //동적 컨텐츠를 위한거라면 
//모든 인자를 추출하고 
  ptr = index(uri, '?');
  if(ptr){
    strcpy(cgiargs, ptr+1);
    *ptr = '\0';
  }
  else 
    strcpy(cgiargs, "");
//
//나머지 uri부분을 상대 리눅스 파일 이름으로 변환한다. 
  strcpy(filename, ".");
  strcat(filename, uri);
//
  return 0;
  }
}

// 서버의 디스크 파일을 정적 컨텐츠라고 하며, 디스크 파일을 가져와 클라이언트에게 전달하는 작업을 정적 컨텐츠를 처리한다고 말한다. 
//컨텐츠를 보내기 전에 어떤 컨텐츠를 보낼지, 어느 정도 크기의 컨텐츠를 보낼지를 포함한 response header을 보낸다. 
void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

/*   Send response headers to client */
  get_filetype(filename, filetype); //파일 이름의 접미어 부분을 검사해서 파일 타입을 결정

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype); //빈줄 한 개가 헤더를 종료
  Rio_writen(fd, buf, strlen(buf));   //클라이언트에 response line과 response header을 보낸다
 
  printf("Response headers:\n");
  printf("%s",buf);//console에 확인하는 것 

/*   Send response body to client  */
//O_RDONLY - > 파일을 읽기 전용으로 열기 <-> O_WRONLY, 둘 합치면 O_RDWR
//0 - > mode 값
  srcfd = Open(filename, O_RDONLY, 0);//읽기 위해서 filename을 오픈하고 식별자를 얻어온다, 파일의 식별자가 srcfd에 들어감

  //리눅스 mmap함수는 요청한 파일을 가상메모리 영역으로 매핑한다. 
  //mmap을 호출하면 파일 srcfd의 첫 번째 filesize 바이트를 주소 srcp에서 시작하는 사적 읽기-허용 가상메모리 영역으로 매핑한다. 
  //srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);               /* //Mmap */
  srcp = Malloc(filesize);               /* //Malloc */
  Rio_readn(srcfd, srcp, filesize);               /* //Malloc */
  //파일을 메모리로 매핑한 후에 더 이상 이 식별자는 필요없으며, 이 파일을 닫는다.
  Close(srcfd);

//파일을 클라이언트에게 전송 
//주소 srcp에서 시작하는 filesize 바이트(파일에 매핑되어있음)를 클라이언트의 연결 식별자로 복사한다
  Rio_writen(fd, srcp, filesize);

  //매핑된 가상메모리 주소를 반환한다. (메모리 누수를 피하는 데 중요)
  // Munmap(srcp, filesize);               /* //Mmap */
  Free(srcp);;               /* //Malloc */
}

/* get_filetype - Derive file type from filename */
//response header에 들어갈 내용인 클라이언트가 요청한 파일의 타입을 확인한다. 
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename,".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");  
  else
    strcpy(filetype, "text/plain");  
}

//동적 컨텐츠를 처리하는 함수
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = { NULL };

/*   Return first part of HTTP response*/  
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));//클라이언트에 성공을 알려주는 응답 라인을 보내는 것으로 시작
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));
//
//현재 프로세스에서는 새로운 프로세스를 실행할 수 없으니까, 자식 프로세스를 fork한다, 따른 main 함수를 실행할 수 없으니까
//새로운 자식 프로세스를 fork한다. 부모의 메모리를 물려받는 자식 프로세스를 만들고 새로운 프로세스를 실행할 준비를 한다.
  if(Fork() == 0){ /* Child */
/*     Real server would set all CGI vars here */

// 자식은 QUERY_STRING 환경변수를 요청 URI의 CGI 인자들로 초기화한다. 
//실제 서버는 여기서 다른 CGI 환경변수들도 마찬가지로 설정한다. 
//클라이언트 표준출력을 CGI 표준출력과 프로그램에 연결한다. 
    setenv("QUERY_STRING", cgiargs, 1);

//표준출력 값이 fd로 가게끔 
//자식은 자식의 표준 출력을 연결 파일 식별자로 재지정하고     
    Dup2(fd, STDOUT_FILENO);           /* Redirect stdout to client  */

//CGI 프로그램을 로드하고 실행한다. 
// CGI 프로그램이 자식 컨텍스트에서 실행되기 때문에 execve함수를 호출하기 전에 존재하던 열린 파일들과
// 환경변수들에도 동일하게 접근할 수 있다. 그래서 CGI 프로그램이 표준 출력에 쓴느 모든 것은 직접
// 클라이언트 프로세스로 부모 프로세스의 어떤 간섭도 없이 전달된다. 
//또다른 main을 실행하는 건데, emptylist 빈값을 주겠다 , environ= 환경변수

    Execve(filename, emptylist, environ);           /* Run CGI program */
  }

  //부모는 자식이 종료되어 정리되는 것을 기다리기 위해 wait 함수에서 블록된다. 
  Wait(NULL);           /* Parent waits for and reaps child */
}
