#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>

#define BUF_SIZE 100
#define NORMAL_SIZE 20

void *send_msg(void *arg);
void *recv_msg(void *arg);
void error_handling(char *msg);
void strip_newline(char *s);
void menu();

char name[NORMAL_SIZE] = "[Default]";
char msg[BUF_SIZE];
int sock;

char serv_port[10];
char clnt_ip[20];
char serv_time[20];

int main(int argc, char *argv[]){
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t adr_sz;
    pthread_t snd_thread, rcv_thread;

    if (argc != 4){
        printf("Usage : %s <IP> <port> <name>\n", argv[0]);
        exit(1);
    }

    struct tm *t;
    time_t timer = time(NULL);
    t = localtime(&timer);
    sprintf(serv_time, "%d-%d-%d %d:%d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min);
    sprintf(name, "%s", argv[3]);
    sprintf(serv_port, "%s", argv[2]);

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_adr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("connect() error!");

    // 클라이언트 IP 정보 얻기
    adr_sz = sizeof(clnt_adr);
    getsockname(sock, (struct sockaddr *)&clnt_adr, &adr_sz);
    sprintf(clnt_ip, "%s", inet_ntoa(clnt_adr.sin_addr));

    // 접속 후 이름 전송
    char name_msg[NORMAL_SIZE + 10];
    sprintf(name_msg, "%s\n", name);
    write(sock, name_msg, strlen(name_msg));

    menu();

    pthread_create(&snd_thread, NULL, send_msg, NULL);
    pthread_create(&rcv_thread, NULL, recv_msg, NULL);
    pthread_detach(snd_thread);
    pthread_detach(rcv_thread);

    while (1){
        sleep(1);
    }

    close(sock);
    return 0;
}

void *send_msg(void *arg){
    char send_buf[BUF_SIZE];
    while (1){
        fgets(send_buf, sizeof(send_buf), stdin);
        strip_newline(send_buf);

        if (!strcmp(send_buf, "q") || !strcmp(send_buf, "Q")){
            close(sock);
            exit(0);
        }

        write(sock, send_buf, strlen(send_buf));
    }
    return NULL;
}

void *recv_msg(void *arg){
    int str_len;
    while (1){
        str_len = read(sock, msg, sizeof(msg) - 1);
        if (str_len <= 0){
            return NULL;
        }
        msg[str_len] = 0;
        printf("%s", msg);
    }
    return NULL;
}

void strip_newline(char *s){
    char *p = strchr(s, '\n');
    if (p)
        *p = 0;
}

void error_handling(char *msg){
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}
void menu(){
    printf(" <<<< Chat Client >>>>\n");
    printf(" Server Port : %s \n", serv_port);
    printf(" Client IP   : %s \n", clnt_ip);
    printf(" Chat Name   : %s \n", name);
    printf(" Server Time : %s \n", serv_time);
    printf(" ============= Mode =============\n");
    printf(" /create <room> : create new room\n");
    printf(" /join <room>   : join/move room\n");
    printf(" /name <name>   : change name\n");
    printf(" /start         : start up and down game\n");
    printf(" =================================\n");
    printf(" Exit -> q\n\n");
}