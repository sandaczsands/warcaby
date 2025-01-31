#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#define MAX_GAMES 10

pthread_mutex_t gameMutex[MAX_GAMES];

char gameBoards[MAX_GAMES][8][8]; //array with boards of game state
int gameTurns[MAX_GAMES] = {0}; // 0 for white, 1 for black
int gamePlayersSockets[MAX_GAMES][2] = {{0, 0}};
int gamePlayerCount[MAX_GAMES] = {0}; //for keeping count of players in each game

void create_gameBoards() {
    char board[8][8] = {
        {' ', 'b', ' ', 'b', ' ', 'b', ' ', 'b'},
        {'b', ' ', 'b', ' ', 'b', ' ', 'b', ' '},
        {' ', 'b', ' ', 'b', ' ', 'b', ' ', 'b'},
        {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
        {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
        {'w', ' ', 'w', ' ', 'w', ' ', 'w', ' '},
        {' ', 'w', ' ', 'w', ' ', 'w', ' ', 'w'},
        {'w', ' ', 'w', ' ', 'w', ' ', 'w', ' '}
    };

    for (int gameID = 0; gameID < MAX_GAMES; gameID++) {
        memcpy(gameBoards[gameID], board, sizeof(board));
    }
}

void prepare_board(char* buffer, int gameID) { //prepare board for sending to player/ add x ax and y ax
    strcpy(buffer, "  0 1 2 3 4 5 6 7\n");
    for(int i = 0; i < 8; i++) {
        char row[32];
        sprintf(row, "%d ", i);
        strcat(buffer, row);
        for(int j = 0; j < 8; j++) {
            char piece[3];
            sprintf(piece, "%c ", gameBoards[gameID][i][j]);
            strcat(buffer, piece);
        }
        sprintf(row, "%d\n", i);
        strcat(buffer, row);
    }
    strcat(buffer, "  0 1 2 3 4 5 6 7\n");
}

bool in_bounds(int row, int col) {
    return row >= 0 && row < 8 && col >= 0 && col < 8;
}

bool is_empty(int row, int col, int gameID) {
    return gameBoards[gameID][row][col] == ' ';
}

bool is_opponent_piece(char piece, int player) {
    if (player == 0) { //white player move
        return (piece == 'b' || piece == 'B');
    } else if (player == 1) { 
        return (piece == 'w' || piece == 'W');
    }
    return false;
}

bool is_valid_move(int fromRow, int fromCol, int toRow, int toCol, int player, int gameID) {
    if (!in_bounds(fromRow, fromCol) || !in_bounds(toRow, toCol)) {
        return false;
    }

    char piece = gameBoards[gameID][fromRow][fromCol];

    if (player == 0 && (piece != 'w' && piece != 'W')) return false; //player is using wrong pieces
    if (player == 1 && (piece != 'b' && piece != 'B')) return false;

    if (!is_empty(toRow, toCol, gameID)) return false; //putting piece on piece

    int row_diff = toRow - fromRow;
    int col_diff = toCol - fromCol;

    if (abs(row_diff) == 2 && abs(col_diff) == 2) { //if doing jump
        int mid_row = (fromRow + toRow) / 2;
        int mid_col = (fromCol + toCol) / 2;
        if (piece == 'W' || piece == 'B'){ //if queen 
            return true;
        } else if (!is_opponent_piece(gameBoards[gameID][mid_row][mid_col], player)) { //check if piece is jumping over piece
            return false;
        }
        return true;
    }

    if (piece == 'w') { //if its normal piece that isnt jumping over
        if (row_diff != -1 || abs(col_diff) != 1) return false; //white goes up, form 7 to 0
    } else if (piece == 'b') {
        if (row_diff != 1 || abs(col_diff) != 1) return false; //black goes down, from 0 to 7
    }

    if (piece == 'W' || piece == 'B') {
        if (abs(row_diff) != abs(col_diff)) return false; 
        if (abs(row_diff) == 1 && abs(col_diff) == 1) return true; //if normal step

        int row_step = (row_diff > 0) ? 1 : -1;
        int col_step = (col_diff > 0) ? 1 : -1;
        int row = fromRow + row_step;
        int col = fromCol + col_step;

        while (row != toRow || col != toCol) { //move twoard final destination
            if (!is_empty(row, col, gameID)) { //check for pieces in way
                if (is_opponent_piece(gameBoards[gameID][row][col], player)) {
                    int next_row = row + row_step;
                    int next_col = col + col_step;
                    if (next_row == toRow && next_col == toCol && is_empty(next_row, next_col, gameID)) {
                        return true;
                    }
                    return false;
                }
                return false;
            }
            row += row_step;
            col += col_step;
        }
        return true;
    }
    return true;
}

void update_board(int fromRow, int fromCol, int toRow, int toCol, int gameID) {
    char piece = gameBoards[gameID][fromRow][fromCol];
    gameBoards[gameID][toRow][toCol] = piece;
    gameBoards[gameID][fromRow][fromCol] = ' ';

    if (abs(toRow - fromRow) == 2 && abs(toCol - fromCol) == 2) { //if opponent got hit
        int mid_row = (fromRow + toRow) / 2;
        int mid_col = (fromCol + toCol) / 2;
        gameBoards[gameID][mid_row][mid_col] = ' ';
    }

    if (piece == 'w' && toRow == 0) { //if pieces got to end of board
        gameBoards[gameID][toRow][toCol] = 'W';
    } else if (piece == 'b' && toRow == 7) {
        gameBoards[gameID][toRow][toCol] = 'B';
    }
}

int is_game_over(int gameID) { //count pieces to check if someone won
    int white_pieces = 0;
    int black_pieces = 0;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (gameBoards[gameID][i][j] == 'w' || gameBoards[gameID][i][j] == 'W') white_pieces++;
            if (gameBoards[gameID][i][j] == 'b' || gameBoards[gameID][i][j] == 'B') black_pieces++;
        }
    }
    if (white_pieces == 0) return 1;
    if (black_pieces == 0) return 2;
    return 0;
}

void *socketThread(void *arg) {
    int newSocket = *((int *)arg);
    char playerMove[16];
    char boardState[1024];
    int gameID = -1;
    int player = -1;

    for (int i = 0; i < MAX_GAMES; i++) { //find new player a game
        pthread_mutex_lock(&gameMutex[i]);

        if (gamePlayerCount[i] < 2) {
            gameID = i;
            player = gamePlayerCount[i];
            gamePlayerCount[i]++;
            gamePlayersSockets[i][player] = newSocket;
            pthread_mutex_unlock(&gameMutex[i]);
            break;
        }
        pthread_mutex_unlock(&gameMutex[i]);
    }

    if (gameID == -1) {
        send(newSocket, "Server is full. Try again later.\n", 32, 0);
        close(newSocket);
        pthread_exit(NULL);
    }

    printf("Player %d joined game %d.\n", player + 1, gameID + 1);

    if (player == 0) { //if first player joined
        send(newSocket, "You are playing as white. Waiting for another player...\n", 56, 0);

    } else if (player == 1) { //if second joined
        send(gamePlayersSockets[gameID][0], "Game starting...\n", 17, 0);
        send(gamePlayersSockets[gameID][1], "Game starting...\n", 17, 0);

        pthread_mutex_lock(&gameMutex[gameID]); //lock game so no one can join
        prepare_board(boardState, gameID);
        pthread_mutex_unlock(&gameMutex[gameID]);

        send(gamePlayersSockets[gameID][0], boardState, strlen(boardState), 0);
        send(gamePlayersSockets[gameID][1], boardState, strlen(boardState), 0);

        send(gamePlayersSockets[gameID][0], "Enter your move (like 5243 or 'exit' to quit the game): ", 56, 0);
    }

    for (;;) { //game loop
        memset(playerMove, 0, sizeof(playerMove));
        int n = recv(newSocket, playerMove, sizeof(playerMove) - 1, 0);
        
        if (n <= 0) {
            printf("Player %d in game %d disconnected.\n", player + 1, gameID + 1);
            send(gamePlayersSockets[gameID][0], "Your opponent quit the game. Ending the game...", 45, 0);
            send(gamePlayersSockets[gameID][1], "Your opponent quit the game. Ending the game...", 45, 0);
            break;
        }

        printf("Received move from player %d in game %d: %s\n", player + 1, gameID + 1, playerMove);

        int fromRow = playerMove[0] - '0'; //count difference between char number and char '0'
        int fromCol = playerMove[1] - '0';
        int toRow = playerMove[2] - '0';
        int toCol = playerMove[3] - '0';

        pthread_mutex_lock(&gameMutex[gameID]); //lock to check move

        if (is_valid_move(fromRow, fromCol, toRow, toCol, player, gameID)) {
            update_board(fromRow, fromCol, toRow, toCol, gameID);
            gameTurns[gameID] = 1 - gameTurns[gameID];
            prepare_board(boardState, gameID);

            int game_over_status = is_game_over(gameID);
            if (game_over_status == 1) {
                strcat(boardState, "Black wins!\n");
                send(gamePlayersSockets[gameID][0], boardState, strlen(boardState), 0);
                send(gamePlayersSockets[gameID][1], boardState, strlen(boardState), 0);
                break;
            } else if (game_over_status == 2) {
                strcat(boardState, "White wins!\n");
                send(gamePlayersSockets[gameID][0], boardState, strlen(boardState), 0);
                send(gamePlayersSockets[gameID][1], boardState, strlen(boardState), 0);
                break;
            }

            send(gamePlayersSockets[gameID][0], "Updated board:\n", 15, 0);
            send(gamePlayersSockets[gameID][0], boardState, strlen(boardState), 0);
            send(gamePlayersSockets[gameID][1], "Updated board:\n", 15, 0);
            send(gamePlayersSockets[gameID][1], boardState, strlen(boardState), 0);

            if (gameTurns[gameID] == 0) {
                send(gamePlayersSockets[gameID][0], "Enter your move (like 5243 or 'exit' to quit the game): ", 56, 0);
            } else {
                send(gamePlayersSockets[gameID][1], "Enter your move (like 5243 or 'exit' to quit the game): ", 56, 0);
            }
        } else {
            send(newSocket, "Invalid move. Please try again.\n", 32, 0);
            if (gameTurns[gameID] == 0) {
                send(gamePlayersSockets[gameID][0], "Enter your move (like 5243 or 'exit' to quit the game): ", 56, 0);
            } else {
                send(gamePlayersSockets[gameID][1], "Enter your move (like 5243 or 'exit' to quit the game): ", 56, 0);
            }
        }

        pthread_mutex_unlock(&gameMutex[gameID]); //allow new move
    }

    close(newSocket);
    gamePlayersSockets[gameID][player] = -1;
    gamePlayerCount[gameID]--;
    printf("Closing client socket for player %d in game %d\n", player + 1, gameID + 1);
    
    pthread_exit(NULL);
}

int main(void) {
    int serverSocket, newSocket;
    struct sockaddr_in serverAddr;
    struct sockaddr_storage serverStorage;
    socklen_t addrSize;
    pthread_t threadID;

    create_gameBoards();

    for (int i = 0; i < MAX_GAMES; i++) {
        pthread_mutex_init(&gameMutex[i], NULL);
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(2220);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);

    serverSocket = socket(PF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }

    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("bind failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    if (listen(serverSocket, 20) == -1) {
        perror("listen failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }
    printf("Server is listening.\n");

    while (1) {
        addrSize = sizeof serverStorage;
        newSocket = accept(serverSocket, (struct sockaddr *)&serverStorage, &addrSize);

        if (newSocket == -1) {
            perror("accept failed");
            continue;
        }

        if (pthread_create(&threadID, NULL, socketThread, &newSocket) != 0) {
            printf("failed to create thread\n");
        }

        pthread_detach(threadID);
    }

    close(serverSocket);
    return EXIT_SUCCESS;
}