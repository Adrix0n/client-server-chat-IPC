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


// Struktura wiadomości przekazywanej pomiędzy serwerem i klientem
struct message{
    long receiver;
    long msgType;
    long pid;
    char text[1024];
}msg;

// Struktura specjalnej wiadomości wywoływanej dla zapytania o odebranie wiadomości
struct messageLonger{
    long receiver;
    long msgType;
    long pid;
    char text[4096];
}msgLonger;

struct bClients{    // Dodatkowa struktura zawierająca pid i nickname zablokowanego użytkownika
    long pid;
    char nickname[MAX_NICKNAME_SIZE];
};

// Utworzenie tablicy klientów 
int clientsSize = 0; // Oznacza rozmiar tablicy klientów
struct clientData{
    long pid;
    char nickname[MAX_NICKNAME_SIZE];
    int messegesidx;    // Oznacza aktualny indeks pod jakim będzie zapisywana kolejna wiadomość w messeges[][]
    char messeges[20][MAX_TEXT_SIZE+MAX_NICKNAME_SIZE*2+6];

    int blockedClientsidx;
    struct bClients blockedClients[MAX_NUMBER_OF_CLIENTS]; // Lista zablokowanych przez clients[x] innych użytkowników
}clients[MAX_NUMBER_OF_CLIENTS];

// Tworzenie tablicy grup
int groupsSize = 0;
struct groupData{   // Struktura grupy
    char name[MAX_NICKNAME_SIZE];

    int clientsSize;
    struct clientData clients[20];
}groups[MAX_NUMBER_OF_GROUPS];

int findClientByNickname(char* text);   // Funkcja zwraca indeks klienta w tablicy clients[]. W przypadku nie znalezienia zwrócona zostaje wartość -1
int findClientByPid(int pid);           // Podobnie jak poprzednia
int findClientInGroupByName(char *text, int groupidx); // Funkcja zwraca indeks klienta w tablicy groups[groupidx].clients[], czyli indeks w grupie
int findClientInGroupByPid(int pid, int groupidx);     // Podobnie jak poprzednia
int findGroupByName(char* groupName);   // Funkcja zwraca indeks grupy w tablicy groups[].

int rcvLogin(struct message fmsg); // msgType = 1
int rcvLogout(struct message fmsg); // msgType = 2
int rcvShowClients(struct message fmsg); // msgType = 3
int rcvShowGroupClients(struct message fmsg); // msgType = 4
int rcvJoinGroup(struct message fmsg); // msgType = 5
int rcvLeaveGroup(struct message fmsg); // msgType = 6
int rcvShowGroups(struct message fmsg); // msgType = 7
int rcvSendMessageToGroup(struct message fmsg); // msgType = 8
int rcvSendMessageToClient(struct message fmsg); // msgType = 9
int rcvReceiveMesseges(struct message fmsg); // msgType = 10
int rcvShowBlockedClients(struct message fmsg); // msgType = 11
int rcvBlockClient(struct message fmsg); // msgType = 12
int rcvUnblockClient(struct message fmsg); // msgType = 13

int sid;

int main(){
    // Otwarcie pliku konfiguracyjnego i dodanie z pliku klientów i grup
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
                    newClient.messegesidx=0;
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


    int mid = msgget(0x123, 0660 | IPC_CREAT);
    //printf("mid = %d\n",mid);
    sid = mid;  // mid dla wygody mógłby być globalny ale nie może, dlatego globalny sid staje się mid
                // dzięki temu nie trzeba do funkcji przesyłać wartości mid

    // Główna pętla programu, to tutaj serwer czeka na sygnał, a następnie różnie reaguje na żądanie w zależności od typu wiadomości
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
            case 10:{rcvReceiveMesseges(msg); break;}
            case 11:{rcvShowBlockedClients(msg);break;}
            case 12:{rcvBlockClient(msg);break;}
            case 13:{rcvUnblockClient(msg);break;}
            default:{
                printf("Nieznana wiadomosc\n");
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

    if(strlen(msg.text)>=MAX_NICKNAME_SIZE){ // Sprawdzenie, czy podana nazwa użytkowanika nie jest za długa
        strcpy(msg.text,"Podana nazwa jest za dluga.\n");
        msg.pid = -1;
        msgsnd(sid, &msg, MESSAGE_SIZE, IPC_NOWAIT);
        return -1;
    }
    if(findClientByNickname(msg.text)!=-1){ // Sprawdzenie, czy użytkownik o podanej nazwie jest już zalogowany
        strcpy(msg.text,"Uzytkownik jest juz zalogowany\n");
        msg.pid = -1;
        msgsnd(sid, &msg, MESSAGE_SIZE, IPC_NOWAIT);
        return -1;
    }
    if(clientsSize>=MAX_NUMBER_OF_CLIENTS){   // Sprawdzenie, czy osiągnięto maksymalną liczbę zalogowanych klientów
        strcpy(msg.text,"Osiagnieto maksymalna liczbe uzytkownikow\n");
        msg.pid = -1;
        msgsnd(sid, &msg, MESSAGE_SIZE, IPC_NOWAIT);
        return -1;
    }   

    // Jeżeli wszystko się zgadza to tworzony jest nowy klient o podanej nazwie
    struct clientData newClient;
    newClient.pid = msg.pid;
    strcpy(newClient.nickname,msg.text);
    newClient.messegesidx=0;
    newClient.blockedClientsidx=0;
    int k;
    for(k=0;k<20;k++)
        newClient.messeges[k][0]='\0';
    clients[clientsSize] = newClient;
    clientsSize++;
    printf("Dodano użytkownika %ld %s do serwera\n", newClient.pid, newClient.nickname);

    // Informacja zwrotna
    strcpy(msg.text,"Zostales dodany do serwera\n");
    msgsnd(sid, &msg, MESSAGE_SIZE, IPC_NOWAIT);
    return 0;
}
int rcvLogout(struct message fmsg){
    msg.receiver = fmsg.pid;
    msg.msgType = fmsg.msgType;
    msg.pid = fmsg.pid;
    
    // Usunięcie klienta z każdej grupy
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
        printf("Blad rcvLogout!!\n");
        return 0;
    }

    // Ususięcie klienta z tablicy klientów
    printf("Wylogowano uzytkownika %ld %s z serwera\n", clients[clientIndex].pid, clients[clientIndex].nickname);
    for(i=clientIndex;i<clientsSize-1;i++)
        clients[i] = clients[i+1];
    clientsSize--;

    // Inforacja zwrotna
    strcpy(msg.text,"Wylogowano pomyslnie.\n");
    msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
    return 0;
}
int rcvShowClients(struct message fmsg){
    printf("Klient %ld wykonal polecenie ShowClients\n",msg.pid);
    msg.receiver = fmsg.pid;
    msg.msgType = fmsg.msgType;
    msg.pid = fmsg.pid;

    // Z tablicy klientów pobierane są nazwy klientów, które dalej umieszczane są w text[]
    char text[1024];
    strcpy(text,"Lista uzytkownikow: \n");
    int i;
    for(i=0;i<clientsSize;i++){
        strcat(text,clients[i].nickname);
        strcat(text,"\n");
    }

    // Informacja zwrotna zawierająca listę zalogowanych klientów
    strcpy(msg.text,text);
    msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
    return 0;
}
int rcvShowGroupClients(struct message fmsg){
    printf("Klient %ld wykonal polecenie ShowGroupClients\n",fmsg.pid);
    msg.receiver = fmsg.pid;
    msg.msgType = fmsg.msgType;
    msg.pid = fmsg.pid;
    char groupName[1024];
    strcpy(groupName,fmsg.text);

    // Znajdowanie indeksu wskazanej grupy w tablicy groups[]    
    int groupidx = findGroupByName(groupName);
    if(groupidx==-1){
        strcpy(msg.text,"Nie znaleziono takiej grupy\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;
    }

    // Tworzenie informacji zwrotnej - dodawanie nazw klientów z grupy do text[]
    char text[1024];
    strcpy(text,"Lista uzytkownikow w grupie ");
    strcat(text,groupName);
    strcat(text," : \n");
    int i;
    for(i=0;i<groups[groupidx].clientsSize;i++){
        strcat(text,groups[groupidx].clients[i].nickname);
        strcat(text,"\n");
    }

    // Informacja zwrotna
    strcpy(msg.text,text);
    msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
    return 0;

}
int rcvJoinGroup(struct message fmsg){
    printf("Klient %ld wykonal polecenie JoinGroup\n",fmsg.pid);
    msg.receiver = fmsg.pid;
    msg.msgType = fmsg.msgType;
    msg.pid = fmsg.pid;
    char groupName[1024];
    strcpy(groupName,fmsg.text);

    if(strlen(groupName)>=MAX_NICKNAME_SIZE){ // Sprawdzenie, czy podana nazwa grupy nie jest za długa
        strcpy(msg.text,"Podana nazwa grupy jest zbyt dluga.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;        
    }

    
    int groupidx;
    if((groupidx = findGroupByName(groupName))!=-1){ // Jeżeli istnieje taka grupa, to sprawdzane jest, czy klient jest już członkiem tej grupy
        if(findClientInGroupByPid(msg.pid,groupidx)!=-1){ // Sprawdzenie, czy klient jest już członkiem podanej grupy
            strcpy(msg.text,"Jestes juz czlonkiem tej grupy\n");
            msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
            return -1;
        }
        if(groups[groupidx].clientsSize>=MAX_NUMBER_OF_CLIENTS){    // Sprawdzenie, czy grupa posiada już maksymalną liczbę członków
            strcpy(msg.text,"Nie mozna dolaczyc do grupy. Osiagnieto maksymalna liczbe osob w grupie.\n");
            msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
            return -1;     
        }

        // Jeżeli podane informacje są poprawne to klient dodawany jest do grupy
        struct clientData newClient;
        newClient.pid = msg.pid;
        int clientidx = findClientByPid(msg.pid);
        strcpy(newClient.nickname,clients[clientidx].nickname);
        groups[groupidx].clients[groups[groupidx].clientsSize]=newClient;
        groups[groupidx].clientsSize+=1;

        // Informacja zwrotna
        strcpy(msg.text,"Dolaczyles do nowej grupy\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return 0;
    }

    // Jeżeli podana grupa nie istnieje to tworzona jest nowa grupa 

    if(groupsSize>=MAX_NUMBER_OF_GROUPS){ // Sprawdzenie, czy osiągnięto maksymalną liczbę grup na serwerze
        strcpy(msg.text,"Osiagnieto maksymalna liczbe grup. Nie mozna utworzyc nowej.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;
    }

    // Tworzenie klienta do dodania do grupy
    struct clientData newClient;
    newClient.pid = msg.pid;
    int clientidx = findClientByPid(msg.pid);
    newClient.messegesidx=0;
    newClient.blockedClientsidx=0;
    strcpy(newClient.nickname,clients[clientidx].nickname);

    // Tworzenie nowej grupy wraz z klientem
    struct  groupData newGroup;
    newGroup.clientsSize=1;
    newGroup.clients[0] = newClient;
    strcpy(newGroup.name,groupName);
    groups[groupsSize] = newGroup;
    groupsSize++;

    // Informacja zwrotna
    strcpy(msg.text, "Utworzono nowa grupe\n");
    msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
    return 0;
}
int rcvLeaveGroup(struct message fmsg){
    printf("Klient %ld wykonal polecenie LeaveGroup\n",fmsg.pid);
    msg.receiver = fmsg.pid;
    msg.msgType = fmsg.msgType;
    msg.pid = fmsg.pid;
    char groupName[1024];
    strcpy(groupName,fmsg.text);

    int groupidx;
    if((groupidx = findGroupByName(groupName))==-1){ // Sprawdzenie, czy podana grupa istnieje
        strcpy(msg.text,"Nie znaleziono takiej grupy.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;
    }

    int clientidx;
    if((clientidx = findClientInGroupByPid(msg.pid,groupidx))==-1){ // Sprawdzenie, czy klient należy do podanej grupy
        strcpy(msg.text,"Nie nalezysz do tej grupy.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;
    }

    // Usunięcie klienta z grupy
    int i;
    for(i=clientidx;i<groups[groupidx].clientsSize-1;i++)
        groups[groupidx].clients[i] = groups[groupidx].clients[i+1];
    groups[groupidx].clientsSize--;

    // Usunięcie grupy, jeżeli nie ma ona czlonków
    if(groups[groupidx].clientsSize==0){
        for(i=groupidx;i<groupsSize-1;i++)
            groups[i] = groups[i+1];
        groupsSize--;
    }

    // Informacja zwrotna
    strcpy(msg.text,"Pomyslnie opusciles grupe");
    msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
    return 0;
}
int rcvShowGroups(struct message fmsg){
    printf("Klient %ld wykonal polecenie ShowGroups\n",fmsg.pid);
    msg.receiver = fmsg.pid;
    msg.msgType = fmsg.msgType;
    msg.pid = fmsg.pid;

    // Tworzenie informacji zwrotnej - dodawanie nazw istniejących grup do tablicy text[]
    char text[1024];
    int i;
    strcpy(text,"Lista istniejacych grup:\n");
    for(i=0;i<groupsSize;i++){
        strcat(text,groups[i].name);
        strcat(text,"\n");
    }

    // Informacja zwrotna
    strcpy(msg.text,text);
    msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
    return 0;
}
int rcvSendMessageToGroup(struct message fmsg){
    printf("Klient %ld wykonal polecenie SendMessageToGroup\n",fmsg.pid);
    msg.receiver = fmsg.pid;
    msg.msgType = fmsg.msgType;
    msg.pid = fmsg.pid;
    char text[1024];
    strcpy(text,fmsg.text);

    // Podana wiadomość od klienta utworzona zostaje w formacie "nazwa_grupy wlasciwa_wiadomosc". Nalezy z tak podanego ciągu znaków oddzielić
    // nazwę grupy od wiadomości

    // Szukanie spacji w podanej wiadomości
    int length = strlen(text);
    int i;
    for(i=0;i<length;i++)
        if(text[i]==' ')
            break;
    if(i==length)
        return -1;

    // Zapisanie nazwy grupy
    char groupName[1024];
    int j;
    for(j=0;j<i;j++)
        groupName[j] = text[j];
    groupName[i]='\0';
    
    if(strlen(groupName)>=MAX_NICKNAME_SIZE){ // Sprawdzenie, czy podana nazwa grupy nie jest za długa
        strcpy(msg.text,"Podana nazwa grupy jest zbyt dluga.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;        
    }

    int groupidx;
    if((groupidx = findGroupByName(groupName))==-1){ // Sprawdzenie, czy podana grupa znajduje się na serwerze
        strcpy(msg.text,"Nie znaleziono takiej grupy.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;
    }

    if(findClientInGroupByPid(msg.pid,groupidx)==-1){ // Sprawdzenie, czy nadawca należy do podanej grupy
        strcpy(msg.text,"Nie nalezysz do wskazanej grupy.\n");
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
        strcpy(msg.text,"Twoja wiadomosc jest zbyt dluga.\n");
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
        strcpy(clients[clientidx].messeges[clients[clientidx].messegesidx],text2);
        clients[clientidx].messegesidx++;
        if(clients[clientidx].messegesidx>=20)
            clients[clientidx].messegesidx=0;
    }

    // Informacja zwrotna
    strcpy(msg.text,"Wiadomosc zostala wyslana do czlonkow grupy.\n");
    msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
    return 0;
}
int rcvSendMessageToClient(struct message fmsg){
    printf("Klient %ld wykonal polecenie SendMessageToClient\n",fmsg.pid);
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
        strcpy(msg.text,"Podana nazwa uzytkownika jest zbyt dluga.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;        
    }    
    int clientidx;
    if((clientidx = findClientByNickname(clientName))==-1){ // Sprawdzenie, czy podany użytkownik jest zalogowany
        strcpy(msg.text,"Nie znaleziono takiego uzytkownika.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;
    }
    if(clients[clientidx].pid==msg.pid){ // Sprawdzenie, czy klient wysyła wiadomość do samego siebie
        strcpy(msg.text,"Probowales wyslac wiadomosc do samego siebie.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;
    }

    // Nadpisanie przeslanej treści wiadomości z formatu "nazwa_klienta wlasciwa_wiadomosc" na "wlasciwa_wiadomosc"
    i++;
    for(j=0;j<length-i;j++)
        text[j]=text[i+j];
    text[j]='\0';

    if(strlen(text)>=MAX_TEXT_SIZE){ // Sprawdzenie, czy podana wiadomość nie jest za długa
        strcpy(msg.text,"Twoja wiadomosc jest zbyt dluga.\n");
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
    strcpy(clients[clientidx].messeges[clients[clientidx].messegesidx],text2);
    clients[clientidx].messegesidx++;
    if(clients[clientidx].messegesidx>=20)
        clients[clientidx].messegesidx=0;

    // Informacja zwrotna
    strcpy(msg.text,"Pomyslnie wyslano wiadomosc.\n");
    msgsnd(sid,&msg,MESSAGE_SIZE,0);
    return 0;
}
int rcvReceiveMesseges(struct message fmsg){
    printf("Klient %ld wykonal polecenie ReceiveMesseges\n",fmsg.pid);
    msgLonger.receiver = fmsg.pid;
    msgLonger.msgType = fmsg.msgType;
    msgLonger.pid = fmsg.pid;
    char text[4096];
    strcpy(text,"");

    // Szukanie wiadomości własciwego klienta i dodawanie treści wiadomości do informacji zwrotnej
    int clientidx = findClientByPid(msg.pid);
    int i;
    for(i=clients[clientidx].messegesidx;i<20;i++){
        strcat(text,clients[clientidx].messeges[i]);
    }
    for(i=0;i<clients[clientidx].messegesidx;i++){
        strcat(text,clients[clientidx].messeges[i]);
    }

    // Informacja zwrotna
    strcpy(msgLonger.text,text);
    msgsnd(sid,&msgLonger,LONGER_MESSAGE_SIZE,0);
    return 0;
}

int rcvShowBlockedClients(struct message fmsg){
    printf("Klient %ld wykonal polecenie ShowBlockedClients\n",fmsg.pid);
    msg.receiver = fmsg.pid;
    msg.msgType = fmsg.msgType;
    msg.pid = fmsg.pid;
    char text[1024];
    strcpy(text,"");

    int clientidx = findClientByPid(msg.pid);

    // Tworzenie informacji zwrotnej - dodanie do niej nazw zablokowanych klientów
    strcpy(text,"Lista zablokowanych uzytkownikow:\n");
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
    printf("Klient %ld wykonal polecenie BlockClient\n",fmsg.pid);
    msg.receiver = fmsg.pid;
    msg.msgType = fmsg.msgType;
    msg.pid = fmsg.pid;
    char text[1024];
    strcpy(text,fmsg.text);

    int clientidx = findClientByPid(msg.pid);

    if(strlen(text)>=MAX_NICKNAME_SIZE){
        strcpy(msg.text,"Podana nazwa uzytkownika jest zbyt dluga.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,IPC_NOWAIT);
        return -1;        
    }
    if(clients[clientidx].blockedClientsidx>=MAX_NUMBER_OF_CLIENTS){
        strcpy(msg.text,"Osiagnieto maksymalna liczbe zablokowanych uzytkownikow.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,0);
        return 0;        
    }
    int blockclientidx = findClientByNickname(text);
    if(blockclientidx==-1){
        strcpy(msg.text,"Nie ma takiego uzytkownika.\n");
        msgsnd(sid,&msg,MESSAGE_SIZE,0);
        return 0;       
    }
    int i;
    for(i=0;i<clients[clientidx].blockedClientsidx;i++)
        if(strcmp(text,clients[clientidx].blockedClients[i].nickname)==0){
            strcpy(msg.text,"Uzytkownik jest juz na liscie zablokowanych.\n");
            msgsnd(sid,&msg,MESSAGE_SIZE,0);
            return 0; 
        }

    // Dodanie klienta do tablicy zablokowanych klientów
    strcpy(clients[clientidx].blockedClients[clients[clientidx].blockedClientsidx].nickname,clients[blockclientidx].nickname);
    clients[clientidx].blockedClients[clients[clientidx].blockedClientsidx].pid = clients[blockclientidx].pid;
    clients[clientidx].blockedClientsidx++;

    // Informacja zwrotna
    strcpy(msg.text,"Pomyslnie dodano uzytkownika do listy zablokowanych.\n");
    msgsnd(sid,&msg,MESSAGE_SIZE,0);
    return 0;
}
int rcvUnblockClient(struct message fmsg){
    printf("Klient %ld wykonal polecenie UnblockClient\n",fmsg.pid);
    msg.receiver = fmsg.pid;
    msg.msgType = fmsg.msgType;
    msg.pid = fmsg.pid;
    char text[1024];
    strcpy(text,fmsg.text);

    int clientidx = findClientByPid(msg.pid);

    if(strlen(text)>=MAX_NICKNAME_SIZE){
        strcpy(msg.text,"Podana nazwa uzytkownika jest zbyt dluga.\n");
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
            strcpy(msg.text,"Nie ma takiego uzytkownika na liscie zablokowanych\n");
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
    strcpy(msg.text,"Pomyslnie usunieto uzytkownika z listy zablokowanych\n");
    msgsnd(sid,&msg,MESSAGE_SIZE,0);
    return 0;
}