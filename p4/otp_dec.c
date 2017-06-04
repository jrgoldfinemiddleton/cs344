/* otp_dec.c
 * Author: Jason Goldfine-Middleton
 * Course: CS 344
 */

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/* size of buffer to hold plaintext contents, then key contents */
#define SIZEBUF 100000

/* error exit codes */
#define EBADFILE 1
#define EBADPORT 2

int handshake(int sockfd, const char *sig, size_t sigsz,
        const char *resp_sig, size_t respsz);
int receive(int sockfd, char *buf, size_t bufsz);
int transmit(int sockfd, const char *rfile);
int validate_file(const char *fname);


/* Attempts to send a signature to the server, then reads the
 * signature sent back from the server to determine whether
 * a port number will be forthcoming.  If so, returns the port.
 */
int handshake(int sockfd, const char *sig, size_t sigsz,
        const char *resp_sig, size_t respsz)
{
    char buffer[SIZEBUF];
    char port[6];
    memset(buffer, 0, sizeof(buffer));

    /* send the client signature and read the server's response */
    write(sockfd, sig, sigsz - 1);
    read(sockfd, buffer, respsz - 1);

    /* assuming we connected to correct server, read the port it
       wants to use for future communication */
    if (strcmp(resp_sig, buffer) == 0) {
        memset(port, 0, sizeof(port));
        read(sockfd, port, sizeof(port) - 1);

        /* return the port number */
        return atoi(port);
    }

    /* handshake failed */
    return 0;
}


/* Reads the decrypted message from the server on sockfd into buf */
int receive(int sockfd, char *buf, size_t bufsz)
{
    /* counters */
    size_t rdb = 1, trdb = 0, rem = bufsz - 1;

    /* pointer to next char in buffer to fill */
    char *pos = buf;

    memset(buf, 0, bufsz);
    
    /* loop as long as data is forthcoming */
    while (rdb) {
        rdb = read(sockfd, pos, rem);

        pos += rdb;
        trdb += rdb;
        rem -= rdb;
    }

    /* return the number of bytes read, > 0 is a success */
    return (int) trdb;
}


/* Sends the contents of the file rfile through sockfd to the server */
int transmit(int sockfd, const char *rfile)
{
    char buffer[SIZEBUF];

    /* pointer to next character to send */
    char *cur;

    /* file to read in the data from */
    FILE *rf;

    /* counters */
    size_t rdb, wrb;

    memset(buffer, 0, sizeof(buffer));

    /* open the file and read everything */
    rf = fopen(rfile, "r");
    rdb = fread(buffer, sizeof(char), sizeof(buffer) - 1, rf);

    /* too much information in file, gotta bail out */
    if (!feof(rf)) {
        fclose(rf);
        return 0;
    }
    fclose(rf);

    /* loop through buffer until all data written to socket */
    cur = buffer;
    while (rdb > 0) {
        wrb = write(sockfd, cur, rdb);

        cur += wrb;
        rdb -= wrb;
    }
    
    return 1;
}


/* Verifies that the file contains only characters that can be handled
 * by the server's de-OTP function
 */
int validate_file(const char *fname)
{
    int ch;
    
    /* open the file and peruse the contents */
    FILE *f = fopen(fname, "r");

    if (!f)
        return -1;

    /* anything other than a capital letter, a space, or a newline
       means the file is invalid */
    while ((ch = fgetc(f)) != EOF) {
        if (!((ch >= 'A' && ch <= 'Z') || ch == ' ' || ch == '\n'))
            return 0;
    }

    /* otherwise the file is good */
    fclose(f);
    return 1;
}


int main(int argc, char *argv[])
{
    int sockfd, portno, res;
    struct sockaddr_in serv_addr;
    struct stat st1, st2;
    struct hostent *server;
    int fd_pt, fd_key;

    /* signatures for handshake with server */
    char sig[] = "I am otp_dec";
    char resp_sig[] = "I am otp_dec_d";

    /* place to hold the message and key to send to server */
    char buffer[SIZEBUF];

    /* check for enough arguments */
    if (argc != 4) {
        fprintf(stderr, "Usage: %s plaintext key port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* ensure that first file arg exists */
    if (stat(argv[1], &st1) < 0) {
        fprintf(stderr, "otp_dec: could not access file %s\n", argv[1]);
        exit(EBADFILE);
    }

    /* ensure that second file arg exists */
    if (stat(argv[2], &st2) < 0) {
        fprintf(stderr, "otp_dec: could not access file %s\n", argv[2]);
        exit(EBADFILE);
    }

    /* ensure that key is at least as big as plaintext file */
    if (st1.st_size > st2.st_size) {
        fprintf(stderr, "otp_dec: key file smaller than plaintext file\n");
        exit(EBADFILE);
    }

    /* verify the characters present in the plaintext file */
    if (validate_file(argv[1]) <= 0) {
        fprintf(stderr, "otp_dec: plaintext file %s ", argv[1]);
        fprintf(stderr, "contained invalid characters\n");
        exit(EBADFILE);
    }

    /* verify the characters present in the key file */
    if (validate_file(argv[2]) <= 0) {
        fprintf(stderr, "otp_dec: key file %s ", argv[2]);
        fprintf(stderr, "contained invalid characters\n");
        exit(EBADFILE);
    }

    /* ensure that the port arg is valid */
    portno = atoi(argv[3]);
    if (portno < 1 || portno > 65535) {
        fprintf(stderr, "otp_dec: received an invalid port number\n");
        exit(EBADPORT);
    }

    /* get information about this host */
    if ((server = gethostbyname("localhost")) == NULL) {
        fprintf(stderr, "otp_dec: could not connect to localhost\n");
        exit(EXIT_FAILURE);
    }

    /* try to open a socket to connect with server on */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("otp_dec: could not open socket\n");
        exit(EXIT_FAILURE);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(server->h_addr, &serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    /* attempt to connect to the server */
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "otp_dec: could not connect to server\n");
        close(sockfd);
        exit(EBADPORT);
    }

    /* attempt a signature exchange with server, if all goes well get
       the port number to connect on for data exchange */
    if ((portno = handshake(sockfd, sig, sizeof(sig),
                    resp_sig, sizeof(resp_sig))) <= 0) {
        fprintf(stderr, "otp_dec: failed handshake with server\n");
        close(sockfd);
        exit(EBADPORT);
    }

    /* close the old socket and open up a new one connected to the
       server on the port received */
    close(sockfd);
    server = gethostbyname("localhost");
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    memcpy(server->h_addr, &serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

   if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "otp_dec: could not connect to server ");
        fprintf(stderr, "after successful handshake\n");
        close(sockfd);
        exit(EBADPORT);
    }

    /* write plaintext to socket */
    transmit(sockfd, argv[1]);

    /* write key to socket */
    transmit(sockfd, argv[2]);

    /* read decrypted response from socket */
    res = receive(sockfd, buffer, sizeof(buffer) - 1);
    close(sockfd);

    /* if the server sent no response, we're in trouble */
    if (res == 0) {
        fprintf(stderr, "otp_dec: could not read from socket\n");
        exit(EXIT_FAILURE);
    }

    /* otherwise print the decrypted message */
    printf("%s\n", buffer);
    return EXIT_SUCCESS;
}
