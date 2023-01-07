/*
    TODO:
    - poprawić sposób wpisywania treści do programu (scanf() nie radzi sobie ze spacjami, a także sprawdzać poprawność wiadomości)
    - poprawić komunikację klient-serwer tak, aby proces klienta nie wieszał się w oczekiwaniu na wiadomość od serwera (Jeżeli minie 5s. bez odpowiedzi to klient o tym poinformuje i będzie kontynuował działanie)
*/

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>

#define MESSAGE_SIZE 1088
#define NICKNAME_SIZE 20
#define LONGER_MESSAGE_SIZE 4160

struct message{
        long receiver;
        long msgType;
        long number;
        char text[1024];
};

struct messageLonger{
    long receiver;
    long msgType;
    long pid;
    char text[4096];
};

void showMenu();    // Wyświetla menu z wyborem możliwych akcji
int isMessageValid(char* text); // funkcja sprawdza, czy podana wiadomość spełnia ustalone kryteria

int messageLogin(int mid, char* nickname);
int messageLogout(int mid);
int messageShowClients(int mid);
int messageShowGroupClients(int mid, char* groupName);
int messageJoinGroup(int mid, char* groupName);
int messageLeaveGroup(int mid, char* groupName);
int messageShowGroups(int mid);
int messageSendMessageToGroup(int mid, char* text);
int messageSendMessageToClient(int mid, char* text);
int messageReceiveMessages(int mid);
int sendMessage(long msgType, char* text);

int sid;
int main(){
    int mid = msgget(0x1234, 0660);
    printf("mid = %d\n",mid);
    sid = mid;

    struct message msg;
    struct messageLonger msgLonger;

    // Kod poniżej odpowiedzialny jest za logowanie klienta do serwera
    char nazwa[1024];
    int attemps = 3;
    while(1){
        printf("Podaj swoja nazwe: ");
        scanf("%s",nazwa);
        int res = sendMessage(1,nazwa);
        if(res == -1){
            printf("Serwer jest przeciazony lub nie odpowiada. Sprobuj pozniej.\n");
        }else{
            system("clear");
            msgrcv(mid, &msg, MESSAGE_SIZE, getpid(), 0);
            printf("%s\n", msg.text);
            if(msg.number==-1){
                printf("Pozostale proby: %d\n",--attemps);
                if(attemps==0){
                    printf("Zbyt wiele prob logowania.\n");
                    return 0;
                }
            }
            if(msg.number!=-1)
                break;
        }
    }

    // Po udanym logowaniu następuje wejście w nieskończoną pętlę komunikacji z serwerem
    showMenu();
    while(1){
        int wybor=0;
        fflush(stdin);
        char term;
        if(scanf("%d%c", &wybor, &term) != 2 || term != '\n'){
            getchar();
            continue;
        }
        switch(wybor){
            case 1:{
                sendMessage(2,"");
                break;
                }
            case 2:{
                sendMessage(3,"");
                break;
            }
            case 3:{
                char groupName[1024];
                printf("Podaj nazwe grupy: ");
                scanf("%s",groupName);
                if(isMessageValid(groupName)==-1)
                    continue;
                sendMessage(4,groupName);
                break;
            }
            case 4:{
                char groupName[1024];
                printf("Podaj nazwe grupy: ");
                scanf("%s",groupName);
                if(isMessageValid(groupName)==-1)
                    continue;
                sendMessage(5,groupName);
                break;
            }
            case 5:{
                char groupName[1024];
                printf("Podaj nazwe grupy: ");
                scanf("%s",groupName);
                if(isMessageValid(groupName)==-1)
                    continue;
                sendMessage(6,groupName);
                break;
            }
            case 6:{
                sendMessage(7,"");
                break;               
            }
            case 7:{
                char text[1024];
                char groupName[1024];
                printf("Podaj nazwe grupy: ");
                scanf("%s",groupName);
                strcpy(text,groupName);
                
                printf("Podaj tresc wiadomosci: ");
                fflush(stdin);
                getchar();
                fgets(groupName, sizeof groupName, stdin);
                strcat(text," ");
                strcat(text,groupName);
                if(isMessageValid(text)==-1)
                    continue;
                sendMessage(8,text);
                break;
            }
            case 8:{
                char text[1024];
                char name[1024];
                printf("Podaj nazwe uzytkownika: ");
                scanf("%s",name);
                strcpy(text,name);
                
                printf("Podaj tresc wiadomosci: ");
                fflush(stdin);
                getchar();
                fgets(name, sizeof name, stdin);
                strcat(text," ");
                strcat(text,name);
                if(isMessageValid(text)==-1)
                    continue;
                sendMessage(9,text);
                break;
            }
            case 9:{
                sendMessage(10,"");
                break;
            }
            case 10:{
                sendMessage(11,"");
                break;
            }
            case 11:{
                char name[1024];
                printf("Podaj nazwe uzytkownika: ");
                scanf("%s",name);
                if(isMessageValid(name)==-1)
                    continue;
                sendMessage(12,name);
                break;
            }
            case 12:{
                char name[1024];
                printf("Podaj nazwe uzytkownika: ");
                scanf("%s",name);
                if(isMessageValid(name)==-1)
                    continue;
                sendMessage(13,name);
                break;
            }
            default: {
                printf("zly wybor\n");
                continue;}
        };

        if(wybor==9)
            msgrcv(mid, &msgLonger, LONGER_MESSAGE_SIZE, getpid(),0);
        else
            msgrcv(mid, &msg, MESSAGE_SIZE, getpid(),0);

        system("clear");
        if(wybor==9)
            printf("\n%s\n",msgLonger.text);
        else
            printf("\n%s\n",msg.text);

        if(msg.msgType==2){
                return 0;
        }
        showMenu();
    }
    return 0;
}

int isMessageValid(char* text){
    int i = 0;
    long length = strlen(text);
    if(length>=1000) return -1;  // Sprawdzenie, czy podana wiadomość ma poprawną długość
    // Sprawdzenie, czy podana wiadomość składa się z odpowiednich znaków
    for(i=0;i<length;i++){
        if(!isascii(text[i]))
            return -1;
    }
    return 0;
}
void showMenu(){
    printf("1. wyloguj sie\n");
    printf("2. podglad listy uzytkownikow\n");
    printf("3. podglad grupy\n");
    printf("4. zapisanie do grupy\n");
    printf("5. wypisanie sie z grupy\n");
    printf("6. podglad listy dostepnych grup\n");
    printf("7. wyslanie wiadomosci do grupy\n");
    printf("8. wyslanie wiadomosci do uzytkownika\n");
    printf("9. przeczytaj otrzymane wiadomosci\n");
    printf("10. podglad listy zablokowanych uzytkownikow\n");
    printf("11. zablokuj uzytkownika\n");
    printf("12. odblokuj uzytkownika\n");

    printf("Wybierz numer wiadomosci: \n");
}

int sendMessage(long msgType, char* text){
    struct message fmsg;
    fmsg.receiver = 1;
    fmsg.msgType = msgType;
    fmsg.number = getpid();
    strcpy(fmsg.text,text);
    return msgsnd(sid, &fmsg, MESSAGE_SIZE, 0);
}