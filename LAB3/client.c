#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8889
#define BUFFER_SIZE 1024

typedef struct {
    GtkWidget *window;
    GtkWidget *text_view;
    GtkWidget *entry;
    GtkWidget *send_button;
    GtkWidget *exit_button;
    GtkTextBuffer *buffer;
    int sock;
    int connected;
    pthread_t recv_thread;
} AppWidgets;

typedef struct {
    AppWidgets *app;
    char *message;
} MessageData;

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
        
        append_text(app, buffer);
    }
    
    return NULL;
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
    
    // Check for EXIT command
    if(strcmp(message, "EXIT!") == 0) {
        append_text(app, "Disconnecting...\n");
        app->connected = 0;
        sleep(1);
        gtk_main_quit();
    }
    else {
        // Echo own message
        char echo[BUFFER_SIZE];
        message[BUFFER_SIZE - 20] = '\0';
        snprintf(echo, BUFFER_SIZE, "[You]: %s\n", message);
        append_text(app, echo);
    }
    
    gtk_entry_set_text(GTK_ENTRY(app->entry), "");
}

void on_exit_button_clicked(GtkWidget *widget, gpointer data) {
    AppWidgets *app = (AppWidgets*)data;
    
    if(app->connected) {
        send(app->sock, "EXIT!", 5, 0);
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
    
    // Initialize GTK
    gtk_init(&argc, &argv);
    
    // Create window
    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), "TCP Chat Client");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 500, 400);
    g_signal_connect(app->window, "destroy", G_CALLBACK(on_window_destroy), app);
    
    // Create vertical box
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(app->window), vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    
    // Create scrolled window for text view
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), 
                                   GTK_POLICY_AUTOMATIC, 
                                   GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);
    
    // Create text view
    app->text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app->text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app->text_view), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(scrolled), app->text_view);
    
    app->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->text_view));
    
    // Create info label
    GtkWidget *info_label = gtk_label_new(
        "Type your message and press Enter or click Send\n"
        "Type 'EXIT!' to disconnect"
    );
    gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);
    
    // Create horizontal box for input
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    // Create entry
    app->entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "Type your message here...");
    gtk_box_pack_start(GTK_BOX(hbox), app->entry, TRUE, TRUE, 0);
    g_signal_connect(app->entry, "key-press-event", G_CALLBACK(on_key_press), app);
    
    // Create send button
    app->send_button = gtk_button_new_with_label("Send");
    gtk_box_pack_start(GTK_BOX(hbox), app->send_button, FALSE, FALSE, 0);
    g_signal_connect(app->send_button, "clicked", G_CALLBACK(send_message), app);
    
    // Create EXIT button
    app->exit_button = gtk_button_new_with_label("EXIT");
    gtk_box_pack_start(GTK_BOX(hbox), app->exit_button, FALSE, FALSE, 0);
    g_signal_connect(app->exit_button, "clicked", G_CALLBACK(on_exit_button_clicked), app);
    
    // Style the EXIT button (make it red)
    GtkStyleContext *context = gtk_widget_get_style_context(app->exit_button);
    gtk_style_context_add_class(context, "destructive-action");
    
    // Show all widgets first
    gtk_widget_show_all(app->window);
    
    // Now start receive thread after GTK is initialized
    append_text(app, "Connected to server!\n\n");
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