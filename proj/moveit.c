/*
 *
Name: Michael Kausch
Assignment: Project
date: 3/7/24
professor: Gordon
class: Operating Systems cmps3600


TODO:

need 4 mutexes - one for each window
read and write to the shared memory
need to be able to handle order for painters algorithm
    --> last touched is drawn last?


*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <unistd.h>
#include <time.h>
#include <sys/msg.h>
#include <pthread.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <sys/ipc.h>

#define DEBUG
// #define BOX_DEBUG

#ifndef NUM_BOXES
#define NUM_BOXES 4
#endif //  NUM_BOXES

// #ifndef MAX_CHILDREN
// #define MAX_CHILDREN 4
// #endif // !MAX_CHILDREN

typedef enum {false, true} bool;
enum MsgType { PTOC = 1, CTOP = 2, CLICK=10, KILL_SIG=20, REMOVE_CHILD=30, MAKE_CHILD=40, NONE=99};

typedef struct {
    int x;
    int y;
} Vec2;

typedef struct {
    enum MsgType t;
    int box_color;
    int text_color;
    int pid;
    int box_index;
} BoxClickData;

struct WinMsg{
    long type;
    BoxClickData m;
} mymsg;

struct Box {
    Vec2 pos;
    Vec2 dim;
    Vec2 dir;
    int color;
    int text_color;
    int winner;
    int cpid;
} *boxes[NUM_BOXES], *myBox, boxy;

struct Global {
    Display *dpy;
    Window win;
    GC gc;
    XWindowAttributes xwa;
    int xres, yres;

    int mx, my; // mouse positions
    int background_color;
    int text_color;
    int isClicking; // whether or not the user is currently clicking mouse on window
    Vec2 myPos;

    int mqid;
    // int my_shmid;
    int isParent; // parent controls all child windows
    int num_children;
    int index;
    int cpid_buf[NUM_BOXES]; // has all the cpids of children (max 20)
                                // not really used now, probably will send child
                                // specific signals with them
    int shmid;   // holds all the msgids
    int thread_active;

    char fname[128];    // file path from main
    char ** envp;       // environment params 
   
    // mask vars
    struct sigaction sa;
    sigset_t mask;
    pthread_t tid;
	pthread_mutex_t mut;

} g;

int fd;

void x11_cleanup_xwindows(void);
void x11_init_xwindows(void);
void x11_clear_window(void);
int check_mouse(XEvent *e);
int check_keys(XEvent *e);
void render(void);
void physics(void);
void x11_setFont(unsigned int idx);
void getWindowCoords(void);
void init_globals();
void setupMQ(void);
void teardownMQ(void);
void setupSHM(void);
void attachSHM(void);
void detachSHM(struct Box* b);
void teardownSHM(void);
void start_child_win(void);
void checkMSG(void);
void sigusr1_handler(int sig);
int check_winner(void);
void randomize_colors();
void createChildWindows(void);
void floorCeil(struct Box *b);
bool checkBoxClick(int mx, int my, BoxClickData *bcd);

int main(int argc, char *argv[], char *envp[]) {

    g.envp = envp;
    strcpy(g.fname, argv[0]);

    // PARSE COMMAND LINE ARGS
    if (argc == 4) {
        // child window - will make thread to look for window coords
        g.mqid = atoi(argv[1]);
        g.shmid = atoi(argv[2]);
        g.index = atoi(argv[3]);
        g.isParent = 0;
        char logname[20];
        sprintf(logname, "log%s", argv[1]);
        fd = open(logname, O_CREAT | O_WRONLY | O_TRUNC , 0644);
        if (fd < 0) {
            perror("open");
            exit(EXIT_FAILURE);
        }  

        write(fd, logname, strlen(logname));


    } else if (argc == 1) {
        // parent window - will setupMQ
        g.isParent = 1;
    
    } else {
        printf("Usage: %s [[mqid] [shmid] [index]]\n", argv[0]);
        printf("if mqid or shmid are provided then they BOTH MUST be provided\n");
    }

    XEvent e;
    int kdone = 0, mdone = 0;
    x11_init_xwindows();

    init_globals();
    // pthread_t tid;
    


    if (g.isParent == 1) {
        setupSHM(); // must be setup before they can be initialized in init_globals
        setupMQ();
        createChildWindows();
        pthread_mutex_init(&g.mut, NULL);
    } else {
        // g.thread_active = 1;
        // pthread_create(&g.tid, NULL, (void*)checkMSG, NULL);
        
        attachSHM();
        printf("done attaching\n");
        // pthread_create(&g.tid, NULL, (void*)getWindowCoords, NULL);

    }

    while (!mdone && !kdone) {
        /* Check the event queue */
        while (XPending(g.dpy)) {
            XNextEvent(g.dpy, &e);
            mdone = check_mouse(&e);
            kdone = check_keys(&e);
        }
        render();
        physics();
        checkMSG();
        
        usleep(4000);
    }

    // printf("exited main while loop\n");
    // fflush(stdout);

    x11_cleanup_xwindows();

    if (g.isParent == 1) {
        // parent

        int wStatus;
        char buf[100];
        pid_t childProcID = 0;

        for(int i = 0; i < NUM_BOXES; i++) {
            childProcID = wait(&wStatus);
            sprintf(buf, "Child (%d) exited with code: %X\n", childProcID, WEXITSTATUS(wStatus)); 
            printf("%s",buf);
            g.num_children--;
        }
        printf("num_children left: %d\n", g.num_children);
        teardownMQ();
        teardownSHM();
        exit(0);
    } else {
        // child

        g.thread_active = 0;
        // printf("trying to join threads\n");
        // fflush(stdout);
        // pthread_kill(g.tid, SIGUSR1);
        // pthread_join(g.tid, &status);
        #ifdef DEBUG
        printf("finished joining threads\n");
        fflush(stdout);
        #endif

        detachSHM(myBox);
        exit(0);
    }
    
}

void createChildWindows(void)
{
    pid_t childProcID;
    memset(g.cpid_buf, 0, NUM_BOXES*sizeof(int));

 
    for (int i = 0; i < NUM_BOXES; i++) {
        childProcID = fork();
        if (childProcID == 0) {
            // child
            printf("from child, g.shmid_buf[i]: %d\n", g.shmid);
            start_child_win();
        } else {
            g.cpid_buf[g.num_children++] = childProcID;
            boxes[i]->cpid = childProcID;
        }

    }
}

void x11_cleanup_xwindows(void)
{
    XDestroyWindow(g.dpy, g.win);
    XCloseDisplay(g.dpy);
}

void x11_init_xwindows(void)
{
    int scr;
    if (!(g.dpy = XOpenDisplay(NULL))) {
        fprintf(stderr, "ERROR: could not open display!\n");
        exit(EXIT_FAILURE);
    }
    scr = DefaultScreen(g.dpy);
    g.xres = 400;
    g.yres = 400;
    g.win = XCreateSimpleWindow(g.dpy, RootWindow(g.dpy, scr), 1, 1,
            g.xres, g.yres, 0, 0x00FFFFFF, 0x00000000);

    XStoreName(g.dpy, g.win, "mkausch phase2");
    g.gc = XCreateGC(g.dpy, g.win, 0, NULL);
    XMapWindow(g.dpy, g.win);
    XSelectInput(g.dpy, g.win, ExposureMask | StructureNotifyMask |
            PointerMotionMask | ButtonPressMask |
            ButtonReleaseMask | KeyPressMask);

}

void randomize_colors() {
    int MAX_COLOR = 0x00FFFFFF;
    for (int i = 0; i < NUM_BOXES; i++) {
        boxes[i]->color = rand()&MAX_COLOR;
        boxes[i]->text_color = rand()&MAX_COLOR;
    }

}

// bitwise ands against mask to return a valid color in range
int rand_color() {
    int MAX_COLOR = 0x00FFFFFF; // mask of valid bits RGB(255,255,255)
    return rand()&MAX_COLOR;    
}

void init_globals()
{
    srand(time(NULL));



    g.num_children = 0;

    // set some initial colors for windows
    if (g.isParent == 1) {
        g.background_color = 0x00FFC72C;
        g.text_color = 0x00000000;
    } else {
        g.background_color = 0x00003594;
        g.text_color = 0x00ffffff;
    }

    // block sigusr1 signals to all threads
    // child msg thread will open signal for sigusr1
    sigemptyset(&g.mask);
    sigaddset(&g.mask, SIGUSR1);
    // pthread_sigmask(SIG_BLOCK, &g.mask, NULL);
    sigprocmask(SIG_BLOCK, &g.mask, NULL);


    g.sa.sa_handler = sigusr1_handler;
    // block all signals while in signal handler
    sigfillset(&g.sa.sa_mask);
    g.sa.sa_flags = 0;

    // register sigaction struct with signal handler
    if (sigaction(SIGUSR1, &g.sa, NULL) == -1) {
        perror("sigaction SIGUSR1");
        exit(1);
    }


    // init boxy if child

    if (g.isParent == 0) {
        boxy.dim.x = boxy.dim.y = 80;
        boxy.pos.x = rand() % (g.xres - boxy.dim.x);
        boxy.pos.y = rand() % (g.yres - boxy.dim.y);
        boxy.color = g.text_color;
        boxy.text_color = g.background_color;
        boxy.dir.x = (rand() % 2 == 0) ? 1 : -1;
        boxy.dir.y = (rand() % 2 == 0) ? 1 : -1;
        boxy.winner = 0;
    }

    
    g.isClicking = 0;

}

int check_mouse(XEvent *e) {
    static int savex = 0;
    static int savey = 0;
    static BoxClickData m;

    int mx = e->xbutton.x;
    int my = e->xbutton.y;

    if (e->type != ButtonPress
            && e->type != ButtonRelease
            && e->type != MotionNotify)
        return 0;
    if (e->type == ButtonPress) {
        if (e->xbutton.button==1) { 
            // check for clicking boxes
            if (g.isParent == 1) {
                checkBoxClick(mx, my, &m);
                if (m.t == CLICK) {

                    #ifdef DEBUG
                    printf("You clicked on box w/ pid: %d... setting isClick\n", m.pid);
                    fflush(stdout);
                    #endif
                    
                    g.isClicking = 1;
                    printf("mx: %d my: %d\n", g.mx, g.my);
                } 
                return 0;
            }
        }
        if (e->xbutton.button==3) { return 0;}
    }
    if (e->type == MotionNotify) {
        if (savex != mx || savey != my) {
            /*mouse moved*/
            g.mx = mx;
            g.my = my;
            if (g.isParent == 1) {
                // check if mouse is currently clicked
                if (g.isClicking) {
                    // move box in window
                    Vec2 delta = {(savex - g.mx), (savey - g.my)};
                    pthread_mutex_lock(&g.mut);
                    boxes[m.box_index]->pos.x -= delta.x;
                    boxes[m.box_index]->pos.y -= delta.y;
                    floorCeil(boxes[m.box_index]);
                    pthread_mutex_unlock(&g.mut);
                }

            } else {
                    //update box


            }
            savex = mx;
            savey = my;
        }
        return 0;
    }
    if (e->type == ButtonRelease) {
        if (e->xbutton.button == 1) {
            g.isClicking = 0;
            
            #ifdef DEBUG
            printf("unsetting isClick\n");
            fflush(stdout);
            #endif

        }
    }
    return 0;
}

int check_keys(XEvent *e) {
    int key;

    if (e->type != KeyPress && e->type != KeyRelease)
        return 0;
    key = XLookupKeysym(&e->xkey, 0);
    if (e->type == KeyPress) {
        switch (key) {
            case XK_1:          
                randomize_colors();
                break;
            case XK_Escape:
                // only let parent use escape button
                // children must be sent the signal from the parent
                if (g.isParent == 1) {

                    mymsg.type = (long)PTOC;
                    mymsg.m.t = KILL_SIG;
                    for (int i = 0; i < g.num_children; i++) {
                        msgsnd(g.mqid, &mymsg, sizeof(mymsg), 0);
                    }

                    return 1;
                } else if (g.isParent == 0) {

                    mymsg.type = (long)CTOP;
                    mymsg.m.t = REMOVE_CHILD;
                    msgsnd(g.mqid, &mymsg, sizeof(mymsg), 0);
                    //g.thread_active = 0;

                    // printf("sending pthread_kill\n");
                    // fflush(stdout);
                    // pthread_kill(g.tid, SIGUSR1);

                    return 1;
                }
            case XK_c:
                
                break;
        }
    }
    return 0;
}

void start_child_win() {
    // char timer[12];
    char mqid[12];
    char sBuf[12];
    char iBuf[12];

    // make str of timer and cpid
    // sprintf(timer, "%d", g.mytimer); 
    sprintf(mqid, "%d", g.mqid);
    sprintf(sBuf, "%d", g.shmid);
    sprintf(iBuf, "%d", g.index);

    char *argv[] = {g.fname, mqid, sBuf, iBuf, NULL};
    //char *envp[2] = {"DISPLAY=:0.0", NULL};
    execve(g.fname, argv, g.envp);
}

void render(void) {
    char buf[128];
    char buf2[128];

    if (g.isParent == 1 ) {
        // parent after children spawn with controls
        strcpy(buf, "Parent Window");
        strcpy(buf2, "Left-click mouse on boxes");
    } else {
        // children
        strcpy(buf, "Child Window");
        strcpy(buf2, "You can't do much with me");
    }

    // draw background
    XSetForeground(g.dpy, g.gc, g.background_color); 
    XFillRectangle(g.dpy, g.win, g.gc, 0, 0, g.xres, g.yres);

    if (g.isParent == 1) {
        // render boxes
        

        for (int i = 0; i < g.num_children; i++) {
            // printf("xpos: %d\n", boxes[i]->pos.x);
            XSetForeground(g.dpy, g.gc, boxes[i]->color);
            XFillRectangle(g.dpy, g.win, g.gc, boxes[i]->pos.x, boxes[i]->pos.y, boxes[i]->dim.x, boxes[i]->dim.y);
        }


    } 

    // draw text
    XSetForeground(g.dpy, g.gc, g.text_color);
    x11_setFont(14);
    if (strlen(buf) > 0)
        XDrawString(g.dpy, g.win, g.gc, 30, 40, buf, strlen(buf));
    if (strlen(buf2) > 0)
        XDrawString(g.dpy, g.win, g.gc, 60, 90, buf2, strlen(buf2));


    // draw bouncy box if child window
    if (g.isParent == 0) {
        // box
        XSetForeground(g.dpy, g.gc, boxy.color);
        XFillRectangle(g.dpy, g.win, g.gc, boxy.pos.x, boxy.pos.y, boxy.dim.x, boxy.dim.y);
    }
    
}

void physics(void)
{

    // bouncy box animation on the child window
    if (g.isParent == 0) {
        if (!boxy.winner)
            boxy.winner = check_winner();


        if (!boxy.winner) {
            boxy.pos.x += 1 * boxy.dir.x;
            boxy.pos.y += 1 * boxy.dir.y;

            if (boxy.pos.x > (g.xres-boxy.dim.x) || boxy.pos.x < 0) {
                boxy.dir.x *= -1;
            }
            if (boxy.pos.y > (g.yres-boxy.dim.y) || boxy.pos.y < 0) {
                boxy.dir.y *= -1;
            }    

        }
    }
}

int check_winner(void)
{

    int epsilon = 0;
    int xtouch = (g.xres - (boxy.pos.x + boxy.dim.x) <= epsilon) || ((boxy.pos.x) <= epsilon);
    int ytouch = (g.yres - (boxy.pos.y + boxy.dim.y) <= epsilon) || ((boxy.pos.y) <= epsilon);

#ifdef BOX_DEBUG
    printf("(g.xres(%d) - (g.bx(%d) + g.bwidth(%d)) (*%d*) < epsilon(%d) = %d               \n", 
            g.xres, g.bx, g.bwidth, g.xres - (g.bx + g.bwidth), epsilon, 
            (g.xres - (g.bx + g.bwidth) < epsilon)); 

    printf("g.bx(%d) - g.bwidth(%d) (*%d*) < epsilon(%d) = %d               \n", 
            g.bx, g.bwidth, (g.bx - g.bwidth), epsilon, 
            (g.bx + g.bwidth) < epsilon); 

    printf("g.yres(%d) - (g.by(%d) + g.bheight(%d) (*%d*) < epsilon(%d) = %d               \n", 
            g.yres, g.by, g.bheight, g.yres - (g.by + g.bheight), epsilon, 
            (g.yres - (g.by + g.bheight) < epsilon)); 

    printf("g.by(%d) < epsilon(%d) = %d               \n", 
            g.by, epsilon, (g.by)  < epsilon); 

    //if (!g.winner)
        printf("xtouch: %d, ytouch %d             \n", xtouch, ytouch);
#endif

    return (xtouch && ytouch);

}

void x11_setFont(unsigned int idx) {
    char *fonts[] = { "fixed","5x8","6x9","6x10","6x12","6x13","6x13bold",
        "7x13","7x13bold","7x14","8x13","8x13bold","8x16","9x15","9x15bold",
        "10x20","12x24" };
    Font f = XLoadFont(g.dpy, fonts[idx]);
    XSetFont(g.dpy, g.gc, f);
}

// unused as of now... probably will use to move windows from parent
void getWindowCoords(void) {
    Window root = DefaultRootWindow(g.dpy);
    Window child;

    while(true) {
        XGetWindowAttributes(g.dpy, root, &g.xwa);
        XTranslateCoordinates(g.dpy, g.win, root, g.xwa.x, g.xwa.y, &g.myPos.x, &g.myPos.y, &child);
        usleep(4000);
    }

}


bool checkBoxClick(int mx, int my, BoxClickData* bcd) {
    bcd->t = NONE;
    bcd->box_color = -1;
    bcd->text_color = -1;
    bcd->pid = -1;


    // iterate over all colored boxes on parent
    for (int i = 0; i < NUM_BOXES; i++) {
        if (((mx > boxes[i]->pos.x) && (mx < (boxes[i]->pos.x + boxes[i]->dim.x)))
                && ((my > boxes[i]->pos.y) && (my < (boxes[i]->pos.y + boxes[i]->dim.y)))) {
            bcd->t = CLICK;
            bcd->box_color = rand_color();
            bcd->text_color = rand_color();

            bcd->pid = boxes[i]->cpid;
            bcd->box_index = i;
            return true;
        }
    }
    return false;
}

void setupMQ(void) {
    #ifdef DEBUG
    printf("Setting up MQ\n");
    fflush(stdout);
    #endif

    // check for failure when generating ipc key
    g.mqid = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
    if (g.mqid < 0) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }
}

void teardownMQ(void) {
    #ifdef DEBUG
    printf("Tearing Down MQ\n");
    fflush(stdout);
    #endif

    msgctl(g.mqid, IPC_RMID, 0);
} 

void setupSHM(void)
{

    g.shmid = shmget(IPC_PRIVATE, (sizeof(struct Box) * NUM_BOXES), IPC_CREAT | 0666); 
    
    if (g.shmid < 0) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    #ifdef DEBUG
    printf("%d\n", g.shmid);
    #endif

    struct Box *shared = (struct Box*)shmat(g.shmid, (void*)0, 0);
    if (shared == (void *) -1) {
        perror("Error attaching shared memory segment");
        exit(EXIT_FAILURE);
    }

    #ifdef DEBUG
    printf("attached to mem\n");
    fflush(stdout);
    #endif

    int XPAD = 10, HEIGHT = 80, XSTART = 12, YPAD = 20;
    int WIDTH = (int)((g.xres - (XSTART) - (NUM_BOXES*XPAD))/NUM_BOXES);
    int YSTART = (g.yres - HEIGHT - YPAD);
    printf("width: %d\n",WIDTH);


    // init box
    for (int i = 0; i < NUM_BOXES; i++) {
        pthread_mutex_lock (&g.mut);
        boxes[i] = &shared[i];
        boxes[i]->dim.x = WIDTH;
        boxes[i]->dim.y = HEIGHT;
        boxes[i]->pos.x = XSTART + (i * WIDTH) + (i* XPAD);
                    printf("xpos: %d\n",boxes[i]->pos.x);

        boxes[i]->pos.y = YSTART;
        boxes[i]->color = rand_color();
        boxes[i]->text_color = rand_color();
        pthread_mutex_unlock (&g.mut);
        //boxes[i].cpid is initialized when threads are created in main

    }
    #ifdef DEBUG
    printf("finished initing boxes\n");
    fflush(stdout);
    #endif
}

// attach to the memory from the child
void attachSHM(void)
{
    struct Box* shared = (struct Box*)shmat(g.shmid, (void*)0, 0);
    myBox = &(shared[g.index]);
}

void detachSHM(struct Box *b)
{
    shmdt(b);
}

void teardownSHM(void)
{
    // for (int i = 0; i < NUM_BOXES; i++) {
    //     detachSHM(boxes);
    //     shmctl(g.shmid, IPC_RMID, NULL);
    // }
    shmdt(boxes);
    shmctl(g.shmid, IPC_RMID, NULL);

}

void checkMSG() {
    /* 
    TODO:
        CHANGE CTOP TO THE APPROPRIATE SIGNAL TO A WINDOW
            THIS WILL BE THE MESSAGE HANDLER FOR ALL PROCS */

    if (msgrcv(g.mqid, &mymsg, sizeof(mymsg), 
                (long)CTOP, IPC_NOWAIT) > 0) {// on success

        #ifdef DEBUG
        printf("Got a message in parent\n");
        fflush(stdout);
        #endif

        switch (mymsg.m.t) {
            case REMOVE_CHILD:
                g.cpid_buf[--g.num_children] = 0;
                #ifdef DEBUG
                printf("Signal received to remove child\n");
                printf("g.num_children: %d\n",g.num_children);
                fflush(stdout);
                
                #endif
                break;
            
            default:
                break;
        }
    }
}


// signal handler for worker thread to 
// quit while blocking for msgrcv
void sigusr1_handler(int sig) {
    
    #ifdef DEBUG
    char buf[100];
    strcpy(buf, "in signal handler from pthread_kill\n");
    write(2, buf, strlen(buf));
    fflush(stdout);
    #endif
    
    exit(0);
}

void floorCeil(struct Box *b) {
    if (b->pos.x < 0)
        b->pos.x = 0;
    else if (b->pos.x > (g.xres-b->dim.x)) {
        b->pos.x = g.xres-b->dim.x; 
    }
    
    if (b->pos.y < 0)
        b->pos.y = 0;
    else if (b->pos.y > (g.yres-b->dim.y)) {
        b->pos.y = g.yres-b->dim.y; 
    }
}