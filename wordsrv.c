#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#include "socket.h"
#include "gameplay.h"

#ifndef PORT
#define PORT 55532
#endif
#define MAX_QUEUE 5
typedef enum
{
    false,
    true
} bool;

void add_player(struct client **top, int fd, struct in_addr addr);
void remove_player(struct client **top, int fd);

/* These are some of the function prototypes that we used in our solution 
 * You are not required to write functions that match these prototypes, but
 * you may find the helpful when thinking about operations in your program.
 */
/* Send the message in outbuf to all clients */
void broadcast(struct game_state *game, char *outbuf);
void announce_turn(struct game_state *game);
void announce_winner(struct game_state *game, struct client *winner);
/* Move the has_next_turn pointer to the next active client */
void advance_turn(struct game_state *game);

/* The set of socket descriptors for select to monitor.
 * This is a global variable because we need to remove socket descriptors
 * from allset when a write to a socket fails.
 */
fd_set allset;

/* Add a client to the head of the linked list
 */
void add_player(struct client **top, int fd, struct in_addr addr)
{
    struct client *p = malloc(sizeof(struct client));

    if (!p)
    {
        perror("malloc");
        exit(1);
    }

    printf("Adding client %s\n", inet_ntoa(addr));

    p->fd = fd;
    p->ipaddr = addr;
    p->name[0] = '\0';
    p->in_ptr = p->inbuf;
    p->inbuf[0] = '\0';
    p->next = *top;
    *top = p;
}

/* Removes client from the linked list and closes its socket.
 * Also removes socket descriptor from allset 
 */
void remove_player(struct client **top, int fd)
{
    struct client **p;

    for (p = top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p)
    {
        struct client *t = (*p)->next;
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
        FD_CLR((*p)->fd, &allset);
        close((*p)->fd);
        free(*p);
        *p = t;
    }
    else
    {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                fd);
    }
}

bool name_exists(struct client *head, char *potential_name)
{ //Check wether or not the name exists already
    struct client *k;
    for (k = head; k != NULL; k = k->next)
    {
        if (strcmp(k->name, potential_name) == 0)
        {
            return true;
        }
    }
    return false;
}

int player_joined_msg(struct client *head, char *user_joined)
{ //Notify active players that a new player with a valid name has joined
    struct client *curr;
    char new_buf[MAX_BUF];
    sprintf(new_buf, "\n%s has just joined us!\n", user_joined);
    for (curr = head; curr != NULL; curr = curr->next)
    {
        if (write(curr->fd, new_buf, strlen(new_buf)) != strlen(new_buf))
        {
            perror("Player joined message");
            exit(1);
        }
    }
    return 0;
}

int guess_processor(char *letter, struct game_state *game, int position, struct client *cur_player)
{ //Process the guess
    char prompt_message[MAX_BUF];
    memset(prompt_message, '\0', MAX_BUF);
    game->letters_guessed[position] = 1;
    game->guesses_left -= 1;
    //if the letter mathes any letter in the word then update the guess.
    int true_false = 0;
    for (int j = 0; j < strlen(game->word); j++)
    {
        if (game->word[j] == *letter)
        {
            game->guess[j] = *letter;
            true_false = 1;
        }
    }
    if (true_false == 0) //In case the word does not contain the letter then notify the player
    {
        prompt_message[0] = *letter;
        strcat(prompt_message, " is not in the word!\n");
        if (write(cur_player->fd, prompt_message, strlen(prompt_message)) != strlen(prompt_message))
        {
            perror("Player guessed letter not in word message");
            exit(1);
        }
    }
    for (struct client *temp = game->head; temp != NULL; temp = temp->next) //Notify every other person that the player guessed a certain letter
    {
        memset(prompt_message, '\0', MAX_BUF);
        strcpy(prompt_message, cur_player->name);
        strcat(prompt_message, " guessed: ");
        prompt_message[strlen(prompt_message)] = *letter;
        prompt_message[strlen(prompt_message)] = '\n';
        if (write(temp->fd, prompt_message, strlen(prompt_message)) != strlen(prompt_message))
        {
            perror("Player guessed letter message");
            exit(1);
        }
    }
    return 0;
}

int update_game_status_message(struct game_state *game, char *dict, struct client *cur_player)
{

    int ltrs_left = 0;
    for (int i = 0; i < strlen(game->word); i++)
    {
        if (game->guess[i] == '-')
        {
            ltrs_left++;
        }
    }

    if (game->guesses_left == 0 || ltrs_left == 0)
    { //Check whether it is the end of the current game; Either no guesses left or the word was guessed in full
        char message[MAX_BUF];
        if (game->guesses_left == 0 && ltrs_left > 0) //Case 1: no more guesses left
        {
            for (struct client *temp = game->head; temp != NULL; temp = temp->next) //Notify everyone
            {
                sprintf(message, "The word was %s\nNo guesses left. Game over. \n\n\n\n", game->word);
                if (write(temp->fd, message, strlen(message)) != strlen(message))
                {
                    perror("Invalid game over message");
                    exit(1);
                }
            }
        }
        else if (game->guesses_left >= 0 && ltrs_left == 0)
        { //Case 2: the word was guessed in full
            for (struct client *temp = game->head; temp != NULL; temp = temp->next)
            { //Notify everyone
                sprintf(message, "The word was %s\nGame over ! %s won!\n\n\n\n", game->word, cur_player->name);
                if (strcmp(temp->name, cur_player->name) != 0)
                {
                    if (write(temp->fd, message, strlen(message)) != strlen(message))
                    {
                        perror("Invalid game over message");
                        exit(1);
                    }
                }
                else
                {
                    sprintf(message, "The word was %s\nGame over! You won!\n\n\n\n", game->word);
                    if (write(temp->fd, message, strlen(message)) != strlen(message))
                    {
                        perror("Invalid game over message");
                        exit(1);
                    }
                }
            }
        }
        init_game(game, dict);                        //Reinitiate a new game if the game is over.
        sprintf(message, "Let's play a new game!\n"); //Notify everyone
        char status_buf[MAX_BUF];
        status_message(status_buf, game);
        for (struct client *temp = game->head; temp != NULL; temp = temp->next)
        {
            if (write(temp->fd, message, strlen(message)) != strlen(message))
            {
                perror("Invalid game over message");
                exit(1);
            }
        }
    }
    return 0;
}

int disconnection_message(struct game_state *game, struct client *cur_client)
{
    // Send a message to active players that a current client has disconnected
    char buf[MAX_BUF];
    sprintf(buf, "\nGoodbye %s\n", cur_client->name);
    for (struct client *temp = game->head; temp != NULL; temp = temp->next)
    {
        if (strcmp(temp->name, cur_client->name) != 0)
        {
            if (write(temp->fd, buf, strlen(buf)) != strlen(buf))
            {
                perror("Invalid game over message");
                exit(1);
            }
        }
    }
    return 0;
}

void advance_turn(struct game_state *game)
{
    //Set the turn for the next player
    if (game->has_next_turn->next != NULL)
    {
        game->has_next_turn = game->has_next_turn->next;
    }
    else
    {
        game->has_next_turn = game->head;
    }
}

int disconnection_handler(struct game_state *game, struct client *p)
{

    disconnection_message(game, p); //Send a notification to active players that a parcticular player left that game
    if (game->has_next_turn == p)
    {
        advance_turn(game); //Advance the turn
    }
    remove_player(&game->head, p->fd); //Remove the player from the game and close the fd
    if (game->head == NULL)
    {
        game->has_next_turn = NULL; // In case there's no more players than the has next turn is set to NULL
    }
    return 0;
}

int main(int argc, char **argv)
{
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGPIPE, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }

    int clientfd, maxfd, nready;
    struct client *p;
    struct sockaddr_in q;
    fd_set rset;

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <dictionary filename>\n", argv[0]);
        exit(1);
    }

    // Create and initialize the game state
    struct game_state game;

    srandom((unsigned int)time(NULL));
    // Set up the file pointer outside of init_game because we want to
    // just rewind the file when we need to pick a new word
    game.dict.fp = NULL;
    game.dict.size = get_file_length(argv[1]);

    init_game(&game, argv[1]);

    // head and has_next_turn also don't change when a subsequent game is
    // started so we initialize them here.
    game.head = NULL;
    game.has_next_turn = NULL;

    /* A list of client who have not yet entered their name.  This list is
     * kept separate from the list of active players in the game, because
     * until the new playrs have entered a name, they should not have a turn
     * or receive broadcast messages.  In other words, they can't play until
     * they have a name.
     */
    struct client *new_players = NULL;

    struct sockaddr_in *server = init_server_addr(PORT);
    int listenfd = set_up_server_socket(server, MAX_QUEUE);

    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    while (1)
    {
        // make a copy of the set before we pass it into select
        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (nready == -1)
        {
            perror("select");
            continue;
        }

        if (FD_ISSET(listenfd, &rset))
        {
            printf("A new client is connecting\n");
            clientfd = accept_connection(listenfd);

            FD_SET(clientfd, &allset);
            if (clientfd > maxfd)
            {
                maxfd = clientfd;
            }
            printf("Connection from %s\n", inet_ntoa(q.sin_addr));
            add_player(&new_players, clientfd, q.sin_addr);
            char *greeting = WELCOME_MSG;
            if (write(clientfd, greeting, strlen(greeting)) == -1)
            {
                fprintf(stderr, "Write to client %s failed\n", inet_ntoa(q.sin_addr));
                remove_player(&new_players, clientfd);
            };
        }

        /* Check which other socket descriptors have something ready to read.
         * The reason we iterate over the rset descriptors at the top level and
         * search through the two lists of clients each time is that it is
         * possible that a client will be removed in the middle of one of the
         * operations. This is also why we call break after handling the input.
         * If a client has been removed the loop variables may not longer be 
         * valid.
         */
        int cur_fd;
        for (cur_fd = 0; cur_fd <= maxfd; cur_fd++)
        {
            if (FD_ISSET(cur_fd, &rset))
            {
                // Check if this socket descriptor is an active player

                for (p = game.head; p != NULL; p = p->next)
                {
                    if (cur_fd == p->fd)
                    {
                        //TODO - handle input from an active client
                        //Read from the player only if it is his turn. Otherwise, ignore what was written.
                        //Check to see if the input is valide or not
                        int num_read;
                        int num_total;
                        char gsd_ltr = 'A';
                        int position = -1;
                        int quitted = 0;
                        int num_write;
                        while (strlen(p->inbuf) != 1 || gsd_ltr < 'a' || gsd_ltr > 'z' || game.letters_guessed[position] == 1)
                        {
                            if (position != -1)
                            {
                                sprintf(p->inbuf, "Invalid INPUT! Try again!\n"); //Notify the player that his input is invalid.
                                write(p->fd, p->inbuf, strlen(p->inbuf));
                            }
                            num_total = 0;
                            num_read = read(p->fd, &(p->inbuf), MAX_BUF);
                            if (num_read == 0)
                            {
                                quitted = 1;
                                disconnection_message(&game, p); //Handling the disconnection
                                if (game.has_next_turn == p)
                                {
                                    advance_turn(&game);
                                    sprintf(game.has_next_turn->inbuf, "Your guess ? ");
                                    num_write = write(game.has_next_turn->fd, game.has_next_turn->inbuf, strlen(game.has_next_turn->inbuf));
                                    while (num_write <= 0)
                                    {
                                        disconnection_handler(&game, p);
                                        num_write = write(game.has_next_turn->fd, game.has_next_turn->inbuf, strlen(game.has_next_turn->inbuf));
                                    }
                                }
                                remove_player(&game.head, p->fd);
                                if (game.head == NULL)
                                {
                                    game.has_next_turn = NULL;
                                }
                                break;
                            }
                            printf("%d bytes read\n", num_read);
                            num_total += num_read;
                            p->inbuf[num_read] = '\0';
                            while (strstr(p->inbuf, "\r\n") == NULL)
                            { //Handling the partial reads
                                num_read = read(p->fd, &(p->inbuf[num_total]), MAX_BUF);
                                if (num_read == 0)
                                {
                                    quitted = 1;
                                    disconnection_message(&game, p); //Handling the disconnection
                                    if (game.has_next_turn == p)
                                    {
                                        advance_turn(&game);
                                        sprintf(game.has_next_turn->inbuf, "Your guess ? ");
                                        num_write = write(game.has_next_turn->fd, game.has_next_turn->inbuf, strlen(game.has_next_turn->inbuf));
                                        while (num_write <= 0)
                                        {
                                            disconnection_handler(&game, p);
                                            num_write = write(game.has_next_turn->fd, game.has_next_turn->inbuf, strlen(game.has_next_turn->inbuf));
                                        }
                                    }
                                    remove_player(&game.head, p->fd);
                                    if (game.head == NULL)
                                    {
                                        game.has_next_turn = NULL;
                                    }
                                    break;
                                }
                                printf("%d bytes read\n", num_read);
                                num_total += num_read;
                                p->inbuf[num_total] = '\0';
                            }
                            p->inbuf[num_total - 2] = '\0';
                            if (strlen(p->inbuf) == 1)
                            {
                                gsd_ltr = p->inbuf[0];
                                position = (int)(gsd_ltr - 'a');
                            }
                        }

                        if (game.has_next_turn == p && quitted == 0)
                        {
                            //Process the input
                            guess_processor(&gsd_ltr, &game, position, p);

                            //Reset the game to the new state IF THE GAME IS OVER!
                            update_game_status_message(&game, argv[1], p);

                            //Display the new status of the game state
                            char status_buf[MAX_BUF];
                            status_message(status_buf, &game);
                            for (struct client *temp = game.head; temp != NULL; temp = temp->next)
                            {
                                int num_write = write(temp->fd, status_buf, strlen(status_buf));
                                if (num_write <= 0)
                                {
                                    disconnection_handler(&game, p);
                                }
                            }
                            //pass the turn to the next player.
                            advance_turn(&game);

                            for (struct client *q = game.head; q != NULL; q = q->next)
                            {
                                if (q != game.has_next_turn)
                                {
                                    sprintf(q->inbuf, "%s's turn!\n", game.has_next_turn->name);
                                    int num_write = write(q->fd, q->inbuf, strlen(q->inbuf));
                                    if (num_write <= 0)
                                    {
                                        disconnection_handler(&game, p);
                                    }
                                }
                            }
                            sprintf(game.has_next_turn->inbuf, "Your guess ? ");
                            int num_write = write(game.has_next_turn->fd, game.has_next_turn->inbuf, strlen(game.has_next_turn->inbuf));
                            if (num_write <= 0)
                            {
                                disconnection_handler(&game, p);
                            }
                        }
                        else if (quitted == 0)
                        {
                            sprintf(p->inbuf, "It's not your turn!\n");
                            int num_write = write(p->fd, p->inbuf, strlen(p->inbuf));
                            if (num_write <= 0)
                            {
                                disconnection_handler(&game, p);
                            }
                        }
                        break;
                    }
                }

                // Check if any new players are entering their names
                for (p = new_players; p != NULL; p = p->next)
                {
                    if (cur_fd == p->fd)
                    {
                        //TODO - handle input from an new client who has
                        //not entered an acceptable name.
                        int num_write;
                        int quitted = 0;
                        int num_total;
                        int num_read;
                        int attempt = 1;
                        while (strlen(p->name) == 0 || name_exists(game.head, p->name))
                        {
                            if (attempt != 1)
                            {
                                sprintf(p->inbuf, "Invalid Name! Try again!\n");
                                num_write = write(p->fd, p->inbuf, strlen(p->inbuf));
                                if (num_write == 0)
                                {
                                    remove_player(&new_players, p->fd);
                                    quitted = 1;
                                    break;
                                }
                            }
                            num_total = 0;
                            num_read = read(p->fd, &(p->inbuf), MAX_BUF);
                            if (num_read == 0)
                            {
                                remove_player(&new_players, p->fd);
                                quitted = 1;
                                break;
                            }
                            printf("%d bytes read\n", num_read);
                            num_total += num_read;
                            p->inbuf[num_read] = '\0';
                            while (strstr(p->inbuf, "\r\n") == NULL)
                            {
                                num_read = read(p->fd, &(p->inbuf[num_total]), MAX_BUF);
                                if (num_read == 0)
                                {
                                    remove_player(&new_players, p->fd);
                                    quitted = 1;
                                    break;
                                }
                                printf("%d bytes read\n", num_read);
                                num_total += num_read;
                                p->inbuf[num_total] = '\0';
                            }
                            p->inbuf[num_total - 2] = '\0';
                            strcat(p->name, p->inbuf);
                            attempt = 2;
                        }
                        if (quitted == 1)
                        {
                            break;
                        }
                        //Set the turn for the player
                        if (game.head == NULL)
                        { //if this is the first player to join then set his turn to be next.
                            game.has_next_turn = p;
                        }

                        //Deleting the player from the linked list new_players once a valid name is entered.
                        if (strcmp(new_players->name, p->name) == 0)
                        {
                            new_players = NULL;
                        }
                        else
                        {
                            for (struct client *q = new_players; q != NULL; q = q->next)
                            {
                                if (strcmp((q->next)->name, p->name) == 0)
                                {
                                    q->next = p->next;
                                }
                            }
                        }
                        //Assigning the player with the name to the head of the game.
                        struct client *new_head = p;
                        new_head->next = game.head;
                        game.head = new_head;

                        //Sending a message to active players that a new player has joined.
                        player_joined_msg(game.head, p->name);
                        char status_buf[MAX_BUF];
                        status_message(status_buf, &game);
                        num_write = write(p->fd, status_buf, strlen(status_buf));
                        if (num_write <= 0)
                        {
                            disconnection_handler(&game, p);
                        }

                        sprintf(game.has_next_turn->inbuf, "Your guess ? ");
                        num_write = write(game.has_next_turn->fd, game.has_next_turn->inbuf, strlen(game.has_next_turn->inbuf));
                        while (num_write <= 0)
                        {
                            disconnection_handler(&game, p);
                            num_write = write(game.has_next_turn->fd, game.has_next_turn->inbuf, strlen(game.has_next_turn->inbuf));
                        }
                    }
                    break;
                }
            }
        }
    }
    return 0;
}
