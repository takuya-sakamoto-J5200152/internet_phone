#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netinet/ip.h>
#include<netinet/tcp.h>
#include<errno.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<string.h>
#include<stdio.h>
#include <err.h>
#define SIZE 8192

void die(char* s) {perror(s); exit(1);}

int main(int argc, char * argv[]) {
    if(argc != 2) {
        printf("two argument is needed");
        return -1;
    }

    int port = atoi(argv[1]);

    int ss = socket(PF_INET, SOCK_STREAM, 0);
    if(ss == -1) die("socket");

    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if(bind(ss,(struct sockaddr *)&addr, sizeof(addr)) == -1) die("bind");

    if(listen(ss,10) == -1) die("listen");

    struct sockaddr_in client_addr;
    socklen_t len = sizeof(struct sockaddr_in);
    int s = accept(ss,(struct sockaddr*)&client_addr,&len);
    if (s == -1) die("accept");

    FILE *fp = NULL; 
	char	*cmdline = "rec -t raw -b 16 -c 1 -e s -r 44100 -";
	if ( (fp=popen(cmdline,"r")) ==NULL) {
		err(EXIT_FAILURE, "%s", cmdline);
	}
    
    while(1) {
        unsigned char buf[SIZE] = {0};
        unsigned char buf2[SIZE] = {0};
        int n = fread(buf,sizeof(unsigned char),SIZE,fp);

        int set = send(s,buf,n,0);
        if(set== -1) die("send");
        if(set== 0) {shutdown(s, SHUT_WR); break;}

        int ret = recv(s, buf2, SIZE, 0);
        if (ret == -1) die("recv");    
        else if (ret != 0) write(1,buf2,ret);
        else {
            shutdown(s, SHUT_WR);
            break;
        }
	}
	(void) pclose(fp);
    close(ss);

}