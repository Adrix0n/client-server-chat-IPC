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

#define MESSAGE_SIZE 1088
#define LONGER_MESSAGE_SIZE 4160
#define MAX_NICKNAME_SIZE 20
#define MAX_TEXT_SIZE 100
#define MAX_NUMBER_OF_CLIENTS 40
#define MAX_NUMBER_OF_GROUPS 40


// Structure of message that is sent between server and client
struct message{
    long receiver;
    long msgType;
    long pid;
    char text[1024];
}msg;

// Structure of special message called for "Read received messages" request 
struct messageLonger{
    long receiver;
    long msgType;
    long pid;
    char text[4096];
}msgLonger;

struct bClients{    // Additional structure which contains PID and nickname of blocked user
    long pid;
    char nickname[MAX_NICKNAME_SIZE];
};

// Creating array of clients
int clientsSize = 0; // Current number of connected clients
struct clientData{
    long pid;
    char nickname[MAX_NICKNAME_SIZE];
    int messagesidx;    // Index where next message will be saved in messages[][]
    char messages[20][MAX_TEXT_SIZE+MAX_NICKNAME_SIZE*2+6];

    int blockedClientsidx;
    struct bClients blockedClients[MAX_NUMBER_OF_CLIENTS]; // List of blocked users by clients[x]
}clients[MAX_NUMBER_OF_CLIENTS];

// Creating array of groups
int groupsSize = 0;
struct groupData{   // Structure of group
    char name[MAX_NICKNAME_SIZE];

    int clientsSize;
    struct clientData clients[20];
}groups[MAX_NUMBER_OF_GROUPS];

int findClientByNickname(char* text);   // Fucntion returns client index from clients[] by his nickname. If not found, it returns -1
int findClientByPid(int pid);           // Fucntion returns client index from clients[] by his PID. If not found, it returns -1
int findClientInGroupByName(char *text, int groupidx); // Function returns client index from groups[groupidx].clients[] by nickname
int findClientInGroupByPid(int pid, int groupidx);     // Function returns client index from groups[groupidx].clients[] by PID
int findGroupByName(char* groupName);   // Function returns group index from groups[]

int rcvLogin(struct message fmsg); // msgType = 1
int rcvLogout(struct message fmsg); // msgType = 2
int rcvShowClients(struct message fmsg); // msgType = 3
int rcvShowGroupClients(struct message fmsg); // msgType = 4
int rcvJoinGroup(struct message fmsg); // msgType = 5
int rcvLeaveGroup(struct message fmsg); // msgType = 6
int rcvShowGroups(struct message fmsg); // msgType = 7
int rcvSendMessageToGroup(struct message fmsg); // msgType = 8
int rcvSendMessageToClient(struct message fmsg); // msgType = 9
int rcvReceivemessages(struct message fmsg); // msgType = 10
int rcvShowBlockedClients(struct message fmsg); // msgType = 11
int rcvBlockClient(struct message fmsg); // msgType = 12
int rcvUnblockClient(struct message fmsg); // msgType = 13

int sid;

int main(){
    // Opening initial file and creating test users and groups
    int fd = open("initialclients",O_RDONLY);
    char buf[1000];
    char name[20];
    int i,n,j;
    j=0;
    while((n=read(fd,buf,1000))>0){
        for(i=0;i<n;i++){
            if(buf[i]!='\n'){
                name[j] = buf[i];
                j++;
            }else{
                if(j<20)
                    name[j] = '\0';
                if(clientsSize<9){
                    struct clientData newClient;
                    newClient.pid = 1;
                    strcpy(newClient.nickname,name);
                    newClient.messagesidx=0;
                    newClient.blockedClientsidx=0;
                    clients[clientsSize] = newClient;
                    clientsSize++;
                }else{
                    struct groupData newGroup;
                    strcpy(newGroup.name,name);
                    newGroup.clientsSize=0;
                    groups[groupsSize] = newGroup;
                    groupsSize++;
                }
                j=0;
            }
        }
    }


    int mid = msgget(0x1234, 0660 | IPC_CREAT);
    //printf("mid = %d\n",mid);
    
    sid = mid;  // For some reason mid cannot be global variable, so global sid becomes mid.
                // By doing this, it is not necessery to send mid to functions

    // Main loop. That is where server waits for signals from clients.
    while(1){
        msgrcv(mid, &msg, MESSAGE_SIZE, 1, 0);
        switch(msg.msgType){
            case 1:{rcvLogin(msg); break;}
            case 2:{rcvLogout(msg); break;}
            case 3:{rcvShowClients(msg); break;}
            case 4:{rcvShowGroupClients(msg); break;}
            case 5:{rcvJoinGroup(msg); break;}
            case 6:{rcvLeaveGroup(msg); break;}
            case 7:{rcvShowGroups(msg); break;}
            case 8:{rcvSendMessageToGroup(msg); break;}
            case 9:{rcvSendMessageToClient(msg); break;}
            case 10:{rcvReceivemessages(msg); break;}
            case 11:{rcvShowBlockedClients(msg);break;}
            case 12:{rcvBlockClient(msg);break;}
            case 13:{rcvUnblockClient(msg);break;}
            default:{
                printf("Uknown message\n");
                printf("receiver: %ld\n",msg.receiver);
                printf("msgType: %ld\n",msg.msgType);
                printf("pid: %ld\n",msg.pid);
                printf("text: %s\n",msg.text);
            }
        }
    }
    msgctl(mid, IPC_RMID, NULL);
    return 0;
}

int findClientByNickname(char* text){
    int i = 0;
    for(i=0;i<clientsSize;i++)
        if(strcmp(clients[i].nickname,text)==0)
            return i;
    return -1;
}
int findClientInGroupByName(char *text, int groupidx){
    int i = 0;
    for(i=0;i<groups[groupidx].clientsSize;i++)
        if(strcmp(groups[groupidx].clients[i].nickname,text)==0)
            return i;
    return -1;
}
int findClientInGroupByPid(int pid, int groupidx){
    int i = 0;
    for(i=0;i<groups[groupidx].clientsSize;i++)
        if(groups[groupidx].clients[i].pid==pid)
            return i;
    return -1;
}
int findClientByPid(int pid){
    int i = 0;
    for(i=0;i<clientsSize;i++)
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
    msg.receiver = fmsg.pid;
    msg.msgType = fmsg.msgType;
    msg.pid = fmsg.pid;
    strcpy(msg.text,fmsg.text);

    if(strlen(msg.text)>=MAX_NICKNAME_SIZE){ // Checks, if nickname is not too long
        strcpy(msg.text,"Podana nazwa jest za dluga.\n");
        msg.pid = -1;
        msgsnd(sid, &msg, MESSAGE_SIZE, IPC_NOWAIT);
        return -1;
    }
    if(findClientByNickname(msg.text)!=-1){ // Checks, if client with given nickname is already logged in
        strcpy(msg.text,"Uzytkownik jest juz zalogowany\n");
        msg.pid = -1;
        msgsnd(sid, &msg, MESSAGE_SIZE, IPC_NOWAIT);
        return -1;
    }
    if(clientsSize>=MAX_NUMBER_OF_CLIENTS){   // Checks, if limit of logged users is reached.
        strcpy(msg.text,"Osiagnieto maksymalna liczbe uzytkownikow\n");
        msg.pid = -1;
        msgsnd(sid, &msg, MESSAGE_SIZE, IPC_NOWAIT);
        return -1;
    }   

    // Creating new client
    struct clientData newClient;
    newClient.pid = msg.pid;
    strcpy(newClient.nickname,msg.text);
    newClient.messagesidx=0;
    newClient.blockedClientsidx=0;
    int k;
    for(k=0;k<20;k++)
        newClient.messages[k][0]='\0';
    clients[clientsSize] = newClient;
    clientsSize++;
    printf("Added user pid=%ld nickname=%s to server\n", newClient.pid, newClient.nickname);
   
    // Feedback message
    strcpy(msg.text,"You have been added to the server\n");
    msgsnd(sid, &msg, MESSAGE_SIZE, IPC_NOWAIT);
    return 0;
}
int rcvLogout(struct message fmsg){
    msg.receiver = fmsg.pid;
    msg.msgType = fmsg.msgType;
    msg.pid = fmsg.pid;
    
    // Deleting client from every group
    int i,j;
    for(j = 0; j<groupsSize; j++){
        int clientIndex = findClientInGroupByPid(msg.pid,j);
        if(clientIndex!=-1){
            for(i = clientIndex; i<groups[j].clientsSize-1;i++)
                groups[j].clients[i] = groups[j].clients[i+1];
            
            groups[j].clientsSize--;
        }
    }

    // Sprawdzenie poprawności wysłania żądania wylogowania przez użytkownika. Błąd w teorii nigdy nie powinien zostać wyświetlony, gdyż docelowo
    // jedynie zalogowany użytkownik może się wylogować. W trakcie testów działania programów odkryto, że w sytacjach, gdy serwer się wyłączy
    // a proces klienta wciąż trwa, to klient po wywołaniu wiadomości wylogowania spowoduje, że ponowne uruchomienie serwera wyświetli poniższą
    // instrukcję printf()
    int clientIndex = findClientByPid(msg.pid);
    if(clientIndex == -1){
        printf("rcvLogout error!!\n");
        return 0;
    }

    // Deleting client from client array
    printf("User pid=%ld nickname=%s logged out\n", clients[clientIndex].pid, clients[clientIndex].nickname);
    for(i=clientIndex;i<clientsSize-1;i++)
        clients[i] = clients[i+1];
    clientsSize--;

    // Feedback message
    strcpy(msg.text,"Logged out succesfully\n");
    msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
    return 0;
}
int rcvShowClients(struct message fmsg){
    printf("Client pid=%ld requests ShowClients\n",msg.pid);
    msg.receiver = fmsg.pid;
    msg.msgType = fmsg.msgType;
    msg.pid = fmsg.pid;

    // Getting clients nicknames from clients array
    char text[1024];
    strcpy(text,"List of users: \n");
    int i;
    for(i=0;i<clientsSize;i++){
        strcat(text,clients[i].nickname);
        strcat(text,"\n");
    }

    // Feedback message
    strcpy(msg.text,text);
    msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
    return 0;
}
int rcvShowGroupClients(struct message fmsg){
    printf("Client pid=%ld requests ShowGroupClients\n",fmsg.pid);
    msg.receiver = fmsg.pid;
    msg.msgType = fmsg.msgType;
    msg.pid = fmsg.pid;
    char groupName[1024];
    strcpy(groupName,fmsg.text);

    // Finding index of requested group from groups[]
    int groupidx = findGroupByName(groupName);
    if(groupidx==-1){
        strcpy(msg.text,"This group does not exists\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;
    }

    // Creating feedback message
    char text[1024];
    strcpy(text,"List of logged users: ");
    strcat(text,groupName);
    strcat(text," : \n");
    int i;
    for(i=0;i<groups[groupidx].clientsSize;i++){
        strcat(text,groups[groupidx].clients[i].nickname);
        strcat(text,"\n");
    }

    // Feedback message
    strcpy(msg.text,text);
    msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
    return 0;

}
int rcvJoinGroup(struct message fmsg){
    printf("Client %ld requests JoinGroup\n",fmsg.pid);
    msg.receiver = fmsg.pid;
    msg.msgType = fmsg.msgType;
    msg.pid = fmsg.pid;
    char groupName[1024];
    strcpy(groupName,fmsg.text);

    if(strlen(groupName)>=MAX_NICKNAME_SIZE){ // Checks, if given name is not too long
        strcpy(msg.text,"Given group name is too long.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;        
    }

    
    int groupidx;
    if((groupidx = findGroupByName(groupName))!=-1){ // Checks, if group with that name exists
        if(findClientInGroupByPid(msg.pid,groupidx)!=-1){ // Checks, if user is already in this group
            strcpy(msg.text,"You are already in this group\n");
            msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
            return -1;
        }
        if(groups[groupidx].clientsSize>=MAX_NUMBER_OF_CLIENTS){    // Checks, if group reached maximum users
            strcpy(msg.text,"Cannot join to this group. Maximum users in group reached.\n");
            msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
            return -1;     
        }

        // Adding client to group
        struct clientData newClient;
        newClient.pid = msg.pid;
        int clientidx = findClientByPid(msg.pid);
        strcpy(newClient.nickname,clients[clientidx].nickname);
        groups[groupidx].clients[groups[groupidx].clientsSize]=newClient;
        groups[groupidx].clientsSize+=1;

        // Feedback message
        strcpy(msg.text,"Joined to group\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return 0;
    }

    // If there is no group with given name, new group is created

    if(groupsSize>=MAX_NUMBER_OF_GROUPS){ // Checks, if limit of groups is reached
        strcpy(msg.text,"Server reached limit of groups. Cannot create new one.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;
    }

    // Creating client to join to group
    struct clientData newClient;
    newClient.pid = msg.pid;
    int clientidx = findClientByPid(msg.pid);
    newClient.messagesidx=0;
    newClient.blockedClientsidx=0;
    strcpy(newClient.nickname,clients[clientidx].nickname);

    // Creating new group
    struct  groupData newGroup;
    newGroup.clientsSize=1;
    newGroup.clients[0] = newClient;
    strcpy(newGroup.name,groupName);
    groups[groupsSize] = newGroup;
    groupsSize++;

    // Feedback message
    strcpy(msg.text, "New group created.\n");
    msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
    return 0;
}
int rcvLeaveGroup(struct message fmsg){
    printf("Client pid=%ld requests LeaveGroup\n",fmsg.pid);
    msg.receiver = fmsg.pid;
    msg.msgType = fmsg.msgType;
    msg.pid = fmsg.pid;
    char groupName[1024];
    strcpy(groupName,fmsg.text);

    int groupidx;
    if((groupidx = findGroupByName(groupName))==-1){ // // Checks, if group with that name exists
        strcpy(msg.text,"There is no group with given name.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;
    }

    int clientidx;
    if((clientidx = findClientInGroupByPid(msg.pid,groupidx))==-1){ // Checks, if user is in group
        strcpy(msg.text,"You are not in this group.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;
    }

    // Deleting user from group
    int i;
    for(i=clientidx;i<groups[groupidx].clientsSize-1;i++)
        groups[groupidx].clients[i] = groups[groupidx].clients[i+1];
    groups[groupidx].clientsSize--;

    // Deleting group, if there are no users
    if(groups[groupidx].clientsSize==0){
        for(i=groupidx;i<groupsSize-1;i++)
            groups[i] = groups[i+1];
        groupsSize--;
    }

    // Feedback message
    strcpy(msg.text,"Group left successfully.");
    msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
    return 0;
}
int rcvShowGroups(struct message fmsg){
    printf("Client pid=%ld requests ShowGroups\n",fmsg.pid);
    msg.receiver = fmsg.pid;
    msg.msgType = fmsg.msgType;
    msg.pid = fmsg.pid;
    
    // Creating message with list of group names
    char text[1024];
    int i;
    strcpy(text,"List of groups:\n");
    for(i=0;i<groupsSize;i++){
        strcat(text,groups[i].name);
        strcat(text,"\n");
    }

    // Feedback message
    strcpy(msg.text,text);
    msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
    return 0;
}
int rcvSendMessageToGroup(struct message fmsg){
    printf("Client pid=%ld requests SendMessageToGroup\n",fmsg.pid);
    msg.receiver = fmsg.pid;
    msg.msgType = fmsg.msgType;
    msg.pid = fmsg.pid;
    char text[1024];
    strcpy(text,fmsg.text);

    // Podana wiadomość od klienta utworzona zostaje w formacie "nazwa_grupy wlasciwa_wiadomosc". Nalezy z tak podanego ciągu znaków oddzielić
    // nazwę grupy od wiadomości

    // Looking for space char
    int length = strlen(text);
    int i;
    for(i=0;i<length;i++)
        if(text[i]==' ')
            break;
    if(i==length)
        return -1;

    // Saving group name
    char groupName[1024];
    int j;
    for(j=0;j<i;j++)
        groupName[j] = text[j];
    groupName[i]='\0';
    
    if(strlen(groupName)>=MAX_NICKNAME_SIZE){ // Checks, if group name is not too long
        strcpy(msg.text,"Group name is too long.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;        
    }

    int groupidx;
    if((groupidx = findGroupByName(groupName))==-1){ // Checks, if group is already on the server
        strcpy(msg.text,"This group does not exists.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;
    }

    if(findClientInGroupByPid(msg.pid,groupidx)==-1){ // Sprawdzenie, czy nadawca należy do podanej grupy
        strcpy(msg.text,"You are not in this group.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;
    }

    // Operowanie na ciągach znaków w C jest trudne
    i++;
    for(j=0;j<length-i;j++)
        text[j]=text[i+j];
    text[j]='\0';
    strcpy(msg.text,text);

    if(strlen(text)>=MAX_TEXT_SIZE){ // Sprawdzenie, czy podana wiadomość nie jest za długa
        strcpy(msg.text,"Your message is too long.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;
    }

    // Tworzenie informacji zwrotnej
    // Dodanie informacji o nadawcy wiadomości
    char text2[1024];
    text2[0]='\0';
    int senderidx = findClientByPid(msg.pid);
    strcpy(text2,"<");
    strcat(text2,clients[senderidx].nickname);
    strcat(text2,"/");
    strcat(text2,groups[groupidx].name);
    strcat(text2,">: ");
    strcat(text2,text);
    
    // Rozesłanie wiadomości do członków grupy
    int clientidx;
    for(j=0;j<groups[groupidx].clientsSize;j++){
        clientidx = findClientByPid(groups[groupidx].clients[j].pid);
        // Opcja pominięcia nadawcy jako odbiorcy
        // if(clients[clientidx].pid==msg.pid)
        //     continue;
        strcpy(clients[clientidx].messages[clients[clientidx].messagesidx],text2);
        clients[clientidx].messagesidx++;
        if(clients[clientidx].messagesidx>=20)
            clients[clientidx].messagesidx=0;
    }

    // Informacja zwrotna
    strcpy(msg.text,"Message sent to group.\n");
    msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
    return 0;
}
int rcvSendMessageToClient(struct message fmsg){
    printf("Client pid=%ld requests SendMessageToClient\n",fmsg.pid);
    msg.receiver = fmsg.pid;
    msg.msgType = fmsg.msgType;
    msg.pid = fmsg.pid;
    char text[1024];
    strcpy(text,fmsg.text);

    // Podana wiadomość od klienta utworzona zostaje w formacie "nazwa_klienta wlasciwa_wiadomosc". Nalezy z tak podanego ciągu znaków oddzielić
    // nazwę klienta od wiadomości

    // Szukanie spacji w podanej wiadomości
    int length = strlen(text);
    int i;
    for(i=0;i<length;i++)
        if(text[i]==' ')
            break;
    if(i==length)
        return -1;

    // Zapisanie nazwy klienta
    char clientName[1024];
    int j;
    for(j=0;j<i;j++)
        clientName[j] = text[j];
    clientName[i]='\0';

    if(strlen(clientName)>=MAX_NICKNAME_SIZE){ // Sprawdzenie, czy podana nazwa klienta nie jest za długa
        strcpy(msg.text,"Nickname is too long.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;        
    }    
    int clientidx;
    if((clientidx = findClientByNickname(clientName))==-1){ // Sprawdzenie, czy podany użytkownik jest zalogowany
        strcpy(msg.text,"There is no user with that nickname.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;
    }
    if(clients[clientidx].pid==msg.pid){ // Sprawdzenie, czy klient wysyła wiadomość do samego siebie
        strcpy(msg.text,"You tried to sent message to yourself.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;
    }

    // Nadpisanie przeslanej treści wiadomości z formatu "nazwa_klienta wlasciwa_wiadomosc" na "wlasciwa_wiadomosc"
    i++;
    for(j=0;j<length-i;j++)
        text[j]=text[i+j];
    text[j]='\0';

    if(strlen(text)>=MAX_TEXT_SIZE){ // Sprawdzenie, czy podana wiadomość nie jest za długa
        strcpy(msg.text,"Your message is too long.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;        
    }

    // Tworzenie informacji zwrotnej
    // Dodanie infromacji o nadawcy wiadomości
    char text2[1024];
    int senderidx = findClientByPid(msg.pid);
    strcpy(text2,"<");
    strcat(text2,clients[senderidx].nickname);
    strcat(text2,">: ");
    strcat(text2,text);

    // Wysłanie wiadomości do odbiorcy
    strcpy(clients[clientidx].messages[clients[clientidx].messagesidx],text2);
    clients[clientidx].messagesidx++;
    if(clients[clientidx].messagesidx>=20)
        clients[clientidx].messagesidx=0;

    // Informacja zwrotna
    strcpy(msg.text,"Message sent successfully.\n");
    msgsnd(sid,&msg,MESSAGE_SIZE,0);
    return 0;
}
int rcvReceivemessages(struct message fmsg){
    printf("Client pid=%ld requests Receivemessages\n",fmsg.pid);
    msgLonger.receiver = fmsg.pid;
    msgLonger.msgType = fmsg.msgType;
    msgLonger.pid = fmsg.pid;
    char text[4096];
    strcpy(text,"");

    // Szukanie wiadomości własciwego klienta i dodawanie treści wiadomości do informacji zwrotnej
    int clientidx = findClientByPid(msg.pid);
    int i;
    for(i=clients[clientidx].messagesidx;i<20;i++){
        strcat(text,clients[clientidx].messages[i]);
    }
    for(i=0;i<clients[clientidx].messagesidx;i++){
        strcat(text,clients[clientidx].messages[i]);
    }

    // Informacja zwrotna
    strcpy(msgLonger.text,text);
    msgsnd(sid,&msgLonger,LONGER_MESSAGE_SIZE,0);
    return 0;
}

int rcvShowBlockedClients(struct message fmsg){
    printf("Client pid=%ld requests ShowBlockedClients\n",fmsg.pid);
    msg.receiver = fmsg.pid;
    msg.msgType = fmsg.msgType;
    msg.pid = fmsg.pid;
    char text[1024];
    strcpy(text,"");

    int clientidx = findClientByPid(msg.pid);

    // Tworzenie informacji zwrotnej - dodanie do niej nazw zablokowanych klientów
    strcpy(text,"List of blocked users:\n");
    int i;
    for(i=0;i<clients[clientidx].blockedClientsidx;i++){
        printf("%d\n",i);
        strcat(text,clients[clientidx].blockedClients[i].nickname);
        strcat(text,"\n");
    }

    // Informacja zwrotna
    strcpy(msg.text,text);
    msgsnd(sid,&msg,MESSAGE_SIZE,0);
    return 0;

}
int rcvBlockClient(struct message fmsg){
    printf("Client pid=%ld requests BlockClient\n",fmsg.pid);
    msg.receiver = fmsg.pid;
    msg.msgType = fmsg.msgType;
    msg.pid = fmsg.pid;
    char text[1024];
    strcpy(text,fmsg.text);

    int clientidx = findClientByPid(msg.pid);

    if(strlen(text)>=MAX_NICKNAME_SIZE){
        strcpy(msg.text,"Nickname is too long.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;        
    }
    if(clients[clientidx].blockedClientsidx>=MAX_NUMBER_OF_CLIENTS){
        strcpy(msg.text,"Maximum number of blocked users reached.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,0);
        return 0;        
    }
    int blockclientidx = findClientByNickname(text);
    if(blockclientidx==-1){
        strcpy(msg.text,"There is no user with this nickname.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,0);
        return 0;       
    }
    int i;
    for(i=0;i<clients[clientidx].blockedClientsidx;i++)
        if(strcmp(text,clients[clientidx].blockedClients[i].nickname)==0){
            strcpy(msg.text,"User already is in the list.\n");
            msgsnd(sid,&msg,MESSAGE_SIZE,0);
            return 0; 
        }

    // Dodanie klienta do tablicy zablokowanych klientów
    strcpy(clients[clientidx].blockedClients[clients[clientidx].blockedClientsidx].nickname,clients[blockclientidx].nickname);
    clients[clientidx].blockedClients[clients[clientidx].blockedClientsidx].pid = clients[blockclientidx].pid;
    clients[clientidx].blockedClientsidx++;

    // Informacja zwrotna
    strcpy(msg.text,"User blocked successfully.\n");
    msgsnd(sid,&msg,MESSAGE_SIZE,0);
    return 0;
}
int rcvUnblockClient(struct message fmsg){
    printf("Client pid=%ld requests UnblockClient\n",fmsg.pid);
    msg.receiver = fmsg.pid;
    msg.msgType = fmsg.msgType;
    msg.pid = fmsg.pid;
    char text[1024];
    strcpy(text,fmsg.text);

    int clientidx = findClientByPid(msg.pid);

    if(strlen(text)>=MAX_NICKNAME_SIZE){
        strcpy(msg.text,"Nickname is too long.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;        
    }

    // Szukamy podanej nazwy klienta na liście zablokowanych klientów
    int blockedclientidx =-1;
    int i;
    for(i=0;i<clients[clientidx].blockedClientsidx;i++)
        if(strcmp(text,(clients[clientidx].blockedClients[i]).nickname)==0){
            blockedclientidx = i;
            break;
        }

    if(blockedclientidx==-1){
            strcpy(msg.text,"There is no user with this nickname in the list.\n");
            msgsnd(sid,&msg,MESSAGE_SIZE,0);
            return 0;         
    }

    // Usuwanie podanego klienta z tablicy zablokowanych klientów
    for(i=blockedclientidx;i<clients[clientidx].blockedClientsidx-1;i--){
        strcpy(clients[clientidx].blockedClients[i].nickname,clients[clientidx].blockedClients[i+1].nickname);
        clients[clientidx].blockedClients[i].pid = clients[clientidx].blockedClients[i+1].pid;
    }
    clients[clientidx].blockedClientsidx--;

    // Informacja zwrotna
    strcpy(msg.text,"User unblocked successfully.\n");
    msgsnd(sid,&msg,MESSAGE_SIZE,0);
    return 0;
}
