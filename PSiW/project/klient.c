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

int MESSAGE_SIZE = 1088;

struct message{
        long receiver;
        long msgType;
        /*
        możliwe typy wiadomości:
        1. zalogowanie
        2. wylogowanie
        3. podglad listy uzytkownikow
        4. podglad grupy (to samo co 3?)

        5. zapisanie do grupy
        6. wypisanie się z grupy
        7. podgląd listy dostępnych grup

        8. wysłanie wiadomości do grupy
        9. wysłanie wiadomości do użytkownika
        10. odebranie wiadomości
        */
        long number;
        char text[1024];
    };


int isMessageValid(char* text);

int messageLogin(int mid, char* nickname);
int messageLogout(int mid);
int messageShowClients(int mid);
int messageShowGroupClients(int mid, char* groupName);

int messageJoinGroup(int mid, char* groupName);
int messageLeaveGroup(int mid, char* groupName);
int messageShowGroups(int mid);

int messageSendMessageToGroup(int mid, char* text);
int messageSendMessageToClient(int mid, char* text);
int messageReceiveMessage(int mid);


int main(){
    int mid = msgget(0x1234, 0660 | IPC_CREAT);
    printf("mid = %d\n",mid);

    struct message msg;

    char nazwa[1024];
    while(1){
        printf("Podaj swoja nazwe: ");
        scanf("%s",nazwa);
        int res = messageLogin(mid,nazwa);
        if(res == -1){
            printf("Serwer jest przeciazony lub nie odpowiada. Sprobuj pozniej.\n");
        }else{
            msgrcv(mid, &msg, MESSAGE_SIZE, getpid(), 0);
            ///printf("%s\n", msg.text);
            if(msg.number!=-1)
                break;
        }
    }

    if(fork()==0){
        while(1){
            if((msg.msgType==8)||(msg.msgType==9)){
                printf("\n%s\n",msg.text);
            }else{
                system("clear");
                printf("\n%s\n",msg.text);
                printf("1. Wyloguj sie\n");
                printf("2. podglad listy uzytkownikow\n");
                printf("3. podglad grupy\n");
                printf("4. zapisanie do grupy\n");
                printf("5. wypisanie sie z grupy\n");
                printf("6. podglad listy dostepnych grup\n");
                printf("7. wyslanie wiadomosci do grupy\n");
                printf("8. wyslanie wiadomosci do uzytkownika\n");
                printf("Wybierz numer wiadomosci: \n");
            }

            msgrcv(mid, &msg, MESSAGE_SIZE, getppid(),0);
            if(msg.msgType==2){
                system("clear");
                printf("\n%s\n",msg.text);
                kill(getppid(), SIGKILL);
                exit(0);
            }

        }
    }

    while(1){
        int wybor;
        fflush(stdin);
        scanf("%d",&wybor);
        switch(wybor){
            case 1:{
                messageLogout(mid);
                break;
                }
            case 2:{
                messageShowClients(mid);
                break;
            }
            case 3:{
                char groupName[1024];
                printf("Podaj nazwe grupy: ");
                scanf("%s",groupName);
                messageShowGroupClients(mid,groupName);
                break;
            }
            case 4:{
                char groupName[1024];
                printf("Podaj nazwe grupy: ");
                scanf("%s",groupName);
                messageJoinGroup(mid,groupName);
                //msgrcv(mid, &msg, MESSAGE_SIZE, getpid(),0);
                //printf("%s",msg.text);
                break;
            }
            case 5:{
                char groupName[1024];
                printf("Podaj nazwe grupy: ");
                scanf("%s",groupName);
                messageLeaveGroup(mid,groupName);
                //msgrcv(mid, &msg, MESSAGE_SIZE, getpid(),0);
                //printf("%s",msg.text);
                break;
            }
            case 6:{
                messageShowGroups(mid);
                //msgrcv(mid, &msg, MESSAGE_SIZE, getpid(),0);
                //printf("%s",msg.text);
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
                scanf("%s",groupName);
                strcat(text," ");
                strcat(text,groupName);
                
                messageSendMessageToGroup(mid,text);

                //msgrcv(mid, &msg, MESSAGE_SIZE, getpid(),0);
                //printf("%s",msg.text);
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
                scanf("%s",name);
                strcat(text," ");
                strcat(text,name);
                messageSendMessageToClient(mid,text);

                //msgrcv(mid, &msg, MESSAGE_SIZE, getpid(),0);
                //printf("%s",msg.text);
                break;
            }
            
            default: {printf("zly wybor");}
        };


    }
    return 0;
}


int isMessageValid(char* text){
    int i = 0;
    long length = strlen(text);
    if(length>=512) return -1;
    for(i=0;i<length;i++){
        if(!isascii(text[i]))
            return -1;
    }

    return 0;
}

int messageLogin(int mid, char* nickname){
    struct message fmsg;
    fmsg.receiver = 1;
    fmsg.msgType = 1;
    fmsg.number = getpid();
    strcpy(fmsg.text,nickname);
    return msgsnd(mid, &fmsg, MESSAGE_SIZE, IPC_NOWAIT); 
}
int messageLogout(int mid){
    struct message fmsg;
    fmsg.receiver =1;
    fmsg.msgType =2;
    fmsg.number= getpid();
    strcpy(fmsg.text,"");
    return msgsnd(mid, &fmsg, MESSAGE_SIZE, IPC_NOWAIT);

}
int messageShowClients(int mid){
    struct message fmsg;
    fmsg.receiver = 1;
    fmsg.msgType = 3;
    fmsg.number = getpid();
    strcpy(fmsg.text,"");
    return msgsnd(mid, &fmsg, MESSAGE_SIZE, IPC_NOWAIT);

}
int messageShowGroupClients(int mid, char* groupName){
    struct message fmsg;
    fmsg.receiver = 1;
    fmsg.msgType = 4;
    fmsg.number = getpid();
    strcpy(fmsg.text,groupName);
    return msgsnd(mid, &fmsg, MESSAGE_SIZE, IPC_NOWAIT);
}

int messageJoinGroup(int mid, char* groupName){
    struct message fmsg;
    fmsg.receiver = 1;
    fmsg.msgType = 5;
    fmsg.number = getpid();
    strcpy(fmsg.text,groupName);
    return msgsnd(mid, &fmsg, MESSAGE_SIZE, IPC_NOWAIT);
}
int messageLeaveGroup(int mid, char* groupName){
    struct message fmsg;
    fmsg.receiver = 1;
    fmsg.msgType = 6;
    fmsg.number = getpid();
    strcpy(fmsg.text,groupName);
    return msgsnd(mid, &fmsg, MESSAGE_SIZE, IPC_NOWAIT);
}
int messageShowGroups(int mid){
    struct message fmsg;
    fmsg.receiver = 1;
    fmsg.msgType = 7;
    fmsg.number = getpid();
    strcpy(fmsg.text,"");
    return msgsnd(mid, &fmsg, MESSAGE_SIZE, IPC_NOWAIT);
}

int messageSendMessageToGroup(int mid, char* text){
    struct message fmsg;
    fmsg.receiver = 1;
    fmsg.msgType = 8;
    fmsg.number = getpid();
    strcpy(fmsg.text,text);
    return msgsnd(mid, &fmsg, MESSAGE_SIZE, IPC_NOWAIT);
}
int messageSendMessageToClient(int mid, char* text){
    struct message fmsg;
    fmsg.receiver = 1;
    fmsg.msgType = 9;
    fmsg.number = getpid();
    strcpy(fmsg.text,text);
    return msgsnd(mid, &fmsg, MESSAGE_SIZE, IPC_NOWAIT);
}


