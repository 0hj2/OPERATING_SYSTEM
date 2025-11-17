24-2 운영체제 기말 프로젝트

# 🗨️ Multi-Room Chatting Program with Turn-Based Up-Down Game  
> C 기반 Socket Programming / Multi-threading / Chat Server & Client  

이 프로젝트는 **TCP 소켓 통신 기반의 다중 채팅 서버**와 **콘솔 클라이언트 프로그램**입니다.  
기본적인 채팅 기능뿐 아니라, 사용자가 명령어로 채팅방을 생성·이동할 수 있으며, 특정 방에서만 진행되는 **턴 기반 Up-Down 게임** 기능을 제공합니다.

---

## 📌 프로젝트 개요
- **개발 기간**: 개인 프로젝트  
- **사용 언어**: C  
- **기술 스택**  
  - TCP/IP Socket Programming  
  - Multi-threading (pthread)  
  - I/O multiplexing 구조  
  - ANSI C 기반 네트워크 프로그래밍  

---

## 🚀 주요 기능 요약

### ✔ 1. 채팅 서버 기능
- 다중 클라이언트 접속 관리 (최대 100명)
- "Lobby" 포함 최대 10개의 채팅방(Room) 관리
- 클라이언트 이름 등록 및 변경
- 방 생성(`/create`), 방 이동(`/join`)
- 서버에서 방 내부 유저에게 메시지 브로드캐스트  
- 클라이언트 종료 및 접속 종료 자동 처리

---

### ✔ 2. 클라이언트 기능
- 접속 시 사용자 이름 송신
- 서버 시간, 클라이언트 IP, 포트 정보 표시
- 명령어 기반 UI 제공  

#### ▶ 지원 명령어
| 명령어 | 설명 |
|-------|------|
| `/create <room>` | 새로운 채팅방 생성 |
| `/join <room>` | 특정 채팅방으로 이동 |
| `/name <name>` | 사용자의 닉네임 변경 |
| `/start` | Up-Down 게임 시작 |
| `q` | 프로그램 종료 |

---

## 🎮 Up-Down 게임 기능 (핵심)
채팅 기능과 별개로 **특정 방에서만 실행되는 턴 기반 게임**을 구현했습니다.

### 게임 규칙
- 서버가 1~100 사이 난수를 생성
- 방의 모든 유저가 순서대로 하나씩 숫자를 추측
- 추측값이 정답보다 크면 “high”, 작으면 “low” 출력
- 정답을 맞히면 해당 방 전체에 알림 후 게임 종료

### 기술적 포인트
- **Mutex**를 활용해 하나의 방에서만 게임이 진행되도록 제어
- `turn_socks[]` 배열에 참여자 리스트 저장 후 턴 관리  
- 입력 메시지가 숫자인지 판별하여 게임 입력인지 구분  
- 게임 진행 중에도 일반 메시지는 정상 처리  

---

## 🛠 서버 구조 상세

### 1) 클라이언트 관리 
typedef struct {
    int sock;
    char name[NAME_SIZE];
    char room[ROOM_NAME_SIZE];
} client_t;

### 2) 주요 서버 스레드
✔ handle_clnt()
- 클라이언트 메시지 수신
- 명령어 처리 (/create, /join, /name, /start 등)
- 게임 입력 처리

✔ send_msg_room()
- 특정 방(Room) 내 모든 클라이언트에게 메시지 브로드캐스트

### 3) 게임 관리 로직
✔ start_game()
- 방에서 게임 시작
- 난수 생성
- 참여자 턴 초기화

✔ handle_game_guess()
- 유저의 추측값 처리
- high/low 판단
- 정답 시 종료 및 알림
- 턴 이동 처리
