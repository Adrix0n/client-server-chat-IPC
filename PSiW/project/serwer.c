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

/*
    UWAGI
    IPC_NOWAIT powinien zostać zmieniony na 0, bo serwer posiada jedna strukture message, z której pobiera dane. Istnieje ryzyko, że jeszcze nie przesłane dane zostaną nadpisane.

*/


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
    }msg;

int clientDataSize = 0;
struct clientData{
    long pid;
    char nickname[1024];
}clients[100];

int groupsSize = 0;
struct groupData{
    int clientDataSize;
    struct clientData clients[20];
    char name[1024];
}groups[100];


int findInClients(char* text);
int findClientInGroupByName(char *text, int groupidx);
int findClientInGroupByIndex(int pid, int groupidx);
int findClientByIndex(int pid);

//1
int rcvLogin(struct message fmsg);
//2
int rcvLogout(struct message fmsg);
//3
int rcvShowClients(struct message fmsg);
//4
int rcvShowGroupClients(struct message fmsg);

//5
int rcvJoinGroup(struct message fmsg);
//6
int rcvLeaveGroup(struct message fmsg);
//7
int rcvShowGroups(struct message fmsg);

//8
int rcvSendMessageToGroup(struct message fmsg);
//9
int rcvSendMessageToClient(struct message fmsg);
//int rcvReceiveMessage(struct message fmsg);


int sid;

int main(){
    // C
    int i;
    for(i=0;i<100;i++)
        groups[i].clientDataSize=0;

    int mid = msgget(0x1234, 0660 | IPC_CREAT);
    printf("mid = %d\n",mid);
    sid = mid;  // mid dla wygody mógłby być globalny ale nie może, dlatego globalny sid staje się mid
                // dzięki temu nie trzeba do funkcji przesyłać wartości mid

    while(1){
        msgrcv(mid, &msg, MESSAGE_SIZE, 1, 0);
        switch(msg.msgType){
            case 1:{rcvLogin(msg);break;}
            case 2:{rcvLogout(msg);break;}
            case 3:{rcvShowClients(msg);break;}
            case 4:{rcvShowGroupClients(msg);break;}
            case 5:{rcvJoinGroup(msg);break;}
            case 6:{rcvLeaveGroup(msg); break;}
            case 7:{rcvShowGroups(msg); break;}
            case 8:{rcvSendMessageToGroup(msg); break;}
            case 9:{rcvSendMessageToClient(msg); break;}
            case 10:{break;}
            default:{
                printf("Nieznana wiadomosc\n");
                printf("receiver: %ld\n",msg.receiver);
                printf("msgType: %ld\n",msg.msgType);
                printf("number: %ld\n",msg.number);
                printf("text: %s\n",msg.text);
            }
        }
    }

    msgctl(mid, IPC_RMID, NULL);


    return 0;
}

int findInClients(char* text){
    int i = 0;
    for(i=0;i<clientDataSize;i++)
        if(strcmp(clients[i].nickname,text)==0)
            return i;

    //printf("Nie znaleziono uzytkownika!\n");
    return -1;
}

int findClientInGroupByName(char *text, int groupidx){
    int i = 0;
    for(i=0;i<groups[groupidx].clientDataSize;i++)
        if(strcmp(groups[groupidx].clients[i].nickname,text)==0)
            return i;

    return -1;
}
int findClientInGroupByIndex(int pid, int groupidx){
    int i = 0;
    for(i=0;i<groups[groupidx].clientDataSize;i++)
        if(groups[groupidx].clients[i].pid==pid)
            return i;

    return -1;
}
int findClientByIndex(int pid){
    int i = 0;
    for(i=0;i<clientDataSize;i++)
        if(clients[i].pid==pid)
            return i;

    return -1;
}

int findGroupByName(char* groupName){
    int i;
    for(i=0;i<groupsSize;i++)
        if(strcmp(groups[i].name,groupName)==0)
            return i;
    return -1;
}

int rcvLogin(struct message fmsg){
    msg.receiver = fmsg.number;
    msg.msgType = fmsg.msgType;
    msg.number = fmsg.number;

    if(findInClients(msg.text)!=-1){
        strcpy(msg.text,"Uzytkownik jest juz zalogowany");
        msg.number = -1;
        msgsnd(sid, &msg, MESSAGE_SIZE, IPC_NOWAIT);
        return -1;
    }
    if(clientDataSize>=100)
    {
        strcpy(msg.text,"Osiagnieto maksymalna liczbe uzytkownikow");
        msg.number = -1;
        msgsnd(sid, &msg, MESSAGE_SIZE, IPC_NOWAIT);
        return -1;
    }

    struct clientData newClient;
    newClient.pid = msg.number;
    strcpy(newClient.nickname,msg.text);

    clients[clientDataSize] = newClient;
    clientDataSize++;
    printf("Dodano użytkownika %ld %s do serwera\n", newClient.pid, newClient.nickname);

    // Wiadomość zwrotna
    strcpy(msg.text,"Zostales dodany do serwera");
    msgsnd(sid, &msg, MESSAGE_SIZE, IPC_NOWAIT);

    return 0;
}

int rcvLogout(struct message fmsg){
    msg.receiver = fmsg.number;
    msg.msgType = fmsg.msgType;
    msg.number = fmsg.number;
    
    // Wylogowanie z grup
    int i,j;
    for(j = 0; j<groupsSize; j++){
        int clientIndex = findClientInGroupByIndex(msg.number,j);
        if(clientIndex!=-1){
            for(i = clientIndex; i<groups[j].clientDataSize-1;i++)
                groups[j].clients[i] = groups[j].clients[i+1];
            
            groups[j].clientDataSize--;
        }
    }

    // Wylogowanie z serwera
    int clientIndex = findClientByIndex(msg.number);
    if(clientIndex == -1){
        printf("Blad rcvLogout!!\n");
        return 0;
    }

    printf("Wylogowano uzytkownika %ld %s z serwera\n", clients[clientIndex].pid, clients[clientIndex].nickname);
    for(i=clientIndex;i<clientDataSize-1;i++)
        clients[i] = clients[i+1];
    clientDataSize--;


    // Wiadomość zwrotna
    strcpy(msg.text,"Zostales pomyslnie wylogowany\n");
    msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);

    return 0;
}

int rcvShowClients(struct message fmsg){
    msg.receiver = fmsg.number;
    msg.msgType = fmsg.msgType;
    msg.number = fmsg.number;

    char text[1024];
    strcpy(text,"Lista uzytkownikow: \n");
    int i;
    for(i=0;i<clientDataSize;i++){
        strcat(text,clients[i].nickname);
        strcat(text,"\n");
    }

    printf("Klient %ld wykonal polecenie ShowClients\n",msg.number);

    // Wiadomość zwrotna
    strcpy(msg.text,text);
    msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);

    
    return 0;
}

int rcvShowGroupClients(struct message fmsg){
    printf("Klient %ld wykonal polecenie ShowGroupClients\n",fmsg.number);
    msg.receiver = fmsg.number;
    msg.msgType = fmsg.msgType;
    msg.number = fmsg.number;
    char groupName[1024];
    strcpy(groupName,fmsg.text);
    int groupidx = findGroupByName(groupName);
    if(groupidx==-1){
        strcpy(msg.text,"Nie znaleziono takiej grupy\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;
    }

    char text[1024];
    // Mozna poprawic wypisujac nazwe szukanej grupy
    strcpy(text,"Lista uzytkownikow w grupie : \n");
    int i;
    for(i=0;i<groups[groupidx].clientDataSize;i++){
        strcat(text,groups[groupidx].clients[i].nickname);
        strcat(text,"\n");
    }


    // Wiadomość zwrotna
    strcpy(msg.text,text);
    msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
    return 0;

}

int rcvJoinGroup(struct message fmsg){
    printf("Klient %ld wykonal polecenie JoinGroup\n",fmsg.number);
    msg.receiver = fmsg.number;
    msg.msgType = fmsg.msgType;
    msg.number = fmsg.number;
    char groupName[1024];
    strcpy(groupName,fmsg.text);
    // Jeżeli istnieje podana grupa to sprawdzamy, czy jesteśmy jej członkiem, czy też nie
    int groupidx;
    if((groupidx = findGroupByName(groupName))!=-1){
        if(findClientInGroupByIndex(msg.number,groupidx)!=-1){
            strcpy(msg.text,"Jestes juz czlonkiem tej grupy\n");
            msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
            return -1;
        }
        if(groups[groupidx].clientDataSize>20){
            strcpy(msg.text,"Nie mozna dolaczyc do grupy. Osiagnieto maksymalna liczbe osob w grupie.\n");
            msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
            return -1;     
        }

        struct clientData newClient;
        newClient.pid = msg.number;
        int clientidx = findClientByIndex(msg.number);
        strcpy(newClient.nickname,clients[clientidx].nickname);
        groups[groupidx].clients[groups[groupidx].clientDataSize]=newClient;
        groups[groupidx].clientDataSize+=1;

        strcpy(msg.text,"Dolaczyles do nowej grupy\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return 0;
    }
    if(groupsSize>=100){
        strcpy(msg.text,"Osiagnieto maksymalna liczbe grup. Nie mozna utworzyc nowej.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;
    }

    struct clientData newClient;
    newClient.pid = msg.number;
    int clientidx = findClientByIndex(msg.number);
    strcpy(newClient.nickname,clients[clientidx].nickname);

    struct  groupData newGroup;
    newGroup.clientDataSize=1;
    newGroup.clients[0] = newClient;
    strcpy(newGroup.name,groupName);
    groups[groupsSize] = newGroup;

    groupsSize++;

    strcpy(msg.text, "Utworzono nowa grupe\n");
    msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);

    return 0;
}

int rcvLeaveGroup(struct message fmsg){
    printf("Klient %ld wykonal polecenie LeaveGroup\n",fmsg.number);
    msg.receiver = fmsg.number;
    msg.msgType = fmsg.msgType;
    msg.number = fmsg.number;
    char groupName[1024];
    strcpy(groupName,fmsg.text);

    int groupidx;
    if((groupidx = findGroupByName(groupName))==-1){
        strcpy(msg.text,"Nie znaleziono takiej grupy.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;
    }

    int clientidx;
    if((clientidx = findClientInGroupByIndex(msg.number,groupidx))==-1){
        strcpy(msg.text,"Nie nalezysz do tej grupy.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;
    }

    int i;
    for(i=clientidx;i<groups[groupidx].clientDataSize-1;i++)
        groups[groupidx].clients[i] = groups[groupidx].clients[i+1];
    groups[groupidx].clientDataSize--;

    // Usunięcie grupy, jeżeli nie ma czlonków
    if(groups[groupidx].clientDataSize==0){
        for(i=groupidx;i<groupsSize-1;i++)
            groups[i] = groups[i+1];
        groupsSize--;
    }


    strcpy(msg.text,"Pomyslnie opusciles grupe");
    msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
    return 0;

}

int rcvShowGroups(struct message fmsg){
    printf("Klient %ld wykonal polecenie ShowGroups\n",fmsg.number);
    msg.receiver = fmsg.number;
    msg.msgType = fmsg.msgType;
    msg.number = fmsg.number;

    char text[1024];
    int i;
    strcpy(text,"Lista istniejacych grup:\n");
    for(i=0;i<groupsSize;i++){
        strcat(text,groups[i].name);
        strcat(text,"\n");
    }
    strcpy(msg.text,text);
    msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
    return 0;
}

int rcvSendMessageToGroup(struct message fmsg){
    printf("Klient %ld wykonal polecenie SendMessageToGroup\n",fmsg.number);
    msg.receiver = fmsg.number;
    msg.msgType = fmsg.msgType;
    msg.number = fmsg.number;
    char text[1024];
    strcpy(text,fmsg.text);
    int length = strlen(text);
    int i;
    for(i=0;i<length;i++)
        if(text[i]==' ')
            break;
    if(i==length)
        return -1;
    char groupName[1024];
    int j;
    for(j=0;j<i;j++)
        groupName[j] = text[j];
    groupName[i]='\0';
    
    int groupidx;
    if((groupidx = findGroupByName(groupName))==-1){
        strcpy(msg.text,"Nie znaleziono takiej grupy.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;
    }
    printf("%s\n",text);

    i++;
    for(j=0;j<length-i;j++)
        text[j]=text[i+j];
    text[j]='\0';
    strcpy(msg.text,text);

    printf("%s\n",text);
    for(j=0;j<groups[groupidx].clientDataSize;j++){
        if(groups[groupidx].clients[j].pid == msg.number)
            continue;
        printf("Pid uzytkownika %ld\n",groups[groupidx].clients[j].pid);
        msg.receiver = groups[groupidx].clients[i].pid;
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
    }

    strcpy(msg.text,"Wiadomosc zostala wyslana do czlonkow grupy.\n");
    msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);

    return 0;
}

int rcvSendMessageToClient(struct message fmsg){
    printf("Klient %ld wykonal polecenie SendMessageToClient\n",fmsg.number);
    msg.receiver = fmsg.number;
    msg.msgType = fmsg.msgType;
    msg.number = fmsg.number;
    char text[1024];
    strcpy(text,fmsg.text);
    int length = strlen(text);
    int i;
    for(i=0;i<length;i++)
        if(text[i]==' ')
            break;
    if(i==length)
        return -1;
    char clientName[1024];
    int j;
    for(j=0;j<i;j++)
        clientName[j] = text[j];
    clientName[i]='\0';
    
    int clientidx;
    if((clientidx = findInClients(clientName))==-1){
        strcpy(msg.text,"Nie znaleziono takiego uzytkownika.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;
    }
    printf("%s\n",text);

    i++;
    for(j=0;j<length-i;j++)
        text[j]=text[i+j];
    text[j]='\0';
    strcpy(msg.text,text);

    msg.receiver = clients[clientidx].pid;
    msgsnd(sid,&msg,MESSAGE_SIZE,0);
    strcpy(msg.text,"Pomyslnie wyslano wiadomosc.\n");
    msg.receiver = msg.number;
    msgsnd(sid,&msg,MESSAGE_SIZE,0);


    return 0;
}