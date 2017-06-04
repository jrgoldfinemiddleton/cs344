/* otp_dec_d.c
 * Author: Jason Goldfine-Middleton
 * Course: CS 344
 */

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/* general purpose byte buffer size - huge to handle large transmissions */
#define SIZEBUF 200000

/* maximum allowed accepted connections on server socket */
#define MAX_CON 20


int bg_check(pid_t **bg_pids, int *num_bg, int max_bg);
void decode(char *decoded, size_t len, char *buffer, char *key);
int handshake(int sockfd, const char *sig,
        const char *resp_sig, size_t respsz);
int listen_port(int p);
int process(int sockfd);
int propose_port(int sockfd, int oldportno);


/* Checks on each background process started by shell and possibly
 * still running.  Processes still running are stored in *bg_pids.
 */
int bg_check(pid_t **bg_pids, int *num_bg, int max_bg)
{
    /* list of pids still running */
    pid_t *running_pids;
    int i, j = 0;
    int status, cur_pid;

    /* if no background processes, nothing to do */
    if (!(*num_bg))
        return 1;

    if (!(running_pids = malloc(max_bg * sizeof(pid_t)))) {
        perror("could not allocate memory");
        return 0;
    }

    /* check to see if each background process has exited or terminated */
    for (i = 0; i != *num_bg; ++i) {
        cur_pid = waitpid((*bg_pids)[i], &status, WNOHANG);
        /* if not finished add it to the list of running
           background processes */
        if (cur_pid <= 0)
            running_pids[j++] = (*bg_pids)[i];
    }

    *num_bg = j;
    free(*bg_pids);
    *bg_pids = running_pids;
    return 1;
}


/* Given a buffer containing a string and a randomized key,
 * applies the OTP transformation and stores resulting first
 * len chars in encoded
 */
void decode(char *decoded, size_t len, char *buffer, char *key)
{
    int i;
    char ch;
    char b, k;

    /* for the first len chars, get the new char from the
       buffer and key and store it */
    for (i = 0; i < len; ++i) {
        b = buffer[i];
        k = key[i];
        b = (b != ' ') ? b - 'A' : 26;
        k = (k != ' ') ? k - 'A' : 26;
        ch = (b - k + 27) % 27;
        ch = (ch != 26) ? ch + 'A' : ' ';
        decoded[i] = ch;
    }

    /* null-terminate */
    decoded[i] = 0;
}


/* Verifies that the client accepted on socket sockfd can supply
 * a matching signature
 */
int handshake(int sockfd, const char *sig,
        const char *resp_sig, size_t respsz)
{
    /* set up the buffer to hold signature sent from client */
    char buffer[SIZEBUF];
    memset(buffer, 0, sizeof(buffer));

    /* get the signature and store in buffer */
    read(sockfd, buffer, sizeof(buffer) - 1);

    /* if signature matches what was expected, send back the server's
       own signature */
    if (strcmp(sig, buffer) == 0) {
        write(sockfd, resp_sig, respsz - 1);
        return 1;
    }

    return 0;
}


/* Creates, binds to, and listens on a new socket on port p */
int listen_port(int p)
{
    int sockfd;
    struct sockaddr_in serv_addr;

    /* try to get a socket file descriptor */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }

    /* try to bind the socket to a specific port and allow all traffic */
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(p);
    if (bind(sockfd, (struct sockaddr *) &serv_addr,
                sizeof(serv_addr)) < 0) {
        return -2;
    }

    /* try to listen on the socket */
    if (listen(sockfd, p) < 0) {
        return -3;
    }

    /* listening on port p */
    return sockfd;
}


/* Reads all the input from the client (expected to be a message followed 
 * by a key, each terminated by a newline character) and then writes back
 * a decrypted message.
 */
int process(int sockfd)
{
    /* buffer to hold read data */
    char buffer[SIZEBUF];

    /* buffer to hold message to send back */
    char *decoded;

    /* counters */
    size_t rdb = 0, trdb = 0, left = sizeof(buffer), len = 0;

    /* pointer to first char of key */
    char *key = buffer;

    /* flag set to true after first newline char is found */
    int found_key = 0;

    memset(buffer, 0, sizeof(buffer));
    
    /* read from client until key is found, then read that many more chars
       to reconstruct the key */
    for (;;) {
        rdb = read(sockfd, buffer + rdb, left);

        if (!found_key) {
            for (; (*key != '\n') && (key < buffer + rdb); ++key)
                ++len;

            /* key begins after the first newline */
            if (*key == '\n') {
                ++key;
                found_key = 1;
            }
        }

        left -= rdb;
        trdb += rdb;

        /* once 2 * message length read, we're good */
        if (trdb > 2 * len)
            break;
    }

    /* uncomment for diagnostics */
    /*
    fprintf(stderr, "trdb: %d\n", trdb);
    fprintf(stderr, "len: %d\n", len);
    fprintf(stderr, "found_key: %d\n", found_key);
    fprintf(stderr, "left: %d\n", left);
    fprintf(stderr, "%s\n", buffer);
    fprintf(stderr, "%s\n", key);
    */

    /* allocate memory for decoded message and encode */
    decoded = malloc(len * sizeof(char));
    memset(decoded, 0, len);
    decode(decoded, len, buffer, key);

    /* fire it back to the patient client */
    write(sockfd, decoded, len);
    free(decoded);
}


/* Determines a port for future comms with the client and starts listening
 * on a unused port
 */
int propose_port(int sockfd, int oldportno)
{
    /* new socket for client to use */
    int newsockfd;

    /* new port for the socket */
    int newportno = rand() % 10000 + 50000;

    /* buffer to hold string representation of port */
    char portbuf[6];

    socklen_t clilen;
    struct sockaddr_in cli_addr;

    /* try to use the generated port */
    newsockfd = listen_port(newportno);

    /* if not successful, keep trying new ports till one works */
    while (newsockfd < 0) {
        newportno = rand() % 10000 + 50000;
        newsockfd = listen_port(newportno);
    }

    /* convert port number to string */
    snprintf(portbuf, sizeof(portbuf), "%d", newportno);

    /* send client the port to use from now on */
    write(sockfd, portbuf, sizeof(portbuf) - 1);

    /* shut down the current socket, over and out */
    close(sockfd);
    return newsockfd;
}
    

int main(int argc, char *argv[])
{
    /* socket file descriptors and ports */
    int servsockfd, consockfd, clisockfd, portno, newportno;
    socklen_t clilen;
    struct sockaddr_in cli_addr;

    /* handshake signatures */
    char sig[] = "I am otp_dec";
    char resp_sig[] = "I am otp_dec_d";

    /* for tracking children */
    pid_t pid, *bg_pids;
    int num_bg = 0;
    int max_bg = 10;
    
    bg_pids = malloc(max_bg * sizeof(pid_t));

    /* check command line options */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    srand(time(0));

    /* get and use the port passed as argument to listen on new socket */
    portno = atoi(argv[1]);
    servsockfd = listen_port(portno);

    /* check reason for failure to listen on new socket, if any */
    switch (servsockfd) {
        case -3: {
            fprintf(stderr, "otp_enc_d: unable to listen on port %d\n", portno);
            close(servsockfd);
            exit(EXIT_FAILURE);
        }
        case -2: {
            fprintf(stderr, "otp_enc_d: unable to bind socket on port ");
            fprintf(stderr, "%d\n", portno);
            close(servsockfd);
            exit(EXIT_FAILURE);
        }
        case -1: {
            fprintf(stderr, "otp_enc_d: unable to create socket on port ");
            fprintf(stderr, "%d\n", portno);
            exit(EXIT_FAILURE);
        }
        default:
            break;
    }

    /* wait for client connections */
    consockfd = accept(servsockfd, (struct sockaddr *) &cli_addr, &clilen);

    /* got one, take care of the client and pass it off to a forked child,
       then wait for the next client */
    while (consockfd >= 0) {
        /* try to handshake the client to make sure it's correct */
        if (handshake(consockfd, sig, resp_sig, sizeof(resp_sig))) {
            /* if so, propose a port for future communications and close
               up the current socket */
            consockfd = propose_port(consockfd, portno);

            /* entrust a child process with the client */
            pid = fork();

            /* check whether process is child or parent */
            switch (pid) {
                /* failed fork */
                case -1: {
                    fprintf(stderr, "otp_enc_d: failed for fork child\n");
                    close(consockfd);
                    close(servsockfd);
                    exit(EXIT_FAILURE);
                }
                /* child process */
                case 0: {
                    /* accept the client */
                    int accsockfd = accept(consockfd,
                            (struct sockaddr *) &cli_addr, &clilen);

                    close(consockfd);

                    /* get the data, decrypt, and send it back */
                    process(accsockfd);
                    close(accsockfd);
                    exit(EXIT_SUCCESS);
                }
                default: {
                    /* add the new child to the list */
                    if (num_bg == max_bg) {
                        max_bg += 10;
                        if (!realloc(bg_pids, max_bg * sizeof(pid_t))) {
                            free(bg_pids);
                            exit(EXIT_FAILURE);
                        }
                    }
                    bg_pids[num_bg++] = pid;

                    /* clean up finished children */        
                    bg_check(&bg_pids, &num_bg, max_bg);
                    break;
                }
            }
        }
        
        /* close the socket opened for a client */
        close(consockfd);

        /* wait for a new client to connect */
        consockfd = accept(servsockfd,
                (struct sockaddr *) &cli_addr, &clilen);
    }
  
    return EXIT_SUCCESS;
}
