#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <stdbool.h>
#include <gtk/gtk.h>

#define SERVER_PORT 49200
#define SERVER_ADDRESS "127.0.0.1"
#define MESSAGE_SIZE 1024
#define MAX_FILE_SIZE 1048576

extern int errno;

typedef struct sessionButton{
    GtkWidget *button;
    char filename[MESSAGE_SIZE];
    int id;
    int users;
} sessionButton;

int sessionPort = -2;
int ID = 0;
sessionButton buttons[128];
int numberOfSessions = -2;
int sd, sed;

struct sockaddr_in server;
struct sockaddr_in session;

fd_set readfds;
fd_set actfds;
int fd;
int nfds;

static int *positionInsert, *insertSize, *positionDelete, *deleteSize, *phase;
static char *charactersInsert;

GtkApplication *app;
GtkWidget *windowSessions, *windowOptions, *windowEditor;
GtkWidget* dialog;

char *big_buffer;
bool *init_big_buffer;
char filename[MESSAGE_SIZE];

void createPhaseOne(GtkApplication *app, gpointer data);
void createPhaseTwo(GtkApplication *app, gpointer data);

void listenToServer(int fileDescriptor) {
    while(1) {
        char received[MESSAGE_SIZE];
        int len;
        int sender;

        memset(received, 0, sizeof(received));

        if(read(fileDescriptor, &sender, sizeof(int)) < 0) {
            perror("Error at read.\n");
            exit(0);
        }
        if(read(fileDescriptor, &len, sizeof(int)) < 0) {
            perror("Error at read.\n");
            exit(0);
        }
        if(read(fileDescriptor, received, len) < 0) {
            perror("Error at read.\n");
            exit(0);
        }
        if(strncmp(received, "Big", 3) == 0) {
            strcpy(big_buffer, received + 3);
            *init_big_buffer = true;
        }
        else if(strncmp(received, "Erase", 5) == 0) {
            int position = atoi(received + 7);
            positionDelete[*deleteSize] = position;
            *deleteSize = *deleteSize + 1;
        }
        else if(strncmp(received, "Write", 5) == 0) {
            char letter = received[7];
            //printf("%c <- \n", letter);
            int position = atoi(received + 10);
            positionInsert[*insertSize] = position;
            charactersInsert[*insertSize] = letter;
            *insertSize = *insertSize + 1;
        }
        else if(strncmp(received, "Disconnect", 10) == 0) {
            exit(1);
        }
        else {
            printf("What: %s\n", received);
        }
    }
}

void writeToDestination(int descriptor, int senderPort, const char* message) {
    int len = strlen(message);
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

void askJoin(int id) {
    //printf("%d\n", id);
    int receivedPort = buttons[id].id;
    strcpy(filename, buttons[id].filename);

    if(receivedPort < 0) {
        printf("Invalid port: %d.\n", receivedPort);
    }
    else {
        sessionPort = receivedPort;
        if((sed = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("Error at socket.\n");
            exit(0);
        }

        session.sin_family = AF_INET;
        session.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
        session.sin_port = htons(sessionPort);

        if(connect(sed, (struct sockaddr *)&session, sizeof(struct sockaddr)) == -1) {
            perror("Error at connect.\n");
            exit(0);
        }

        *phase = 2;
        gtk_widget_destroy(windowSessions);
        int pid;
        if(-1 == (pid = fork())) {
            perror("Error at fork.\n");
        }
        if(pid == 0) {
            createPhaseTwo(app, NULL);
        }
        else {
            listenToServer(sed);
        }
    }
}

void askNewSession() {
    char message[MESSAGE_SIZE];
    char received[MESSAGE_SIZE];
    int len;
    int sender;

    memset(message, 0, sizeof(message));
    memset(received, 0, sizeof(received));


    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;

    dialog = gtk_file_chooser_dialog_new("Choose file", windowSessions, action, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
    g_signal_connect_swapped(dialog, "delete-event", G_CALLBACK(gtk_widget_hide), dialog);
    gint res = gtk_dialog_run (GTK_DIALOG (dialog));

    if(res == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
        strcpy(filename, gtk_file_chooser_get_filename (chooser));
        char *p = strrchr (filename, '.');
        gtk_widget_hide (dialog);
        if(strncmp (p, ".txt", 4) != 0) {
            return;
        }
    }
    else {
        gtk_widget_hide (dialog);
        return;
    }


    strcpy(message, "New Session ");
    strcat(message, filename);
    //printf("%s\n", message);
    writeToDestination(sd, ID, message);

    if(read(sd, &sender, sizeof(int)) < 0) {
        perror("Error at read.\n");
        exit(0);
    }
    if(read(sd, &len, sizeof(int)) < 0) {
        perror("Error at read.\n");
        exit(0);
    }
    if(read(sd, received, len) < 0) {
        perror("Error at read.\n");
        exit(0);
    }

    int receivedPort = atoi(received);
    if(receivedPort < 0) {
        printf("Invalid port: %d.\n", receivedPort);
    }
    else {
        sessionPort = receivedPort;
        if((sed = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("Error at socket.\n");
            exit(0);
        }

        session.sin_family = AF_INET;
        session.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
        session.sin_port = htons(sessionPort);

        if(connect(sed, (struct sockaddr *)&session, sizeof(struct sockaddr)) == -1) {
            perror("Error at connect.\n");
            exit(0);
        }

        *phase = 2;
        gtk_widget_destroy(windowSessions);
        int pid;
        if(-1 == (pid = fork())) {
            perror("Error at fork.\n");
        }
        if(pid == 0) {
            createPhaseTwo(app, NULL);
        }
        else {
            listenToServer(sed);
        }
    }
}

void askDisconnectServer() {
    char message[MESSAGE_SIZE];
    char received[MESSAGE_SIZE];
    int len;
    int sender;

    memset(message, 0, sizeof(message));
    memset(received, 0, sizeof(received));

    strcpy(message, "Disconnect");

    writeToDestination(sd, ID, message);

    if(read(sd, &sender, sizeof(int)) < 0) {
        perror("Error at read.\n");
        exit(0);
    }
    if(read(sd, &len, sizeof(int)) < 0) {
        perror("Error at read.\n");
        exit(0);
    }
    if(read(sd, received, len) < 0) {
        perror("Error at read.\n");
        exit(0);
    }

    gtk_widget_destroy(dialog);
    gtk_widget_destroy(windowSessions);

    close(sd);
    exit(0);
}

void askNewDoc() {
    char message[MESSAGE_SIZE];
    char received[MESSAGE_SIZE];
    int len;
    int sender;

    memset(message, 0, sizeof(message));
    memset(received, 0, sizeof(received));

    strcpy(message, "New Document testnewdoc");

    writeToDestination(sd, ID, message);

    if(read(sd, &sender, sizeof(int)) < 0) {
        perror("Error at read.\n");
        exit(0);
    }
    if(read(sd, &len, sizeof(int)) < 0) {
        perror("Error at read.\n");
        exit(0);
    }
    if(read(sd, received, len) < 0) {
        perror("Error at read.\n");
        exit(0);
    }

    //printf("%s", received);
}

void populateSessions(char response[MESSAGE_SIZE]) {
    char *p = strtok(response, ":");
    p = strtok(NULL, ":");
    numberOfSessions = -1;
    while(p != NULL) {
        int id = atoi(p);

        p = strtok(NULL, ":");
        int usrs = atoi(p);

        p  = strtok(NULL, ":");
        buttons[++numberOfSessions].id = id;
        buttons[numberOfSessions].users = usrs;
        strcpy(buttons[numberOfSessions].filename, p);

        p = strtok(NULL, ":");
    }
}

void refreshButtons(GtkWidget *container) {
    GList *children, *iter;

    children = gtk_container_get_children(GTK_CONTAINER (container));
    bool once = true;
    for(iter = children; iter != NULL; iter = g_list_next (iter)) {
        if(once) {
            iter = g_list_next (iter);
            once = false;
            if(iter == NULL) {
                break;
            }
        }
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free (children);

    for(int i = 0; i <= numberOfSessions; ++i) {
        GtkWidget *button;

        char buttonText[MESSAGE_SIZE * 2];
        
        if(buttons[i].users != 1) {
            sprintf(buttonText, "Session %d\n%d connected users\n%s", buttons[i].id, buttons[i].users, buttons[i].filename);
        }
        else {
            sprintf(buttonText, "Session %d\n%d connected user\n%s", buttons[i].id, buttons[i].users, buttons[i].filename);
        }

        button = gtk_button_new_with_label(buttonText);
        g_signal_connect_swapped (button, "clicked", G_CALLBACK(askJoin), i);

        buttons[i].button = button;

        gtk_container_add(GTK_CONTAINER(container), buttons[i].button);
        gtk_widget_show(GTK_WIDGET(buttons[i].button));
    }
}

void askRefresh(GtkWidget *container) {
    char message[MESSAGE_SIZE];
    char received[MESSAGE_SIZE];
    int len;
    int sender;

    memset(message, 0, sizeof(message));
    memset(received, 0, sizeof(received));

    strcpy(message, "Refresh");
    writeToDestination(sd, ID, message);

    if(read(sd, &sender, sizeof(int)) < 0) {
        perror("Error at read.\n");
        exit(0);
    }
    if(read(sd, &len, sizeof(int)) < 0) {
        perror("Error at read.\n");
        exit(0);
    }
    if(read(sd, received, len) < 0) {
        perror("Error at read.\n");
        exit(0);
    }

    populateSessions(received);
    refreshButtons(container);
}

void askDisconnectSession() {
    *phase = 1;
    char message[MESSAGE_SIZE];
    char received[MESSAGE_SIZE];
    int len;
    int sender;

    memset(message, 0, sizeof(message));
    memset(received, 0, sizeof(received));

    strcpy(message, "Disconnect");

    writeToDestination(sed, ID, message);
/*
    if(read(sed, &sender, sizeof(int)) < 0) {
        perror("Error at read.\n");
        exit(0);
    }
    if(read(sed, &len, sizeof(int)) < 0) {
        perror("Error at read.\n");
        exit(0);
    }
    if(read(sed, received, len) < 0) {
        perror("Error at read.\n");
        exit(0);
    }
*/
    createPhaseOne(app, NULL);
    gtk_widget_destroy(windowOptions);
    gtk_widget_destroy(windowEditor);
}

void askDownload(GtkTextBuffer *buf) {
    GtkTextIter st, fn;
    gtk_text_buffer_get_start_iter(buf, &st);
    gtk_text_buffer_get_end_iter(buf, &fn);
    char buff[MAX_FILE_SIZE];
    strcpy(buff, gtk_text_buffer_get_text(buf, &st, &fn, FALSE));

    char filenameSave[MESSAGE_SIZE], *p;
    p = strrchr(filename, '/');
    sprintf(filenameSave, "Save-%d-%s", ID, p + 1);
    
    int dwld = open(filenameSave, O_RDWR | O_CREAT, 0777);
    write(dwld, buff, strlen(buff));
    close(dwld);
}

void askWrite(char c, int position) {
    char message[MESSAGE_SIZE];

    memset(message, 0, sizeof(message));

    sprintf(message, "Write__%c__%d", c, position);

    writeToDestination(sed, ID, message);
}

void askErase(bool forward, int position) {
    if(forward) {
        position++;
    }
    char message[MESSAGE_SIZE];

    memset(message, 0, sizeof(message));

    sprintf(message, "Erase__%d", position);

    writeToDestination(sed, ID, message);
}

void askCursor(GtkWidget *widget, gpointer data) {

}

void askKey(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    gint pos;
    GtkTextIter it, it2;

    g_object_get((GtkTextBuffer*)data, "cursor-position", &pos, NULL);
   // printf("%d ", pos);

    gtk_text_buffer_get_iter_at_offset ((GtkTextBuffer*)data, &it, pos);

    if(strncmp("BackSpace", gdk_keyval_name(event->keyval), 9) == 0) {
        askErase(false, pos);
    }
    else if(strncmp("Delete", gdk_keyval_name(event->keyval), 6) == 0) {
        askErase(true, pos);
    }
    else if(strncmp("Right", gdk_keyval_name(event->keyval), 5) == 0) {
    //    gtk_text_buffer_get_iter_at_offset ((GtkTextBuffer*)data, &it2, pos + 1);
     //   gtk_text_buffer_place_cursor((GtkTextBuffer*)data, &it); ???
    }
    else if(strncmp("Left", gdk_keyval_name(event->keyval), 4) == 0) {
       // gtk_text_buffer_get_iter_at_offset ((GtkTextBuffer*)data, &it2, pos - 1);
       // if(pos > 0) {
           // gtk_text_buffer_place_cursor((GtkTextBuffer*)data, &it); ???
      //  }
    }
    else if(strncmp("Return", gdk_keyval_name(event->keyval), 6) == 0) {
        askWrite('\n', pos);
    }
    else if(strncmp("Tab", gdk_keyval_name(event->keyval), 3) == 0) {
        askWrite('\t', pos);
    }
    else if(strncmp("space", gdk_keyval_name(event->keyval), 5) == 0) {
        askWrite(' ', pos);
    }
    else {
        char c = gdk_keyval_to_unicode(event->keyval);
        if(c != 0) {
            askWrite(c, pos);
        }
    }
    //if(strlen(gdk_keyval_name(event->keyval)) == 1) {
    //    gtk_text_buffer_insert((GtkTextBuffer*)data, &it, gdk_keyval_name(event->keyval), 1);
    //}


    //gtk_text_buffer_insert_at_cursor((GtkTextBuffer*)data, gdk_keyval_name(event->keyval), 1);
    //printf("%s -> %c", gdk_keyval_name(event->keyval), gdk_keyval_to_unicode(event->keyval));
    g_object_get((GtkTextBuffer*)data, "cursor-position", &pos, NULL);
    //printf("%d\n", pos);
}

gboolean verifyChanges(gpointer data) {
    if(*phase == 1) {
        return FALSE;
    }
    GtkTextIter it, it2;
    char letter;
    gint position;
    while(*deleteSize > 0) {
        position = positionDelete[*deleteSize - 1];
        gtk_text_buffer_get_iter_at_offset ((GtkTextBuffer*)data, &it, position);
        gtk_text_buffer_get_iter_at_offset ((GtkTextBuffer*)data, &it2, position - 1);
        gtk_text_buffer_delete((GtkTextBuffer*)data, &it, &it2);
        *deleteSize = *deleteSize - 1;
    }
    while(*insertSize > 0) {
        position = positionInsert[*insertSize - 1];
        letter = charactersInsert[*insertSize - 1];
        gtk_text_buffer_get_iter_at_offset ((GtkTextBuffer*)data, &it, position);
        gtk_text_buffer_insert ((GtkTextBuffer*)data, &it, &letter, 1);
        *insertSize = *insertSize - 1;
    }
    return TRUE;
}

void createPhaseOne(GtkApplication *app, gpointer data) {
    GtkWidget *refresh, *newSession, *disconnect;
    GtkWidget *buttonsContainer, *othersContainer;

    windowSessions = gtk_application_window_new (app);
    gtk_window_set_title (GTK_WINDOW (windowSessions), "Collaborative Notepad - Sessions");
    gtk_window_set_default_size (GTK_WINDOW (windowSessions), 256, 512);

    buttonsContainer = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
    gtk_container_add(GTK_CONTAINER (windowSessions), buttonsContainer);

    othersContainer = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_container_add(GTK_CONTAINER (buttonsContainer), othersContainer);

    refresh = gtk_button_new_with_label("Refresh");
    g_signal_connect_swapped (refresh, "clicked", G_CALLBACK(askRefresh), buttonsContainer);
    gtk_container_add(GTK_CONTAINER (othersContainer), refresh);

    newSession = gtk_button_new_with_label("New Session");
    g_signal_connect_swapped (newSession, "clicked", G_CALLBACK(askNewSession), NULL);
    gtk_container_add(GTK_CONTAINER (othersContainer), newSession);

    disconnect = gtk_button_new_with_label("Disconnect");
    g_signal_connect_swapped (disconnect, "clicked", G_CALLBACK(askDisconnectServer), NULL);
    gtk_container_add(GTK_CONTAINER (othersContainer), disconnect);

    gtk_widget_show_all (windowSessions);

    *init_big_buffer = false;
    strcpy (big_buffer, "");
}

void createPhaseTwo(GtkApplication *app, gpointer data) {
    //writing
    GtkWidget *view;
    GtkTextBuffer *buffer;

    windowEditor = gtk_application_window_new (app);
    gtk_window_set_title (GTK_WINDOW (windowEditor), "Collaborative Notepad - Editor");
    gtk_window_set_default_size (GTK_WINDOW (windowEditor), 512, 512);

    while(!(*init_big_buffer)) {
        sleep(1);
    }

    view = gtk_text_view_new();
    buffer = gtk_text_view_get_buffer (view);
    gtk_text_buffer_set_text(buffer, big_buffer, -1);
    gtk_text_view_set_editable(GTK_TEXT_VIEW (view), FALSE);
    gtk_widget_add_events(windowEditor, GDK_KEY_PRESS_MASK);
    gtk_container_add(GTK_CONTAINER (windowEditor), view);
    g_signal_connect(buffer, "notify", G_CALLBACK(askCursor), buffer);
    g_signal_connect(windowEditor, "key_press_event", G_CALLBACK(askKey), buffer);
    g_timeout_add(1, verifyChanges, buffer);

    gtk_widget_show_all (windowEditor);
    //options menu
    GtkWidget *save, *disconnect;
    GtkWidget *othersContainer;

    windowOptions = gtk_application_window_new (app);
    gtk_window_set_title (GTK_WINDOW (windowOptions), "Collaborative Notepad - Options");
    gtk_window_set_default_size (GTK_WINDOW (windowOptions), 256, 256);

    othersContainer = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_container_add(GTK_CONTAINER (windowOptions), othersContainer);


    save = gtk_button_new_with_label("Save file");
    g_signal_connect_swapped (save, "clicked", G_CALLBACK(askDownload), buffer);
    gtk_container_add(GTK_CONTAINER (othersContainer), save);

    disconnect = gtk_button_new_with_label("Disconnect");
    g_signal_connect_swapped (disconnect, "clicked", G_CALLBACK(askDisconnectSession), NULL);
    gtk_container_add(GTK_CONTAINER (othersContainer), disconnect);

    gtk_widget_show_all(windowOptions);

}

void init() {
    positionInsert = mmap(NULL, sizeof(int) * 64, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    insertSize = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    charactersInsert = mmap(NULL, sizeof(char) * 64, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    positionDelete = mmap(NULL, sizeof(int) * 64, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    deleteSize = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    phase = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *insertSize = 0;
    *deleteSize = 0;
    *phase = 1;

    init_big_buffer = mmap(NULL, sizeof(bool), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    big_buffer = mmap(NULL, sizeof(char) * MAX_FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    strcpy(big_buffer, "");
    *init_big_buffer = false;
}

int main(int argc, char *argv[]) {

    init();

    if((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error at socket.\n");
        exit(0);
    }

    char serverAddress[16];
    if(argc == 1) {
        strcpy(serverAddress, SERVER_ADDRESS);
    }
    else if(argc == 2) {
        strcpy(serverAddress, argv[1]);
        argc = 1;
        argv[1] = NULL;
    }
    else {
        printf("Bad call. Use: ./client [<server address>]\n");
        exit(1);
    }
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(serverAddress);
    server.sin_port = htons(SERVER_PORT);

    if(connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1) {
        perror("Error at connect.\n");
        exit(0);
    }
    if(read(sd, &ID, sizeof(int)) < 0) {
        perror("Error at read.\n");
        exit(0);
    }
    
    int status;
    char appid[32];
    sprintf(appid, "client.collaborative.notepad%d", ID);

    app = gtk_application_new (appid, G_APPLICATION_FLAGS_NONE);
    g_signal_connect (app, "activate", G_CALLBACK (createPhaseOne), NULL);
    status = g_application_run (G_APPLICATION (app), argc, argv);
    g_object_unref (app);
    



    /*
    char message[MESSAGE_SIZE];
    char received[MESSAGE_SIZE];
    int len;
    int sender;

    while(1) {
        fflush(stdout);
        memset(message, 0, sizeof(message));
        memset(received, 0, sizeof(received));

        if(sessionPort < 0) {
            //if(read(0, message, sizeof(message)) < 0) {
            //    perror("Error at read.\n");
            //    exit(0);
            //}
            
            writeToDestination(sd, ID, message);

            if(read(sd, &sender, sizeof(int)) < 0) {
                perror("Error at read.\n");
                exit(0);
            }
            if(read(sd, &len, sizeof(int)) < 0) {
                perror("Error at read.\n");
                exit(0);
            }
            if(read(sd, received, len) < 0) {
                perror("Error at read.\n");
                exit(0);
            }

            if(strncmp("Disconnected", received, 12) == 0) {
                printf("Disconnected.\n");
                close(sd);
                return 0;
            }
            else if(strncmp("Document", received, 8) == 0) {
                printf("%s", received);
            }
            else if(strncmp("Unrecognised", received, 12) == 0) {
                printf("%s", received);
            }
            else if(strncmp("Sessions", received, 8) == 0) {
                populateSessions(received);
            }
            else {
                int receivedPort = atoi(received);
                if(receivedPort < 0) {
                    printf("Invalid port: %d.\n", receivedPort);
                }
                else {
                    sessionPort = receivedPort;
                    if((sed = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
                        perror("Error at socket.\n");
                        exit(0);
                    }

                    session.sin_family = AF_INET;
                    session.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
                    session.sin_port = htons(sessionPort);

                    if(connect(sed, (struct sockaddr *)&session, sizeof(struct sockaddr)) == -1) {
                        perror("Error at connect.\n");
                        exit(0);
                    }

                    FD_ZERO (&actfds);
                    FD_SET (0, &actfds);
                    FD_SET (sed, &actfds);
                    nfds = sed;
                }
            }
        }
        else {
            bcopy((char* ) &actfds, (char* ) &readfds, sizeof(readfds));

            if(select(nfds + 1, &readfds, NULL, NULL, NULL) < 0) {
                perror("Error at select.\n");
                exit(0);
            }
            for(fd = 0; fd <= nfds; ++fd) {
                if(FD_ISSET(fd, &readfds)) {
                    if(fd == 0) { //keyboard
                        if(read(0, message, sizeof(message)) < 0) {
                            perror("Error at read.\n");
                            exit(0);
                        }
                        if(strlen(message) < 3) {
                            char c[MESSAGE_SIZE];
                            strcpy(c, message);
                            strcpy(message, "Write__");
                            strncat(message, c, 1);
                            strcat(message, "__");
                            strcat(message, "0");
                            writeToDestination(sed, ID, message);
                        }
                        else {
                            writeToDestination(sed, ID, message);
                        }
                    }
                    else { //session
                        if(read(fd, &sender, sizeof(int)) < 0) {
                            perror("Error at read.\n");
                            exit(0);
                        }
                        if(read(fd, &len, sizeof(int)) < 0) {
                            perror("Error at read.\n");
                            exit(0);
                        }
                        if(read(fd, received, len) < 0) {
                            perror("Error at read.\n");
                            exit(0);
                        }

                        printf("%d said %s", sender, received);

                        if(strncmp("Disconnect", received, 10) == 0) {
                            sessionPort = -2;
                        }
                    }
                }
            }
        }
        fflush(stdout);
    }*/

}