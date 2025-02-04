//Rahma Seid
//CSCI 3240
//client.c: This program runs the client-side functionality for the Pac-Man game, interacting with the server for game state updates and sending player inputs.

#include "csapp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAP_SIZE 25

void display_map(char map[MAP_SIZE][MAP_SIZE]);
void get_input_and_send(int clientfd);
void receive_and_update(int clientfd, char map[MAP_SIZE][MAP_SIZE]);

int main(int argc, char *argv[]) {
    int clientfd;
    char *host, *port;
    char map[MAP_SIZE][MAP_SIZE];

    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }

    host = argv[1];
    port = argv[2];
    printf("Connecting to server at %s:%s...\n", host, port);
    clientfd = Open_clientfd(host, port);

    while (1) {
        // Receive the current game state from the server
        receive_and_update(clientfd, map);

        // Display the game state
        display_map(map);

        // Get user input and send it to the server
        get_input_and_send(clientfd);
    }

    Close(clientfd);
    return 0;
}

void display_map(char map[MAP_SIZE][MAP_SIZE]) {
    if (system("clear") == -1) {
        perror("system call failed");
    }
    for (int i = 0; i < MAP_SIZE; i++) {
        for (int j = 0; j < MAP_SIZE; j++) {
            putchar(map[i][j]);
        }
        putchar('\n');
    }
}

void get_input_and_send(int clientfd) {
    char input;
    char buffer[MAXLINE];

    printf("Enter your move (W/A/S/D to move, Q to quit): ");
    input = getchar();
    while (getchar() != '\n'); // Clear the input buffer

    switch (input) {
        case 'w':
        case 'W':
            snprintf(buffer, sizeof(buffer), "UP");
            break;
        case 'a':
        case 'A':
            snprintf(buffer, sizeof(buffer), "LEFT");
            break;
        case 's':
        case 'S':
            snprintf(buffer, sizeof(buffer), "DOWN");
            break;
        case 'd':
        case 'D':
            snprintf(buffer, sizeof(buffer), "RIGHT");
            break;
        case 'q':
        case 'Q':
            snprintf(buffer, sizeof(buffer), "QUIT");
            if (write(clientfd, buffer, strlen(buffer) + 1) == -1) {
                perror("write failed");
            }
            Close(clientfd);  // Close the socket here
            printf("Connection closed. Exiting client...\n");
            exit(0);          // Exit the client program
        default:
            printf("Invalid input. Try again.\n");
            return;
    }

    if (write(clientfd, buffer, strlen(buffer) + 1) == -1) {
        perror("write failed");
    }
}


void receive_and_update(int clientfd, char map[MAP_SIZE][MAP_SIZE]) {
    char buffer[MAP_SIZE * MAP_SIZE + 20]; // Extra space for score and pellet count
    ssize_t n;
    int score = 0, remaining_pellets = 0;

    memset(buffer, 0, sizeof(buffer));

    n = read(clientfd, buffer, sizeof(buffer));
    if (n <= 0) {
        printf("\nGame Over! Final score: %d\n", score); // Use tracked score if disconnected prematurely
        printf("Server disconnected. Exiting...\n");
        Close(clientfd);
        exit(1);
    }

    // Check for the GAME_OVER message
    if (strncmp(buffer, "GAME_OVER", 9) == 0) {
        int final_score = 0;
        sscanf(buffer + 10, "%d", &final_score);
        printf("\nGame Over! Final score: %d\n", final_score);
        Close(clientfd);
        exit(0);
    }

    // Parse the score and pellet count from the buffer
    sscanf(buffer, "%10d%10d", &score, &remaining_pellets);

    // Update the map
    memcpy(map, buffer + 20, sizeof(map[0][0]) * MAP_SIZE * MAP_SIZE);

    // Display the updated score
    printf("Current score: %d\n", score);
}