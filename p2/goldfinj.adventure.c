#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>


/* CONSTANTS */

/* total number of rooms in world */
#define NUM_ROOMS 7

/* number of available room names */
#define NUM_NAMES 10

/* maximum length of room name string */
#define MAX_LEN 15

/* fewest connections a room may have to other rooms */
#define MIN_CON 3

/* most connections a room may have to other rooms */
#define MAX_CON 6

/* maximum number of room changes player is allowed */
#define MAX_STEPS 20

/* constants for room types */
#define START_ROOM 0
#define MID_ROOM 1
#define END_ROOM 2

/* list of available room names */
const char *NAMES[10] = { "swoll", "bliq", "hald", "samnfor", "xtor",
                          "mendina", "sent", "yir", "rwyt", "klipt" };

/* prefix for game file directory */
const char *USR = "goldfinj";

/* prefix for game room files */
const char *FILE_PRFX = "room";


/* STRUCTS */

/* represents a room */
struct Room
{
    /* name of room */
    char name[MAX_LEN];

    /* list of room names connected to this room */
    char cons[MAX_CON][MAX_LEN];

    /* set of flags to indicate connected rooms */
    int neighbors[NUM_ROOMS];

    /* total number of other rooms connected to this room */
    int num_cons;

    /* start, mid, or end room */
    int type;
}; 


/* FUNCTIONS */

/* make two rooms connected once they have been numbered */
void connect(struct Room rooms[NUM_ROOMS], int idx1, int idx2);

/* check whether two rooms are connected once they have been numbered */
int connected(struct Room rooms[NUM_ROOMS], int idx1, int idx2);

/* execute game loop until max steps or game won */
void run();

/* prepare the room files for the game */
void set_up();

/* stores name of game file directory in dirname */
void get_game_dir(char *dirname);

/* set up all the connections between the rooms, at random */
void make_rand_cons(struct Room rooms[NUM_ROOMS]);

/* use the room files to set up the internal game engine */
void read_files(struct Room rooms[NUM_ROOMS]);

/* randomly mix up an array of n integers */
void shuffle(int *arr, int n);

/* output the generated room information to files */
void write_files(struct Room rooms[NUM_ROOMS]);


void set_up()
{
    /* set of rooms to build */
    struct Room rooms[NUM_ROOMS];

    /* list with values corresponding to names in NAMES */
    int name_indices[NUM_NAMES];

    /* list with values corresponding to room numbers to
     * assign room types */
    int type_indices[NUM_ROOMS];

    int i;

    /* fill in the indices */
    for (i = 0; i != NUM_NAMES; ++i) {
        name_indices[i] = i;

        /* feed two birds with one bird feeder */
        if (i < NUM_ROOMS)
            type_indices[i] = i;
    }

    /* mix the name indices up */
    shuffle(name_indices, NUM_NAMES);

    /* take the first few indices and give the names in NAMES that
     * correspond with them to the rooms */
    for (i = 0; i != NUM_ROOMS; ++i) {
        strcpy(rooms[i].name, NAMES[name_indices[i]]);
        rooms[i].name[MAX_LEN - 1] = 0;
        //puts(rooms[i].name);
    }

    /* connect the newly named rooms to each other randomly */
    make_rand_cons(rooms);

    /* mix the type indices up */
    shuffle(type_indices, NUM_ROOMS);

    /* assign room types based on the shuffle */
    rooms[type_indices[0]].type = START_ROOM;
    rooms[type_indices[1]].type = END_ROOM;

    for (i = 2; i != NUM_ROOMS; ++i) {
        rooms[type_indices[i]].type = MID_ROOM;
    }


    //for (i = 0; i != NUM_ROOMS; ++i) {
    //    printf("%s: type %i\n", rooms[i].name, rooms[i].type);
    //}

    /* output NUM_ROOMS room files, one per room */
    write_files(rooms);
}


void make_rand_cons(struct Room rooms[NUM_ROOMS])
{
    int i, j;
    
    /* start off each room with no connections */
    for (i = 0; i != NUM_ROOMS; ++i) {
        rooms[i].num_cons = 0;

        for (j = 0; j != NUM_ROOMS; ++j)
            rooms[i].neighbors[j] = 0;
    }

    /* connect rooms randomly, ensuring between MIN_CON and MAX_CON
     * connections per room
     *
     * note: connections are always two-way */
    for (i = 0; i != NUM_ROOMS; ++i) {
        int cons_needed = MIN_CON + rand() % 1;

        /* keep generating connections for current room until
         * expected number is reached */
        while (rooms[i].num_cons < cons_needed) {
            int con_room = rand() % NUM_ROOMS;

            /* never connect a room to itself or try to duplicate a
             * connection */
            if (con_room != i && !connected(rooms, i, con_room))
                connect(rooms, i, con_room);
        }
    }

    //for (i = 0; i != NUM_ROOMS; ++i) {
    //    printf("Room %i neighbors: [ ", i);
    //    for (j = 0; j != NUM_ROOMS; ++j) {
    //        if (rooms[i].neighbors[j])
    //           printf("%i ", j);
    //    }
    //    puts("]");
    //}

    //for (i = 0; i != NUM_ROOMS; ++i) {
    //    printf("Room %s connections: ", rooms[i].name);
    //    for (j = 0; j != rooms[i].num_cons; ++j) {
    //        printf("%s ", rooms[i].cons[j]);
    //    }
    //    puts("");
    //}
}


void connect(struct Room rooms[NUM_ROOMS], int idx1, int idx2)
{
    //assert (rooms[idx1].num_cons < NUM_ROOMS - 1);
    //assert (rooms[idx2].num_cons < NUM_ROOMS - 1);
    
    /* for first room, indicate second is a neighbor and add its
     * name to the list of connections */
    rooms[idx1].neighbors[idx2] = 1;
    strcpy(rooms[idx1].cons[rooms[idx1].num_cons], rooms[idx2].name);
    rooms[idx1].cons[rooms[idx1].num_cons++][MAX_LEN - 1] = 0;

    /* do the analogous operation for the second room */
    rooms[idx2].neighbors[idx1] = 1;
    strcpy(rooms[idx2].cons[rooms[idx2].num_cons], rooms[idx1].name);
    rooms[idx2].cons[rooms[idx2].num_cons++][MAX_LEN - 1] = 0;
}


int connected(struct Room rooms[NUM_ROOMS], int idx1, int idx2)
{
    //assert (rooms[idx1].neighbors[idx2] == rooms[idx2].neighbors[idx1]);

    return rooms[idx1].neighbors[idx2];
}


void shuffle(int *arr, int n)
{
    // citation: http://benpfaff.org/writings/clc/shuffle.html
    int i;
    for (i = 0; i < n - 1; ++i) {
        int j = i + rand() / (RAND_MAX / (n - i) + 1);
        int tmp = arr[j];
        arr[j] = arr[i];
        arr[i] = tmp;
    }
}


void get_game_dir(char *dirname)
{
    /* ensure unique directory name by using process ID */
    sprintf(dirname, "%s.rooms.%i", USR, getpid());
}


void write_files(struct Room rooms[NUM_ROOMS])
{
    /* file directory name */
    char dir[60];

    /* to hold name of current room file */
    char filename[MAX_LEN];

    /* for stat() */
    struct stat st;

    int i, j;

    /* generate appropriate directory name for this program run */
    get_game_dir(dir);
    //puts(dir);

    /* if the directory doesn't exist yet, create it */
    if (stat(dir, &st) == -1)
        mkdir(dir, 0770);

    /* switch into that directory */
    chdir(dir);

    /* create a file for each room and fill it with info in struct */
    for (i = 0; i != NUM_ROOMS; ++i) {
        /* create room file and open it for writing */
        FILE *cur_room;
        sprintf(filename, "%s%i", FILE_PRFX, i);
        cur_room = fopen(filename, "w");

        /* store the room's name */
        fprintf(cur_room, "ROOM NAME: %s\n", rooms[i].name);

        /* store the room's connected rooms */
        for (j = 0; j != rooms[i].num_cons; ++j) {
            fprintf(cur_room, "CONNECTION %i: %s\n", j + 1,
                    rooms[i].cons[j]);
        }

        /* store the room's type */
        fprintf(cur_room, "ROOM TYPE: ");

        switch(rooms[i].type) {
            case START_ROOM: fprintf(cur_room, "START_ROOM\n"); break;
            case END_ROOM: fprintf(cur_room, "END_ROOM\n"); break;
            default: fprintf(cur_room, "MID_ROOM\n"); break;
        }
        
        /* close the file */        
        fclose(cur_room);
    }

    /* return to base directory */
    chdir("..");
}


void run()
{   
    /* set of rooms used by game engine */
    struct Room rooms[NUM_ROOMS];

    /* number of room changes player has made */
    int steps = 0;

    /* indices in room set corresponding to starting location,
     * current location, and winning location */
    int start_room, cur_room, end_room;

    int i, j;

    /* list of indices corrsponding to rooms changed to during game
     *
     * note: excludes the initial location */
    int rooms_visited[MAX_STEPS];
    
    /* populate each room struct with info from stored room files */
    read_files(rooms);

    /* set up initial conditions */
    for (i = 0; i != NUM_ROOMS; ++i) {
        if(rooms[i].type == START_ROOM) start_room = i;
        if(rooms[i].type == END_ROOM) end_room = i;
    }

    /* place the user's current location in the starting room */
    cur_room = start_room;
    
    //printf("start_room: %i\n", start_room);
    //printf("cur_room: %i\n", cur_room);
    //printf("end_room: %i\n", end_room);
    //printf("steps: %i\n", steps);

    /* game loop until player moves into winning room or takes too
     * many steps */
    do {
        int ch = '\0';

        /* buffer to store name of room player wants to move to next */
        char next_room[MAX_LEN];

        /* keep track of current room */
        int prev_room = cur_room;

        /* give player info about current location and options to move */
        printf("CURRENT LOCATION: %s\n", rooms[cur_room].name);
        printf("POSSIBLE CONNECTIONS: ");

        for (i = 0; i != rooms[cur_room].num_cons - 1; ++i)
            printf("%s, ", rooms[cur_room].cons[i]);
        printf("%s.\n", rooms[cur_room].cons[rooms[cur_room].num_cons - 1]);

        /* prompt for name of room to move to */
        printf("WHERE TO? >");

        i = 0;
        while (i < MAX_LEN && ch != EOF && ch != '\n') {
            ch = getchar();
            next_room[i++] = (char) ch;
        }

        if (i == MAX_LEN)
            while ((ch = getchar()) != EOF && ch != '\n');

        next_room[i - 1] = '\0';

        //fgets(next_room, MAX_LEN, stdin);
        //for (i = 0; i != MAX_LEN; ++i)
        //    if (next_room[i] == '\n') {
        //        next_room[i] = 0;
        //        break;
        //    }

        //scanf("%s", &next_room);
        //while((i = getchar()) != EOF);
        puts("");

        /* try to match user input with a connected room's name */
        for (i = 0; i != rooms[cur_room].num_cons; ++i) {
            if (strcmp(next_room, rooms[cur_room].cons[i]) == 0) {
                //printf("%s\n", rooms[cur_room].cons[i]);
                /* move the player to the desired room */
                for (j = 0; j != NUM_ROOMS; ++j)
                    if (strcmp(next_room, rooms[j].name) == 0) {
                        //printf("%s\n", rooms[j].name);
                        cur_room = j;
                    }
                break;
            }
        }

        //printf("cur_room: %i\n", cur_room);
        //fflush(stdout);

        /* if player didn't enter valid connection name,
         * print error message */
        if (cur_room == prev_room)
            printf("HUH? I DON'T UNDERSTAND THAT ROOM. TRY AGAIN.\n\n");
        /* otherwise take a step and add the new room to list of
         * visited rooms */
        else
            rooms_visited[steps++] = cur_room;
    } while (cur_room != end_room && steps <= MAX_STEPS);

    /* if player took too long, she lost */
    if (steps > MAX_STEPS)
        printf("MAN, YOU ARE TAKING FOREVER. YOU DIE, LOST.\n");

    /* otherwise she won, so print out some info */
    else {
        puts("YOU HAVE FOUND THE END ROOM. CONGRATULATIONS!");
        printf("YOU TOOK %i STEPS. YOUR PATH TO VICTORY WAS:\n", steps);

        for (i = 0; i < steps; ++i)
            printf("%s\n", rooms[rooms_visited[i]].name);
    }
}


void read_files(struct Room rooms[NUM_ROOMS])
{
    /* file directory name */
    char dir[60];

    /* to hold name of current room file */
    char filename[MAX_LEN];

    /* for stat() */
    struct stat st;
    int i, j;

    /* get expected game file directory name */
    get_game_dir(dir);
    //puts(dir);

    /* if directory is missing, exit with error */
    if (stat(dir, &st) == -1) {
        puts("The game directory doesn't exist!");
        exit(-1);
    }

    /* switch into that directory */
    chdir(dir);

    /* parse information about each room from files and set up
     * the room structs */
    for (i = 0; i != NUM_ROOMS; ++i) {
        /* descriptor for current room file */
        FILE *cur_room;

        /* temp variables to store parsed info for the room */
        int num_cons;
        char con_name[MAX_LEN];
        char room_type[MAX_LEN];

        /* order rooms the same way they were stored */
        sprintf(filename, "%s%i", FILE_PRFX, i);

        /* open current room file for reading */
        cur_room = fopen(filename, "r");

        /* get the room's name */
        fscanf(cur_room, "ROOM NAME: %s\n", &(rooms[i].name));
        //printf("%s\n", rooms[i].name);

        /* get neighboring room names  */
        while (fscanf(cur_room, "CONNECTION %i: %s\n", &num_cons, &con_name) == 2) {
            rooms[i].num_cons = num_cons;
            strcpy(rooms[i].cons[num_cons - 1], con_name);

        }

        /* get room type */
        fscanf(cur_room, "ROOM TYPE: %s\n", &room_type);

        if (strcmp(room_type, "START_ROOM") == 0)
            rooms[i].type = START_ROOM;
        else if (strcmp(room_type, "END_ROOM") == 0)
            rooms[i].type = END_ROOM;
        else
            rooms[i].type = MID_ROOM;

        /* close the file */
        fclose(cur_room);
    }
}


int main(int argc, char *argv[])
{
    srand(time(0));
    set_up();
    run();

    return 0;
}
