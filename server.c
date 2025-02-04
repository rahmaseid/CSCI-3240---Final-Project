//Rahma Seid
//CSCI 3240
//server.c: This program manages the server-side functionality for the Pac-Man game, handling client connections, game state updates, and synchronization.

#include "csapp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define MAP_SIZE 25
#define MAX_CLIENTS 100

typedef struct {
    char map[MAP_SIZE][MAP_SIZE];
    int pacman_x, pacman_y;
    int ghost_x, ghost_y;
    int pellet_count;
    int score;
} GameState;

pthread_mutex_t rw_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t rw_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t client_count_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t all_clients_done = PTHREAD_COND_INITIALIZER;

int reader_count = 0;
int writer_active = 0;
int active_clients = 0;
int finished_clients = 0; // Tracks the number of finished clients

void enter_read_lock();
void exit_read_lock();
void enter_write_lock();
void exit_write_lock();
void initialize_map(GameState *game_state);
void *handle_client(void *arg);
void send_game_state(int clientfd, GameState *game_state);
void move_ghost(GameState *game_state, int clientfd);
void update_game_state(char *command, GameState *game_state, int connfd);

int main(int argc, char *argv[]) {
    int listenfd;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    listenfd = Open_listenfd(argv[1]);

    while (1) {
        struct sockaddr_storage clientaddr;
        socklen_t clientlen = sizeof(struct sockaddr_storage);
        int *connfdp = malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        // Increment active clients
        pthread_mutex_lock(&client_count_mutex);
        active_clients++;
        printf("New client connected. Active clients: %d\n", active_clients);
        pthread_mutex_unlock(&client_count_mutex);

        pthread_t tid;
        Pthread_create(&tid, NULL, handle_client, connfdp);
    }

    // Wait for all clients to finish
    pthread_mutex_lock(&client_count_mutex);
    while (active_clients > 0) {
        pthread_cond_wait(&all_clients_done, &client_count_mutex);
    }
    pthread_mutex_unlock(&client_count_mutex);

    printf("All clients disconnected. Shutting down server.\n");
    return 0;
}


void enter_read_lock() {
    pthread_mutex_lock(&rw_mutex);
    while (writer_active > 0) {
        pthread_cond_wait(&rw_cond, &rw_mutex);
    }
    reader_count++;
    pthread_mutex_unlock(&rw_mutex);
}

void exit_read_lock() {
    pthread_mutex_lock(&rw_mutex);
    reader_count--;
    if (reader_count == 0) {
        pthread_cond_broadcast(&rw_cond);
    }
    pthread_mutex_unlock(&rw_mutex);
}

void enter_write_lock() {
    pthread_mutex_lock(&rw_mutex);
    while (writer_active > 0 || reader_count > 0) {
        pthread_cond_wait(&rw_cond, &rw_mutex);
    }
    writer_active = 1;
    pthread_mutex_unlock(&rw_mutex);
}

void exit_write_lock() {
    pthread_mutex_lock(&rw_mutex);
    writer_active = 0;
    pthread_cond_broadcast(&rw_cond);
    pthread_mutex_unlock(&rw_mutex);
}

void initialize_map(GameState *game_state) {
    memset(game_state->map, ' ', sizeof(game_state->map));
    for (int i = 0; i < MAP_SIZE; i++) {
        game_state->map[0][i] = game_state->map[MAP_SIZE - 1][i] = '#';
        game_state->map[i][0] = game_state->map[i][MAP_SIZE - 1] = '#';
    }
    game_state->pacman_x = 1;
    game_state->pacman_y = 1;
    game_state->ghost_x = MAP_SIZE - 2;
    game_state->ghost_y = MAP_SIZE - 2;
    game_state->map[game_state->pacman_x][game_state->pacman_y] = 'P';
    game_state->map[game_state->ghost_x][game_state->ghost_y] = 'G';

    game_state->pellet_count = 0;
    for (int i = 1; i < MAP_SIZE - 1; i++) {
        for (int j = 1; j < MAP_SIZE - 1; j++) {
            if ((i + j) % 4 == 0 && game_state->map[i][j] == ' ') {
                game_state->map[i][j] = '.';
                game_state->pellet_count++;
            }
        }
    }
    game_state->score = 0;
}

void *handle_client(void *arg) {
    int connfd = *((int *)arg);
    free(arg);
    pthread_detach(pthread_self());

    GameState game_state;
    initialize_map(&game_state);

    char buffer[MAXLINE];

    // Send initial game state to the client
    send_game_state(connfd, &game_state);

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t n = read(connfd, buffer, sizeof(buffer));
        if (n <= 0) {
            printf("Client disconnected.\n");

            // Decrement active_clients and signal if no clients remain
            pthread_mutex_lock(&client_count_mutex);
            active_clients--;
            printf("Active clients: %d\n", active_clients);
            if (active_clients == 0) {
                pthread_cond_signal(&all_clients_done);
            }
            pthread_mutex_unlock(&client_count_mutex);
            Close(connfd);
            pthread_exit(NULL);
        }

        //printf("Received command: %s\n", buffer);

        // Update game state and send the updated state to the client
        update_game_state(buffer, &game_state, connfd);
        send_game_state(connfd, &game_state);
        move_ghost(&game_state, connfd);
    }

    Close(connfd);
    return NULL;
}


void update_game_state(char *command, GameState *game_state, int connfd) {
    game_state->map[game_state->pacman_x][game_state->pacman_y] = ' ';

    if (strcmp(command, "UP") == 0 && game_state->map[game_state->pacman_x - 1][game_state->pacman_y] != '#') {
        game_state->pacman_x--;
    } else if (strcmp(command, "DOWN") == 0 && game_state->map[game_state->pacman_x + 1][game_state->pacman_y] != '#') {
        game_state->pacman_x++;
    } else if (strcmp(command, "LEFT") == 0 && game_state->map[game_state->pacman_x][game_state->pacman_y - 1] != '#') {
        game_state->pacman_y--;
    } else if (strcmp(command, "RIGHT") == 0 && game_state->map[game_state->pacman_x][game_state->pacman_y + 1] != '#') {
        game_state->pacman_y++;
    }

    if (game_state->map[game_state->pacman_x][game_state->pacman_y] == '.') {
        game_state->pellet_count--;
        game_state->score += 2; // Increment score by 2 points per pellet
    }

    game_state->map[game_state->pacman_x][game_state->pacman_y] = 'P';
}

void send_game_state(int clientfd, GameState *game_state) {
    char buffer[MAP_SIZE * MAP_SIZE + 20]; // Extra space for score and pellet count

    snprintf(buffer, sizeof(buffer), "%010d%010d", game_state->score, game_state->pellet_count);
    memcpy(buffer + 20, game_state->map, sizeof(game_state->map));

    if (write(clientfd, buffer, sizeof(buffer)) == -1) {
        perror("write failed");
    } //else {
        //printf("Sent game state to client. Score: %d, Pellets remaining: %d\n", game_state->score, game_state->pellet_count);
    //}
}

void move_ghost(GameState *game_state, int clientfd) {
    game_state->map[game_state->ghost_x][game_state->ghost_y] = ' ';

    int dx = game_state->pacman_x - game_state->ghost_x;
    int dy = game_state->pacman_y - game_state->ghost_y;

    if (abs(dx) > abs(dy)) {
        // Prioritize vertical movement
        if (dx > 0) {
            game_state->ghost_x++;
        } else {
            game_state->ghost_x--;
        }
    } else {
        // Prioritize horizontal movement
        if (dy > 0) {
            game_state->ghost_y++;
        } else {
            game_state->ghost_y--;
        }
    }

    if (game_state->ghost_x == game_state->pacman_x && game_state->ghost_y == game_state->pacman_y) {
        printf("Client game over: Ghost caught Pac-Man.\n");
        char buffer[MAXLINE];
        snprintf(buffer, sizeof(buffer), "GAME_OVER %010d", game_state->score);

        // Handle the return value of write
        ssize_t bytes_written = write(clientfd, buffer, strlen(buffer) + 1);
        if (bytes_written == -1) {
            perror("Error sending GAME_OVER message");
        }

        usleep(1000); // Ensure the message is sent before closing
        Close(clientfd);
        pthread_exit(NULL);
    }

    game_state->map[game_state->ghost_x][game_state->ghost_y] = 'G';
}
