#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>

#define PORT 49200 // Server port
#define MESSAGE_SIZE 1024
#define MAX_FILE_SIZE 1048576

extern int errno;

typedef struct thrd{
	int tID; 
	int descriptor;
} thrd;

typedef struct portThrd{
    thrd *td;
    int port;
    int descriptor;
    char big_buffer[MAX_FILE_SIZE];
    char file[MESSAGE_SIZE];
} portThrd;

typedef struct recMessage{
    const char* message;
    int senderPort;
    int length;
} recMessage;

typedef struct sessionStructure{
    int port;
    int clients[200];
    char file[MESSAGE_SIZE];
    int descriptor;
    char big_buffer[MAX_FILE_SIZE];
} sessionStructure;

bool sessionAvailablePorts[100] = {0};
int clients[200] = {-1};
sessionStructure *sessions;
int clientSession[200] = {-1};
//int sessionClient[100][200];
int clientNumber = 0, sessionNumber = 0;
const char* allowedChars = "abcdefghijklmnopqrstuvwxyz0123456789-=,.;[]<>/?:{}()!@#$^&*";

//Utilitary functions
void writeToDestination(int descriptor, int senderPort, const char* message) {
    int len = strlen(message);
    //printf("%s\n", message);
    fflush(stdout);
    if(write(descriptor, &senderPort, sizeof(int)) < 0) {
        perror("Error at write.\n");
        exit(0);
    }
    if(write(descriptor, &len, sizeof(int)) < 0) {
        perror("Error at write.\n");
        exit(0);
    }
    if(write(descriptor, message, len) < 0) {
        perror("Error at write.\n");
        exit(0);
    }
}

static int sessionPort() {
    for(int i = 0; i < 100; ++i) {
        if(sessionAvailablePorts[i] == 0) {
            sessionAvailablePorts[i] = 1;
            return i + 1 + PORT;
        }
    }
    return -1;
}

void addClient(int id) {
    for(int i = 0; i < 200; ++i) {
        if(clients[i] == -1) {
            clients[i] = id;
        }
    }
    clientNumber++;
}

void assignClient(int descriptor, int port) {
    for(int i = 0; i < 100; ++i) {
        if(sessions[i].port == port) {
            for(int j = 0; j < 200; ++j) {
                if(sessions[i].clients[j] == -1) {
                    sessions[i].clients[j] = descriptor;
                    return;
                }
            }
        }
    }
}

void deassignClient(int descriptor, int port) {
    for(int i = 0; i < 100; ++i) {
        if(sessions[i].port == port) {
            for(int j = 0; j < 200; ++j) {
                if(sessions[i].clients[j] == descriptor) {
                    sessions[i].clients[j] = -1;
                    return;
                }
            }
        }
    }
}

void removeClient(int id) {
    for(int i = 0; i < 200; ++i) {
        if(clients[i] == id) {
            clients[i] = -1;
        }
    }
    clientNumber--;
}

void addSession(int id, const char* file) {
    int k = 0;
    for(int i = 0; i < 100; ++i) {
        if(sessions[i].port == -1) {
            sessions[i].port = id;
            strcpy(sessions[i].file, file);
            k = i;
            break;
        }
    }
    for(int i = 0; i < 200; ++i) {
        sessions[k].clients[i] = -1;
    }
    sessionNumber++;
}

void removeSession(int id) {
    for(int i = 0; i < 100; ++i) {
        if(sessions[i].port == id) {
            sessions[i].port = -1;
        }
    }
    sessionNumber--;
}


//Session functions
void moveToAll(int position, int portArg) {
    for(int i = 0; i < 100; ++i) {
        if(sessions[i].port == portArg) {
            for(int j = 0; j < 200; ++j) {
                if(sessions[i].clients[j] > 1) {
                    writeToDestination(sessions[i].clients[j], portArg, "Move.\n");
                }
            }
        }
    }
}

void writeToAll(char letter, int position, int portArg) {
    char message[MESSAGE_SIZE];

    memset(message, 0, sizeof(message));

    sprintf(message, "Write__%c__%d", letter, position);
    for(int i = 0; i < 100; ++i) {
        if(sessions[i].port == portArg) {
            for(int j = 0; j < 200; ++j) {
                if(sessions[i].clients[j] > 1) {
                    writeToDestination(sessions[i].clients[j], portArg, message);
                }
            }
        }
    }
}
void tryWrite(char* message, int portArg, void* arg) {
    char letter = message[7];
    char* numb = message + 10;
    int number = atoi(numb);
    if(letter == '\t') {
        letter = ' ';
    }
    //printf("User tried writing %c at position %d.\n", letter, number);
    //strchr(allowedChars, letter) != NULL && 
    if(number >= 0) {
        writeToAll(letter, number, portArg);
        usleep(10000);
        for(int i = 0; i < 100; ++i) {
            if(sessions[i].port == portArg) {
                strcpy(sessions[i].big_buffer + number + 1, sessions[i].big_buffer + number);
                sessions[i].big_buffer[number] = letter;
                //printf("%s\n", sessions[i].big_buffer);
                break;
            }
        }
        //moveToAll(number, portArg, clientDescriptors);
    }
}

void eraseToAll(int position, int portArg) {
    char message[MESSAGE_SIZE];

    memset(message, 0, sizeof(message));

    sprintf(message, "Erase__%d", position);
    for(int i = 0; i < 100; ++i) {
        if(sessions[i].port == portArg) {
            for(int j = 0; j < 200; ++j) {
                if(sessions[i].clients[j] > 1) {
                    writeToDestination(sessions[i].clients[j], portArg, message);
                }
            }
        }
    }
}
void tryErase(char* message, int portArg, void* arg) {
    char* numb = message + 7;
    int number = atoi(numb);
    //printf("User tried erasing at position %d.\n", number);

    if(number >= 0) {
        eraseToAll(number, portArg);
        usleep(10000);
        for(int i = 0; i < 100; ++i) {
            if(sessions[i].port == portArg) {
                strcpy(sessions[i].big_buffer + number - 1, sessions[i].big_buffer + number);
                //printf("%s\n", sessions[i].big_buffer);
                break;
            }
        }
        //printf("%s\n", (*((struct portThrd *)arg)).big_buffer);
        //moveToAll(number, portArg, clientDescriptors);
    }
}

void tryMove(char* message, int portArg) {
    char* numb = message + 7;
    int number = atoi(numb);
    //printf("Moved user's pointer to %d.\n", number);
}

static void *sessionHandle(void *arg) {
    int sessionP = (*((struct portThrd *)arg)).port;
    struct thrd tdL = *(*((struct portThrd *)arg)).td;
    int fileDesc = (*((struct portThrd *)arg)).descriptor;
    //int *clientDescriptors = (*((struct portThrd *)arg)).descriptors;
    fflush(stdout);
    pthread_detach(pthread_self());
    
    //if(write(tdL.descriptor, &tdL.tID, sizeof(int)) < 0) {
       // perror("Error at write.\n");
     //   exit(0);
   // }
    char buff[MAX_FILE_SIZE + 4];
    strcpy(buff, "Big");
    for(int i = 0; i < 100; ++i) {
        if(sessions[i].port == sessionP) {
            strcat(buff, sessions[i].big_buffer);
            break;
        }
    }
    
    writeToDestination(tdL.descriptor, sessionP, buff);

    char message[MESSAGE_SIZE];
    char senderChar[8];
    char respond[MESSAGE_SIZE];
    int len;
    int sender;

    while(1) {
        memset(message, 0, sizeof(message));
        memset(senderChar, 0, sizeof(senderChar));
        memset(respond, 0, sizeof(respond));

        fflush(stdout);

        if(read(tdL.descriptor, &sender, sizeof(int)) < 0) {
            perror("Error at read.\n");
            exit(0);
        }
        if(read(tdL.descriptor, &len, sizeof(int)) < 0) {
            perror("Error at read.\n");
            exit(0);
        }
        if(read(tdL.descriptor, message, len) < 0) {
            perror("Error at read.\n");
            exit(0);
        }

       // printf("%s\n", message);

        if(strncmp("Disconnect", message, 10) == 0) {
            writeToDestination(tdL.descriptor, sessionP, "Disconnect user from session.\n");
            deassignClient(tdL.descriptor, sessionP);
            close((intptr_t)arg);
            for(int i = 0; i < 100; ++i) {
                if(sessions[i].port == sessionP) {
                    bool ok = false;
                    for(int j = 0; j < 200; ++j) {
                        if(sessions[i].clients[j] > -1) {
                            ok = true;
                        }
                    }
                    if(!ok) {
                        removeSession(sessionP);
                        close(fileDesc);
                        exit(0);
                    }
                }
            }
            return(NULL);
        }
        else if(strncmp("Download", message, 8) == 0) {
            writeToDestination(tdL.descriptor, sessionP, "Download file for user.\n");
        }
        else if(strncmp("Write__", message, 7) == 0) {
            tryWrite(message, sessionP, arg);
            for(int i = 0; i < 100; ++i) {
                if(sessions[i].port == sessionP) {
                    close(sessions[i].descriptor);
                    fclose(fopen(sessions[i].file, "w"));
                    sessions[i].descriptor = open(sessions[i].file, O_RDWR | O_CREAT, 0777);

                    write(sessions[i].descriptor, sessions[i].big_buffer, strlen(sessions[i].big_buffer));
                    break;
                }
            }
        }
        else if(strncmp("Erase__", message, 7) == 0) {
            tryErase(message, sessionP, arg);
            for(int i = 0; i < 100; ++i) {
                if(sessions[i].port == sessionP) {
                    close(sessions[i].descriptor);
                    fclose(fopen(sessions[i].file, "w"));
                    sessions[i].descriptor = open(sessions[i].file, O_RDWR | O_CREAT, 0777);

                    write(sessions[i].descriptor, sessions[i].big_buffer, strlen(sessions[i].big_buffer));
                    break;
                }
            }
        }
        else if(strncmp("MoveP__", message, 7) == 0) {
            tryMove(message, sessionP);
        }
        else {
            writeToDestination(tdL.descriptor, sessionP, "Unrecognised command.\n");
        }
    }
} // Multithreading on Session level

void sessionMain(const char * doc, int portArg) {
    struct sockaddr_in session;
    struct sockaddr_in from;
    int sd, pid;//, fd;
    pthread_t threads[200];
    
    for(int i = 0; i < 100; ++i) {
        if(sessions[i].port == portArg) {
            sessions[i].descriptor = open(doc, O_RDWR | O_CREAT, 0777);
            strcpy(sessions[i].file, doc);
            memset(sessions[i].big_buffer, 0, sizeof(sessions[i].big_buffer));
            read(sessions[i].descriptor, sessions[i].big_buffer, MAX_FILE_SIZE);
            //fd = sessions[i].descriptor;
        }
    }
   // int clientDescriptors[200] = {-1};
    int crtTID = 0;
    int crtPort = portArg;

    if((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error at creating socket.\n");
        exit(0);
    }
    
    int on = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    bzero(&session, sizeof(session));
    bzero(&from, sizeof(from));
    
    session.sin_family = AF_INET;
    session.sin_addr.s_addr = htonl(INADDR_ANY);
    session.sin_port = htons(crtPort);

    if(bind (sd, (struct sockaddr *) &session, sizeof(struct sockaddr)) == -1) {
        perror("Error at binding.\n");
        exit(0);
    }
    if(listen (sd, 10) == -1) {
        perror("Error at listening.\n");
        exit(0);
    }
    while(1) {
        int client;
        thrd *td;
        portThrd *pTd;
        int len = sizeof(from);
        

        if((client = accept (sd, (struct sockaddr *) &from, &len)) < 0) {
            perror("Error at accept.\n");
            exit(0);
        }
        pTd = (struct portThrd *) malloc(sizeof(struct portThrd));
        td = (struct thrd *) malloc(sizeof(struct thrd));
       // clientDescriptors[crtTID] = client;
        
        td->tID = crtTID++;
        td->descriptor = client;
        pTd->td = td;
        pTd->port = crtPort;
        //pTd->descriptor = fd;
        //strcpy(pTd->file, doc);
       // pTd->descriptors = clientDescriptors;
        assignClient(client, crtPort);

        pthread_create(&threads[crtTID], NULL, &sessionHandle, pTd);
    }
}


//Server functions
bool tryCreateDocument(char* message) {
    const char* docname = message + 13;
    return true;
}

int tryJoinSession(const char* message) {
    const char* prt = message + 5;
    int port = atoi(prt);
    for(int i = 0; i < 100; ++i) {
        if(sessions[i].port == port) {
            return port;
        }
    }
    return -1;
}

int tryNewSession(const char* message) {
    const char* docname = message + 12;
    int newPort = sessionPort();
    addSession(newPort, docname);
    
    int pid = 0;
    if(-1 == (pid = fork())) {
        perror("Error at fork.\n");
        exit(0);
    }
    if(pid == 0) {
        sessionMain(docname, newPort);
        exit(0);
    }
    else {
        sleep(1);
        return newPort;
    }
}

void sendSessions(int descriptor, int senderPort) {
    char message[MESSAGE_SIZE];
    strcpy(message, "Sessions");
    for(int i = 0; i < 100; ++i) {
        if(sessions[i].port > -1) {
            strcat(message, ":");

            int numb = 0;
            for(int j = 0; j < 200; ++j) {
                if(sessions[i].clients[j] > -1) {
                    ++numb;
                }
            }
            char aux[MESSAGE_SIZE];
            sprintf(aux, "%d:%d:", sessions[i].port, numb);
            strcat(message, aux);
            strcat(message, sessions[i].file);
        }
    }
    writeToDestination(descriptor, senderPort, message);
}

static void *serverHandle(void *arg){
    int serverPort = PORT;
    struct thrd tdL;
    tdL = *((struct thrd *)arg);
    fflush(stdout);
    pthread_detach(pthread_self());
    
    addClient(tdL.tID);

    if(write(tdL.descriptor, &tdL.tID, sizeof(int)) < 0) {
        perror("Error at write.\n");
        exit(0);
    }

    char message[MESSAGE_SIZE];
    char senderChar[8];
    char respond[MESSAGE_SIZE];
    int len;
    int sender;

    while(1) {
        memset(message, 0, sizeof(message));
        memset(senderChar, 0, sizeof(senderChar));
        memset(respond, 0, sizeof(respond));

        fflush(stdout);
        
        if(read(tdL.descriptor, &sender, sizeof(int)) < 0) {
            perror("Error at read.\n");
            exit(0);
        }
        if(read(tdL.descriptor, &len, sizeof(int)) < 0) {
            perror("Error at read.\n");
            exit(0);
        }
        if(read(tdL.descriptor, message, len) < 0) {
            perror("Error at read.\n");
            exit(0);
        }
        if(strncmp("Disconnect", message, 10) == 0) {
            writeToDestination(tdL.descriptor, PORT, "Disconnected from server.\n");
            removeClient(sender);
            close((intptr_t)arg);
            return(NULL);
        }
        else if(strncmp("New Document ", message, 13) == 0) {
            bool created = tryCreateDocument(message);
            writeToDestination(tdL.descriptor, PORT, created ? "Document created.\n" : "Document not created.\n");
        }
        else if(strncmp("Join ", message, 5) == 0) {
            int portR = tryJoinSession(message);
            sprintf(respond, "%d", portR);
            writeToDestination(tdL.descriptor, PORT, respond);
        }
        else if(strncmp("New Session ", message, 12) == 0) {
            int portR = tryNewSession(message);
            sprintf(respond, "%d", portR);
            writeToDestination(tdL.descriptor, PORT, respond);
        }
        else if(strncmp("Refresh", message, 7) == 0) {
            sendSessions(tdL.descriptor, PORT);
        }
        else {
            writeToDestination(tdL.descriptor, PORT, "Unrecognised command.\n");
        }
    }

} // Multithreading on Server level

void init() {
    sessions = mmap(NULL, sizeof(sessionStructure) * 100, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    for(int i = 0; i < 100; ++i) {
        strcpy(sessions[i].file, "-1");
        sessions[i].port = -1;
        for(int j = 0; j < 200; ++j) {
            sessions[i].clients[j] = -1;
        }
    }
}

int main() {
    struct sockaddr_in server;
    struct sockaddr_in from;
    int sd, pid;
    pthread_t threads[200];
    int crtTID = 0;

    init();

    if((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error at creating socket.\n");
        exit(0);
    }

    int on = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    bzero(&server, sizeof(server));
    bzero(&from, sizeof(from));
    
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    if(bind (sd, (struct sockaddr *) &server, sizeof(struct sockaddr)) == -1) {
        perror("Error at binding.\n");
        exit(0);
    }
    if(listen (sd, 10) == -1) {
        perror("Error at listening.\n");
        exit(0);
    }
    while(1) {
        int client;
        thrd *td;
        int len = sizeof(from);

        if((client = accept (sd, (struct sockaddr *) &from, &len)) < 0) {
            perror("Error at accept.\n");
            exit(0);
        }
        
        td = (struct thrd *) malloc(sizeof(struct thrd));
        td->tID = crtTID++;
        td->descriptor = client;

        pthread_create(&threads[crtTID], NULL, &serverHandle, td);
    }
}