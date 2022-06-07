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
#include <err.h>
#define SIZE 8192


void die(char* s) {perror(s); exit(1);}

int main(int argc, char * argv[]) {
    if(argc != 3) printf("three argument is needed");
    char* ip_addr = (char*)malloc(sizeof(char)*100);
    if (ip_addr == NULL) {
        die("ip_addr");
    }
    ip_addr = argv[1];
    int port = atoi(argv[2]);

    int s = socket(PF_INET, SOCK_STREAM, 0);
    if(s == -1) die("socket");

    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    if(inet_aton(ip_addr, &addr.sin_addr) == 0) die("inet_aton");
    
    addr.sin_port = htons(port);
    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) == -1) die("connect");

    FILE *fp = NULL; 
	char	*cmdline = "rec -t raw -b 16 -c 1 -e s -r 44100 -";
	if ( (fp=popen(cmdline,"r")) ==NULL) {
		err(EXIT_FAILURE, "%s", cmdline);
	}

    while(1) {
        unsigned char buf[SIZE] = {0};
        unsigned char buf2[SIZE] = {0};
        int n = fread(buf,sizeof(unsigned char),SIZE, fp);

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

}