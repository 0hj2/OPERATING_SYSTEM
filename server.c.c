#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>

#define BUF_SIZE 1024
#define MAX_CLNT 100
#define MAX_ROOM 10
#define NAME_SIZE 20
#define ROOM_NAME_SIZE 20

typedef struct {
    int sock;
    char name[NAME_SIZE];
    char room[ROOM_NAME_SIZE];
} client_t;

client_t clients[MAX_CLNT];
int clnt_cnt = 0;
pthread_mutex_t mutx;

char rooms[MAX_ROOM][ROOM_NAME_SIZE] = {"Lobby"};
int room_cnt = 1;

// 게임 관련 변수
int game_number = 0;
int game_active = 0;
char game_room[ROOM_NAME_SIZE] = "";
pthread_mutex_t game_mutx = PTHREAD_MUTEX_INITIALIZER;
int turn_count = 0;
int current_turn = 0;
int turn_socks[MAX_CLNT]; // 방 내 참여자 소켓 저장


// 함수 선언
void *handle_clnt(void *arg);
void send_msg_room(char *msg, int len, const char *room);
void error_handling(char *msg);
void strip_newline(char *s);
void add_client(client_t cl);
void remove_client(int sock);
int find_client_index(int sock);
int room_exists(const char *room);
void add_room(const char *room);
void change_client_room(int sock, const char *new_room);
char *serverState(int count);
void menu(char port[]);
void start_game(const char *room, int clnt_sock);
void handle_game_guess(int clnt_sock, const char *msg);

int main(int argc, char *argv[]){
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t clnt_adr_sz;
    pthread_t t_id;

    struct tm *t;
    time_t timer = time(NULL);
    t = localtime(&timer);

    if (argc != 2){
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    pthread_mutex_init(&mutx, NULL);
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    menu(argv[1]);

    while (1) {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);
        if (clnt_sock == -1) continue;

        pthread_mutex_lock(&mutx);
        if (clnt_cnt >= MAX_CLNT){
            pthread_mutex_unlock(&mutx);
            close(clnt_sock);
            continue;
        }
        client_t new_client;
        new_client.sock = clnt_sock;
        new_client.name[0] = 0;
        strcpy(new_client.room, "Lobby");
        add_client(new_client);
        pthread_mutex_unlock(&mutx);

        pthread_create(&t_id, NULL, handle_clnt, (void *)&clnt_sock);
        pthread_detach(t_id);

        printf("Connected client IP: %s ", inet_ntoa(clnt_adr.sin_addr));
        printf("(%d-%d-%d %d:%d)\n", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
               t->tm_hour, t->tm_min);
        printf(" chatter (%d/%d)\n", clnt_cnt, MAX_CLNT);
    }

    close(serv_sock);
    return 0;
}

void *handle_clnt(void *arg){
    int clnt_sock = *((int *)arg);
    int str_len;
    char msg[BUF_SIZE];
    char send_msg_buf[BUF_SIZE + NAME_SIZE + ROOM_NAME_SIZE];
    int idx;

    // 이름 등록 루프
    while (1){
        str_len = read(clnt_sock, msg, sizeof(msg) - 1);
        if (str_len <= 0){
            pthread_mutex_lock(&mutx);
            remove_client(clnt_sock);
            pthread_mutex_unlock(&mutx);
            close(clnt_sock);
            return NULL;
        }
        msg[str_len] = 0;
        strip_newline(msg);

        pthread_mutex_lock(&mutx);
        idx = find_client_index(clnt_sock);
        if (idx == -1){
            pthread_mutex_unlock(&mutx);
            close(clnt_sock);
            return NULL;
        }
        if (strlen(clients[idx].name) == 0){
            if (strlen(msg) < NAME_SIZE){
                strcpy(clients[idx].name, msg);
                snprintf(send_msg_buf, sizeof(send_msg_buf), "\n----[%s] joined the %s----\n\n", clients[idx].name, clients[idx].room);
                send_msg_room(send_msg_buf, strlen(send_msg_buf), clients[idx].room);
                pthread_mutex_unlock(&mutx);
                break;
            }else{
                char err_msg[] = " Name too long.\n";
                write(clnt_sock, err_msg, strlen(err_msg));
                pthread_mutex_unlock(&mutx);
                continue;
            }
        } else{
            pthread_mutex_unlock(&mutx);
            break;
        }
    }

    // 채팅 및 명령 처리 루프
    while ((str_len = read(clnt_sock, msg, sizeof(msg) - 1)) > 0){
        msg[str_len] = 0;
        strip_newline(msg);

        if (msg[0] == '/'){
            pthread_mutex_lock(&mutx);
            idx = find_client_index(clnt_sock);

            if (strncmp(msg, "/name ", 6) == 0){
                char *new_name = msg + 6;
                if (strlen(new_name) < NAME_SIZE){
                    char old_name[NAME_SIZE];
                    strcpy(old_name, clients[idx].name);
                    strcpy(clients[idx].name, new_name);
                    snprintf(send_msg_buf, sizeof(send_msg_buf), " [%s] changed name to [%s]\n", old_name, new_name);
                    send_msg_room(send_msg_buf, strlen(send_msg_buf), clients[idx].room);
                }else{
                    char err_msg[] = " Name too long.\n";
                    write(clnt_sock, err_msg, strlen(err_msg));
                }
            } 
            else if (strncmp(msg, "/create ", 8) == 0){
                char *new_room = msg + 8;
                if (strlen(new_room) < ROOM_NAME_SIZE){
                    if (room_exists(new_room)){
                        char err_msg[] = "Room already exists.\n";
                        write(clnt_sock, err_msg, strlen(err_msg));
                    }else{
                        add_room(new_room);
                        char ok_msg[BUF_SIZE];
                        snprintf(ok_msg, sizeof(ok_msg), "Room '%s' created.\n", new_room);
                        write(clnt_sock, ok_msg, strlen(ok_msg));
                    }
                }else{
                    char err_msg[] = "Room name too long.\n";
                    write(clnt_sock, err_msg, strlen(err_msg));
                }
            }
            else if (strncmp(msg, "/join ", 6) == 0){
                char *new_room = msg + 6;
                if (strlen(new_room) < ROOM_NAME_SIZE){
                    if (room_exists(new_room)){
                        char old_room[ROOM_NAME_SIZE];
                        strcpy(old_room, clients[idx].room);
                        change_client_room(clnt_sock, new_room);
                        snprintf(send_msg_buf, sizeof(send_msg_buf), "[%s] moved from %s to %s\n", clients[idx].name, old_room, new_room);
                        send_msg_room(send_msg_buf, strlen(send_msg_buf), new_room);
                    } else{
                        char err_msg[] = "Room does not exist. Join failed.\n";
                        write(clnt_sock, err_msg, strlen(err_msg));
                    }
                }else{
                    char err_msg[] = "Room name too long.\n";
                    write(clnt_sock, err_msg, strlen(err_msg));
                }
            }
            else if (strncmp(msg, "/start", 6) == 0) {
                // 게임 시작 명령어
                if (game_active){
                    char err_msg[] = "Game is already active in some room.\n";
                    write(clnt_sock, err_msg, strlen(err_msg));
                }else{
                    start_game(clients[idx].room, clnt_sock);
                }
            }else{
                char err_msg[] = " Unknown command.\n";
                write(clnt_sock, err_msg, strlen(err_msg));
            }
            pthread_mutex_unlock(&mutx);
        }else{
            // 일반 메시지인 경우, 게임이 진행 중이고 메시지가 숫자라면 게임 추측 처리
            pthread_mutex_lock(&mutx);
            idx = find_client_index(clnt_sock);
            if (game_active && strcmp(clients[idx].room, game_room) == 0){
                // 숫자 추측 시도
                handle_game_guess(clnt_sock, msg);
            }else{
                snprintf(send_msg_buf, sizeof(send_msg_buf), "[%s]: %s\n", clients[idx].name, msg);
                send_msg_room(send_msg_buf, strlen(send_msg_buf), clients[idx].room);
            }
            pthread_mutex_unlock(&mutx);
        }
    }

    pthread_mutex_lock(&mutx);
    idx = find_client_index(clnt_sock);
    if (idx != -1){
        snprintf(send_msg_buf, sizeof(send_msg_buf), "[%s] left the chat.\n", clients[idx].name);
        send_msg_room(send_msg_buf, strlen(send_msg_buf), clients[idx].room);
        remove_client(clnt_sock);
    }
    pthread_mutex_unlock(&mutx);
    close(clnt_sock);
    return NULL;
}

void send_msg_room(char *msg, int len, const char *room){
    for (int i = 0; i < clnt_cnt; i++){
        if (strcmp(clients[i].room, room) == 0){
            write(clients[i].sock, msg, len);
        }
    }
}

void add_client(client_t cl){
    if (clnt_cnt < MAX_CLNT){
        clients[clnt_cnt++] = cl;
    }
}

void remove_client(int sock){
    int i;
    for (i = 0; i < clnt_cnt; i++){
        if (clients[i].sock == sock){
            for (int j = i; j < clnt_cnt - 1; j++){
                clients[j] = clients[j + 1];
            }
            clnt_cnt--;
            break;
        }
    }
}

int find_client_index(int sock){
    for (int i = 0; i < clnt_cnt; i++){
        if (clients[i].sock == sock)
            return i;
    }
    return -1;
}

int room_exists(const char *room){
    for (int i = 0; i < room_cnt; i++){
        if (strcmp(rooms[i], room) == 0)
            return 1;
    }
    return 0;
}

void add_room(const char *room){
    if (room_cnt < MAX_ROOM){
        strcpy(rooms[room_cnt++], room);
    }
}

void change_client_room(int sock, const char *new_room){
    int idx = find_client_index(sock);
    if (idx != -1){
        strcpy(clients[idx].room, new_room);
    }
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

char *serverState(int count){
    static char stateMsg[20];
    strcpy(stateMsg, "None");

    if (count < 5)
        strcpy(stateMsg, "Good");
    else
        strcpy(stateMsg, "Bad");

    return stateMsg;
}

void menu(char port[]){
    printf(" <<<< Chat Server >>>>\n");
    printf(" Server port : %s\n", port);
    printf(" Server state : %s\n", serverState(clnt_cnt));
    printf(" Max Client : %d\n", MAX_CLNT);
    printf(" <<<< Log >>>>\n\n");
}

// 게임 시작 함수
void start_game(const char *room, int clnt_sock){
    pthread_mutex_lock(&game_mutx);
    if (game_active){
        pthread_mutex_unlock(&game_mutx);
        char msg[] = "Game is already active.\n";
        write(clnt_sock, msg, strlen(msg));
        return;
    }

    // 랜덤 숫자 생성 (1~100)
    srand((unsigned int)time(NULL));
    game_number = rand() % 100 + 1;
    game_active = 1;
    strncpy(game_room, room, ROOM_NAME_SIZE);
    game_room[ROOM_NAME_SIZE - 1] = 0;

    // 턴 초기화
    turn_count = 0;
    current_turn = 0;
    for (int i = 0; i < clnt_cnt; i++){
        if (strcmp(clients[i].room, room) == 0){
            turn_socks[turn_count++] = clients[i].sock;
        }
    }

    char start_msg[BUF_SIZE];
    snprintf(start_msg, sizeof(start_msg),
             "\n===========Game started in room '%s'.============\n Guess the number between 1 and 100.\n"
             "It's [%s]'s turn.\n",
             room, clients[find_client_index(turn_socks[current_turn])].name);
    send_msg_room(start_msg, strlen(start_msg), room);

    pthread_mutex_unlock(&game_mutx);
}

// 게임 추측 처리 함수
void handle_game_guess(int clnt_sock, const char *msg){
    pthread_mutex_lock(&game_mutx);

    if (clnt_sock != turn_socks[current_turn]){
        char err_msg[] = "!!!It's not your turn!!!\n";
        write(clnt_sock, err_msg, strlen(err_msg));
        pthread_mutex_unlock(&game_mutx);
        return;
    }

    int guess = atoi(msg);
    int idx = find_client_index(clnt_sock);
    char send_msg_buf[BUF_SIZE + NAME_SIZE + ROOM_NAME_SIZE];

    if (guess <= 0 || guess > 100){
        char err_msg[] = "Invalid guess. Please enter a number between 1 and 100.\n\n";
        write(clnt_sock, err_msg, strlen(err_msg));
        pthread_mutex_unlock(&game_mutx);
        return;
    }

    if (guess == game_number){
        // 정답 맞춤
        snprintf(send_msg_buf, sizeof(send_msg_buf),
                 "[%s] guessed the number %d correctly! Game over.\n",
                 clients[idx].name, guess);
        send_msg_room(send_msg_buf, strlen(send_msg_buf), game_room);

        game_active = 0;
        game_room[0] = 0;
        pthread_mutex_unlock(&game_mutx);
    }else{
        // 틀린 경우 메시지 전송
        snprintf(send_msg_buf, sizeof(send_msg_buf),
                 "[%s]: %d is too %s.\n\n",
                 clients[idx].name, guess,
                 guess < game_number ? "low" : "high");
        send_msg_room(send_msg_buf, strlen(send_msg_buf), game_room);

        // 다음 차례로 이동
        current_turn = (current_turn + 1) % turn_count;
        snprintf(send_msg_buf, sizeof(send_msg_buf),
                 "!!! It's now [%s]'s turn !!!\n",
                 clients[find_client_index(turn_socks[current_turn])].name);
        send_msg_room(send_msg_buf, strlen(send_msg_buf), game_room);

        pthread_mutex_unlock(&game_mutx);
    }
}