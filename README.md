# Client and server program
Simple implamantation of chat. Users connects to the server (separate process) and then send requests and messages. Works only on Linux distributions. To communicate, both processes use single IPC message queue.

## Key features
Clients can:
- Send messages to other users
- Join or create groups
- Send messages to users in group
- See list of connected users
- See list of active groups
- Block and unblock other users to reject messages from them

## Message structure explanation
There are two structures, which are sent throught message queue.

`struct message{
  long receiver;
  long msgType;
  long pid;
  char text[1024];
}msg;`

`struct messageLonger{
    long receiver;
    long msgType;
    long pid;
    char text[4096];
}msgLonger;`

`msgLonger` is only used in read messages request and has longer char array. 

Because there is only one message queue for all the traffic, `long receiver` indicates the receiver of the message. Message to the server always has receiver set to 1. Messages from server has receiver set to PID of a client. To know who sent message to the server, structures also have `long pid` variable.

`msgType` is used to indicate request type.

`long pid` allows server to set correct `receiver` in the response and helps in finding client information on the server.

`char text[]` contains all the response text (if sent by server) or required information to proceed request (if sent by client).

## Gallery
<p align="center">
  &nbsp;&nbsp;
  <img src="https://github.com/Adrix0n/PPSem3/assets/99897531/b6f87d26-c089-495f-ae05-b55c9c459c97" alt="overview" width="1000">
  &nbsp;&nbsp;
  <img src="https://github.com/Adrix0n/PPSem3/assets/99897531/0ecbea1f-ab29-4cbe-8a81-75e65758fa94" alt="chat-history" width="600">
  &nbsp;&nbsp;
  <img src="https://github.com/Adrix0n/PPSem3/assets/99897531/535cbf18-e6cc-4198-b398-fe626e9d7ab9" alt="options" width="500">
  &nbsp;&nbsp;
</p>
