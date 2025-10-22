#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
// ------------------------------[INIT]--------------------

#define PORT 8888
#define BUFFER_SIZE 1024

typedef struct {
    GtkWidget *window;
    GtkWidget *text_view;
    GtkWidget *entry;
    GtkWidget *send_button;
    GtkWidget *exit_button;
    GtkWidget *room_a_button;
    GtkWidget *room_b_button;
    GtkWidget *room_c_button;
    GtkWidget *current_room_label;
    GtkTextBuffer *buffer;
    int sock;
    int connected;
    char current_room[20];
    pthread_t recv_thread;
} AppWidgets;

typedef struct {
    AppWidgets *app;
    char *message;
} MessageData;

// ------------------------------[END INIT]--------------------

gboolean append_text_idle(gpointer user_data) {
    MessageData *data = (MessageData*)user_data;
    AppWidgets *app = data->app;
    
    if(app->buffer && GTK_IS_TEXT_BUFFER(app->buffer)) {
        GtkTextIter iter;
        gtk_text_buffer_get_end_iter(app->buffer, &iter);
        gtk_text_buffer_insert(app->buffer, &iter, data->message, -1);
        
        // Auto-scroll to bottom
        GtkTextMark *mark = gtk_text_buffer_get_insert(app->buffer);
        gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(app->text_view), mark, 0.0, TRUE, 0.0, 1.0);
    }
    
    g_free(data->message);
    g_free(data);
    return G_SOURCE_REMOVE;
}

void append_text(AppWidgets *app, const char *text) {
    MessageData *data = g_new(MessageData, 1);
    data->app = app;
    data->message = g_strdup(text);
    g_idle_add(append_text_idle, data);
}

void update_room_label(AppWidgets *app) {
    char label_text[100];
    if(strlen(app->current_room) > 0) {
        snprintf(label_text, 100, "Current Room: %s", app->current_room);
    } else {
        snprintf(label_text, 100, "Current Room: None (Please select a room)");
    }
    gtk_label_set_text(GTK_LABEL(app->current_room_label), label_text);
}

void* receive_messages(void* arg) {
    AppWidgets *app = (AppWidgets*)arg;
    char buffer[BUFFER_SIZE];
    
    while(app->connected) {
        memset(buffer, 0, BUFFER_SIZE);
        int read_size = recv(app->sock, buffer, BUFFER_SIZE - 1, 0);
        
        if(read_size <= 0) {
            app->connected = 0;
            append_text(app, "\n[Disconnected from server]\n");
            sleep(1);
            g_idle_add((GSourceFunc)gtk_main_quit, NULL);
            break;
        }
        
        buffer[read_size] = '\0';
        
        // Check for "Server is full!" message
        if(strstr(buffer, "Server is full!") != NULL) {
            append_text(app, buffer);
            app->connected = 0;
            sleep(2);
            g_idle_add((GSourceFunc)gtk_main_quit, NULL);
            break;
        }
        
        // Check if joined a room
        if(strstr(buffer, "You joined Room") != NULL) {
            if(strstr(buffer, "RoomA") != NULL) {
                strcpy(app->current_room, "RoomA");
            } else if(strstr(buffer, "RoomB") != NULL) {
                strcpy(app->current_room, "RoomB");
            } else if(strstr(buffer, "RoomC") != NULL) {
                strcpy(app->current_room, "RoomC");
            }
            g_idle_add((GSourceFunc)update_room_label, app);
        }
        
        append_text(app, buffer);
    }
    
    return NULL;
}

void send_to_server(AppWidgets *app, const char *message) {
    if(!app->connected) {
        return;
    }
    
    if(send(app->sock, message, strlen(message), 0) < 0) {
        append_text(app, "Failed to send message\n");
    }
}

void send_message(GtkWidget *widget, AppWidgets *app) {
    const char *text = gtk_entry_get_text(GTK_ENTRY(app->entry));
    
    if(strlen(text) == 0 || !app->connected) {
        return;
    }
    
    char message[BUFFER_SIZE];
    snprintf(message, BUFFER_SIZE, "%s", text);
    
    if(send(app->sock, message, strlen(message), 0) < 0) {
        append_text(app, "Failed to send message\n");
        return;
    }
    
    // Show own message for EXIT and room changes
    if(strcmp(message, "EXIT!") == 0) {
        append_text(app, "Disconnecting...\n");
        app->connected = 0;
        sleep(1);
        gtk_main_quit();
    }
    else if(strlen(message) == 1 && (message[0] == 'A' || message[0] == 'B' || message[0] == 'C')) {
        char info[100];
        snprintf(info, 100, "Joining Room%c...\n", message[0]);
        append_text(app, info);
    }
    else {
        // Echo own message (limit to prevent truncation)
        char echo[BUFFER_SIZE];
        message[BUFFER_SIZE - 20] = '\0';
        snprintf(echo, BUFFER_SIZE, "[You]: %s\n", message);
        append_text(app, echo);
    }
    
    gtk_entry_set_text(GTK_ENTRY(app->entry), "");
}

void on_room_button_clicked(GtkWidget *widget, gpointer data) {
    AppWidgets *app = (AppWidgets*)data;
    const char *button_label = gtk_button_get_label(GTK_BUTTON(widget));
    
    char room_char;
    if(strcmp(button_label, "Room A") == 0) {
        room_char = 'A';
    } else if(strcmp(button_label, "Room B") == 0) {
        room_char = 'B';
    } else if(strcmp(button_label, "Room C") == 0) {
        room_char = 'C';
    } else {
        return;
    }
    
    char message[2] = {room_char, '\0'};
    send_to_server(app, message);
    
    char info[100];
    snprintf(info, 100, "Switching to Room%c...\n", room_char);
    append_text(app, info);
}

void on_exit_button_clicked(GtkWidget *widget, gpointer data) {
    AppWidgets *app = (AppWidgets*)data;
    
    if(app->connected) {
        send_to_server(app, "EXIT!");
        append_text(app, "Disconnecting...\n");
        app->connected = 0;
        sleep(1);
    }
    gtk_main_quit();
}

gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, AppWidgets *app) {
    if(event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        send_message(widget, app);
        return TRUE;
    }
    return FALSE;
}

void on_window_destroy(GtkWidget *widget, AppWidgets *app) {
    if(app->connected) {
        app->connected = 0;
        close(app->sock);
    }
    gtk_main_quit();
}

int main(int argc, char *argv[]) {
    AppWidgets *app = g_new0(AppWidgets, 1);
    strcpy(app->current_room, "");
    
    // Connect to server
    struct sockaddr_in server_addr;
    
    app->sock = socket(AF_INET, SOCK_STREAM, 0);
    if(app->sock < 0) {
        fprintf(stderr, "Socket creation failed\n");
        return 1;
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    const char *server_ip = (argc > 1) ? argv[1] : "127.0.0.1";
    
    if(inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address\n");
        return 1;
    }
    
    if(connect(app->sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "Connection failed\n");
        return 1;
    }
    
    app->connected = 1;
    
    // ---------------------------[GTK GUI INIT]--------------------------------
    gtk_init(&argc, &argv);
    
    // Create window
    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), "TCP Echo Client");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 600, 450);
    g_signal_connect(app->window, "destroy", G_CALLBACK(on_window_destroy), app);
    
    // Create vertical box
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(app->window), vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    
    // Create room selection area
    GtkWidget *room_frame = gtk_frame_new("Room Selection");
    gtk_box_pack_start(GTK_BOX(vbox), room_frame, FALSE, FALSE, 5);
    
    GtkWidget *room_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(room_frame), room_box);
    gtk_container_set_border_width(GTK_CONTAINER(room_box), 10);
    
    // Current room label
    app->current_room_label = gtk_label_new("Current Room: None (Please select a room)");
    gtk_box_pack_start(GTK_BOX(room_box), app->current_room_label, FALSE, FALSE, 5);
    
    // Room buttons
    GtkWidget *room_buttons_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(room_box), room_buttons_box, FALSE, FALSE, 0);
    
    app->room_a_button = gtk_button_new_with_label("Room A");
    app->room_b_button = gtk_button_new_with_label("Room B");
    app->room_c_button = gtk_button_new_with_label("Room C");
    
    gtk_box_pack_start(GTK_BOX(room_buttons_box), app->room_a_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(room_buttons_box), app->room_b_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(room_buttons_box), app->room_c_button, TRUE, TRUE, 0);
    
    g_signal_connect(app->room_a_button, "clicked", G_CALLBACK(on_room_button_clicked), app);
    g_signal_connect(app->room_b_button, "clicked", G_CALLBACK(on_room_button_clicked), app);
    g_signal_connect(app->room_c_button, "clicked", G_CALLBACK(on_room_button_clicked), app);
    
    // text view
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), 
                                   GTK_POLICY_AUTOMATIC, 
                                   GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);
    
    // text view
    app->text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app->text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app->text_view), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(scrolled), app->text_view);
    
    app->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->text_view));
    
    // info label
    GtkWidget *info_label = gtk_label_new(
        "Type 'A', 'B', or 'C' to join room | Type 'EXIT!' to disconnect | Or use buttons above"
    );
    gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);
    
    //input box
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    // entry
    app->entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "Type your message here...");
    gtk_box_pack_start(GTK_BOX(hbox), app->entry, TRUE, TRUE, 0);
    g_signal_connect(app->entry, "key-press-event", G_CALLBACK(on_key_press), app);
    
    // send button
    app->send_button = gtk_button_new_with_label("Send");
    gtk_box_pack_start(GTK_BOX(hbox), app->send_button, FALSE, FALSE, 0);
    g_signal_connect(app->send_button, "clicked", G_CALLBACK(send_message), app);
    
    // EXIT button
    app->exit_button = gtk_button_new_with_label("EXIT");
    gtk_box_pack_start(GTK_BOX(hbox), app->exit_button, FALSE, FALSE, 0);
    g_signal_connect(app->exit_button, "clicked", G_CALLBACK(on_exit_button_clicked), app);
    
    // set to red
    GtkStyleContext *context = gtk_widget_get_style_context(app->exit_button);
    gtk_style_context_add_class(context, "destructive-action");
    
    // Show all widgets first
    gtk_widget_show_all(app->window);

    // ---------------------------[END GTK GUI INIT]--------------------------------

    
    // start receive thread after GTK is initialized
    append_text(app, "Connected to server!\n");
    append_text(app, "Please select a room or type A/B/C to join.\n\n");
    pthread_create(&app->recv_thread, NULL, receive_messages, app);
    pthread_detach(app->recv_thread);
    
    // Start GTK main loop
    gtk_main();
    
    // Cleanup
    if(app->connected) {
        close(app->sock);
    }
    g_free(app);
    
    return 0;
}