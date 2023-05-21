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
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv) {  //argc: 인자 개수, argv: 인자 배열, main 함수가 받은 각각의 인자들
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {  // 입력 인자가 2개가 아니면
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  
  listenfd = Open_listenfd(argv[1]);  // 듣기 소켓 오픈; argv[1] = 8000 (port number)
  while (1) {  // 무한 서버 루프
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept  // 연결 요청 접수; // 듣기식별자, 소켓 주소 구조체의 주소, 주소(소켓 구조체)
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);  // clientaddr의 구조체에 대응되는 hostname, port를 작성
    printf("Accepted connection from (%s, %s)\n", hostname, port);  // 어떤 client인지 알여줌
    doit(connfd);  // line:netp:tiny:doit  // transaction 수행
    Close(connfd);  // line:netp:tiny:close  // 연결 끝 닫기
  }
}

void doit(int fd) {  // http 하나의 트랜잭션을 처리
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;
  
  // Read request line and headers
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);  // 요청 라인을 rio에서 읽고 buf 에 복사해놓기
  printf("Request headers:\n");
  printf("%s", buf);  // 요청한 라인 출력
  sscanf(buf, "%s %s %s", method, uri, version);  // buf의 내용을 method, uri, version에 저장
  if (strcasecmp(method, 'GET')) {  // TINY는 GET 메소드만 지원하고, POST 같은 다른 메소드가 요청되면 
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");  // 이 에러 메시지를 띄운후에
    return;  // main 루틴으로 돌아오고 나서 연결을 닫고 다음 연결 요청을 기다린다.
  }
  read_requesthdrs(&rio);  // 다른 요청 헤더 무시
  
  // Parse URI from GET request
  is_static = parse_uri(uri, filename, cgiargs));  // 정적 혹은 동적 콘텐츠를 위한 것인지를 나타내는 플래그 설정
  if (stat(filename, &sbuf) < 0) {  // 그 파일이 디스크 상에 없으면
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");  // 에러 메시지를 클라이언트에게 보내고
    return;  // 리턴한다.
  }
  
  if (is_static) {  // Serve static content  // 정적 콘텐츠를 위한 것이라면
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {  // 그 파일이 보통 파일인지와 읽기 권한이 있는지 확인
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size);  // 그렇다면  정적 콘텐츠를 클라이언트에게 제공
  }
  else {  // Serve dynamic content  // 동적 콘텐츠를 위한 것이라면
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {  // 파일이 실행 가능한지 확인
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);  // 그렇다면 동적 컨텐츠를 클라이언트에게 제공
  }
}

void clienterror(int fd, cjhar *cause, char *errnum, char *shormsg, char *longmsg) {  // 에러를 클라이언트에게 보고
  char buf[MAXLINE], body[MAXBUF];
  
  // Build the HTTP response body
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);
  
  // Print the HTTP response
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp) {  // 요청 헤더를 읽기만 하고 무시
  char buf[MAXLINE];
  
  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs) {
  char *ptr;
  
  if (!strstr(uri, "cgi-bin")) {  // 요청이 정적 컨텐츠를 위한 것이라면
    strcpy(cgiargs, "");  // CGI 인자 스트링을 지우고
    strcpy(filename, ".");  // URI를 ./index.html 같은 
    strcat(filename, uri);  // 상대 리눅스 경로이름으로 바꾼다.
    if (uri[strlen(uri) - 1] == '/')  // URI가 '/'로 끝나면
      strcat(filename, "home.html");  // 기본 파일 이름(home.html) 추가
    return 1;
  }
  else {  // Dynamic content  // 요청이 동적 컨텐츠를 위한 것이라면
    ptr = index(uri, '?');
    if (ptr) {
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    }
    else
      strcpy(cgiargs, "");  // 모든 CGI 인자를 추출하고
    strcpy(filename, ".");  // 나머지 URI 부분을
    strcat(filename, uri);  // 상대 리눅스 파일 이름으로 바꾼다.
    
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize) {
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];
  
  // Send response headers to client
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 0K\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  print("Response headers:\n");
  print("%s", buf);
  
  // Send response body to client
  srcfd = Open(filename, O_RDONLY, 0);
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  Close(srcfd);
  Rio_writen(fd, srcp, filesize);
  Munmap(srcp, filesize);
}

/*
 * get_filetype - Derive file type from filename
 */
void get_filetype(char *filename, char *filetype) {
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif")))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png")))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg")))
    strcpy(filetype, "image/jpg");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs) {
  char buf[MAXLINE], *emptylist[] = { NULL };
  
  // Return first part of HTTP response
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));
  
  if (Fork() == 0) {  // Child
    // Real server would set all CGI vars here
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO);  // Redirect stdout to client
    Execve(filename, emptylist, environ);  // Run CGI program
  }
  Wait(NULL);  // Parent waits for and reaps child
}
