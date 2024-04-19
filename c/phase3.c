/*
 *
Name: Michael Kausch
Assignment: Project
date: 3/7/24
professor: Gordon
class: Operating Systems cmps3600

TODO:

Convert to pipes
Refactor a bit

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
#include <sys/stat.h>

// #define DEBUG
// #define BOX_DEBUG

#ifndef NUM_BOXES
#define NUM_BOXES 5
#endif //  NUM_BOXES

#define SEND 1
#define RCV 0

typedef enum {false, true} bool;
enum MsgType { PTOC = 25, CTOP = 50, CLICK=10, KILL_SIG=20, REMOVE_CHILD=30, COLOR_CHANGE=40, MOVE=50, COLOR_SIG=60, NONE=99};

typedef struct {
    int x;
    int y;
} Vec2;

typedef struct {
    enum MsgType t;
    int box_color;
    int text_color;
    int box_num;
    Vec2 pos;
    Vec2 dim;
} MsgData;

struct Box {
    Vec2 pos;
    Vec2 dim;
    Vec2 dir;
    int color;
    int text_color;
    int winner;
    int cpid;
    int enabled;
} boxes[NUM_BOXES], boxy;

struct Global {
    Display *dpy;
    Window win;
    GC gc;
    int scr;
    Vec2 screenResolution;
    XWindowAttributes xwa;
    int xres, yres;

    int mx, my; // mouse positions
    int background_color;
    int text_color;
    int isClickingOnBox; // whether or not the user is currently clicking mouse on window
    Vec2 myPos;

    // int my_shmid;
    int isParent; // parent controls all child windows
    bool isShowingBoxes;
    int num_children;
    int p2c_pipes[2];
    int c2p_pipes[2];
    int thread_active;
    char fname[128];    // file path from main
    char ** envp;       // environment params 
   
    // mask vars
    struct sigaction sa;
    sigset_t mask;
    pthread_t tid[2];
	pthread_mutex_t mut;
    // int wait;

} g;

void x11_cleanup_xwindows(void);
void x11_init_xwindows(void);
void x11_clear_window(void);
int check_mouse(XEvent *e);
int check_keys(XEvent *e);
void render(void);
void physics(void);
void x11_setFont(unsigned int idx);
void *readerThread(void* n);
void *getWindowCoords(void* n);
void init_globals();
void makePipes();
void start_child_win(int idx);
void sigusr1_handler(int sig);
int check_winner(void);
// void randomize_colors();
void rand_color();
void createChildWindows(void);
void floorCeil(struct Box *b);
Vec2 box2Screen(const MsgData * bcd);
Vec2 screen2Box(const MsgData * bcd);
MsgData checkBoxClick(int mx, int my);

int main(int argc, char *argv[], char *envp[]) {

    g.envp = envp;
    strcpy(g.fname, argv[0]);

    /*   char *argv[] = {g.fname, read_fd_buf, write_fd_buf, NULL};     */

    // PARSE COMMAND LINE ARGS
    if (argc == 3) {
        // child window - will make thread to look for window coords
        g.p2c_pipes[RCV] = atoi(argv[1]);
        g.c2p_pipes[SEND] = atoi(argv[2]);
        g.isParent = 0;

    } else if (argc == 1) {
        // parent window - will setupMQ
        g.isParent = 1;
    
    } else {
        printf("Usage: %s [pipe-index]\n", argv[0]);
        return 1;
    }

    XEvent e;
    int kdone = 0, mdone = 0;
    x11_init_xwindows();
    init_globals();

    g.thread_active = 1;
    pthread_create(&g.tid[0], NULL, readerThread, (void*)NULL);
    pthread_create(&g.tid[1], NULL, getWindowCoords, (void*)NULL);

    while (!mdone && !kdone) {
        /* Check the event queue */
        while (XPending(g.dpy) && !mdone && !kdone) {
            XNextEvent(g.dpy, &e);
            // check_mouse(&e);
            mdone = check_mouse(&e);
            kdone = check_keys(&e);
        }
        render();
        physics();
        

        // if thread has been deactivated by esc key
        if (g.thread_active == 0)
            // pthread_kill(g.tid, SIGUSR1);
            break;


        usleep(4000);
    }

    #ifdef DEBUG
    printf("%d setting thread_active to 0", g.index);
    fflush(stdout);
    #endif
    
    g.thread_active = 0;

    // void* status;
    // pthread_join(g.tid, &status);
    #ifdef DEBUG
    printf("%d finished joining threads\n", g.index);
    fflush(stdout);
    #endif


    // closePipe();
    #ifdef DEBUG
    printf("%d finished closing pipe", g.index);
    fflush(stdout);
    #endif

    if (g.isParent == 1) {
        // parent

        int wStatus;

        #ifdef DEBUG
        char buf[100];
        pid_t childProcID = 0;
        printf("parent waiting for children\n");
        fflush(stdout);
        #endif

        for(int i = 0; i < NUM_BOXES; i++) {
            #ifdef DEBUG
            childProcID = wait(&wStatus);
            sprintf(buf, "Child (%d) exited with code: %X\n", childProcID, WEXITSTATUS(wStatus)); 
            printf("%s",buf);
            #else
            wait(&wStatus);
            #endif // DEBUG

            g.num_children--;
        }
        #ifdef DEBUG
        printf("num_children left: %d\n", g.num_children);
        #endif // DEBUG

    } 

    x11_cleanup_xwindows();
    printf("                                                   \n"); // clear line from mouse movement print statements
    fflush(stdout);
    exit(0);

}

void makePipes() 
{
    if (g.isParent) {
        int ret;
        ret = pipe((g.p2c_pipes));
        if (ret == -1) {
            perror("p2c pipes");
            exit(1);
        }
        ret = pipe((g.c2p_pipes));
        if (ret == -1) {
            perror("c2p pipes");
            exit(1);
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
    XInitThreads();
    if (!(g.dpy = XOpenDisplay(NULL))) {
        fprintf(stderr, "ERROR: could not open display!\n");
        exit(EXIT_FAILURE);
    }
    g.scr = DefaultScreen(g.dpy);
    g.screenResolution.x = DisplayWidth(g.dpy, g.scr);
    g.screenResolution.y = DisplayHeight(g.dpy, g.scr);
    g.xres = 16*40;
    g.yres = 9*40;
    g.win = XCreateSimpleWindow(g.dpy, RootWindow(g.dpy, g.scr), 1, 1,
            g.xres, g.yres, 0, 0x00FFFFFF, 0x00000000);

    XStoreName(g.dpy, g.win, "mkausch phase2");
    g.gc = XCreateGC(g.dpy, g.win, 0, NULL);
    XMapWindow(g.dpy, g.win);
    XSelectInput(g.dpy, g.win, ExposureMask | StructureNotifyMask |
            PointerMotionMask | ButtonPressMask |
            ButtonReleaseMask | KeyPressMask);

}

// bitwise ands against mask to return a valid color in range
void rand_color() {
    const int MASK = 0x00FFFFFF; // mask of valid bits RGB(255,255,255)
   
    for (int i = 0; i < NUM_BOXES; i++) {
        boxes[i].color = rand()&MASK;
        boxes[i].text_color = rand()&MASK;
    }

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

    const int XPAD = 10, HEIGHT = 80, XSTART = 12, 
          WIDTH = (int)((g.xres - (XSTART) - (NUM_BOXES*XPAD))/NUM_BOXES), 
          YPAD = 20,
          YSTART = (g.yres - HEIGHT - YPAD);

    for (int i = 0; i < NUM_BOXES; i++) {

        boxes[i].dim.x = WIDTH;
        boxes[i].dim.y = HEIGHT;
        boxes[i].pos.x = XSTART + (i * WIDTH) + (i* XPAD);
                    // printf("xpos: %d\n",boxes[i].pos.x);

        boxes[i].pos.y = YSTART;
        boxes[i].color = rand() & 0x00ffffff;
        boxes[i].text_color = rand() & 0x00ffffff;
        boxes[i].enabled = 1;
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

    
    g.isClickingOnBox = 0;
    g.isShowingBoxes = 0;

}

int check_mouse(XEvent *e) {
    static int savex = 0;
    static int savey = 0;
    static MsgData msgD;
    int retVal = 0;
    static int sum = 0;

    int mx = e->xbutton.x;
    int my = e->xbutton.y;

    if (e->type != ButtonPress
            && e->type != ButtonRelease
            && e->type != MotionNotify)
        return 0;
    if (e->type == ButtonPress) {
        if (e->xbutton.button==1) { 
            // if not already showing boxes in child, show boxes
            if (!g.isParent) {
                if (!g.isShowingBoxes) {
                    g.isShowingBoxes = 1;
                    return 0;
                }
            }
            msgD = checkBoxClick(mx, my);
            
            if (g.isParent && g.isShowingBoxes) {

                if (msgD.box_num >= 0 && msgD.box_num < NUM_BOXES) {
                    if (msgD.box_num == (NUM_BOXES)-1) {
                        msgD.t = KILL_SIG;
                        if (--g.num_children <= 0) {
                            g.num_children = 0;
                            g.isShowingBoxes = 0;
                        }
                    }

                    write(g.p2c_pipes[SEND], &msgD, sizeof(msgD));
                }

            } else if (!g.isParent && g.isShowingBoxes 
                        && msgD.box_num >= 0 && msgD.box_num < NUM_BOXES) {
                write(g.c2p_pipes[SEND], &msgD, sizeof(msgD));
            }
          
            return 0; 
        }
        if (e->xbutton.button==3) { return 0;}
    }
    if (e->type == MotionNotify) {
        if (savex != mx || savey != my) {
            /*mouse moved*/
            savex = mx;
            savey = my;
            g.mx = mx;
            g.my = my;
            sum += 1;
            if (g.isParent == 1) {
                int n = sum%3;
                switch (n) {
                    case 0:
                        printf("oOo You're Moving the Mouse over the Parent oOo\r");
                        break;
                    case 1:
                        printf("OoO You're Moving the Mouse over the Parent OoO\r");
                        break;
                    case 2:
                        printf("ooo You're Moving the Mouse over the Parent ooo\r");
                        break;
                    default:
                        break;
                }
                fflush(stdout);
            } else {
                int n = sum%3;
                switch (n) {
                    case 0:
                        printf("oOo You're Moving the Mouse over a Child oOo   \r");
                        break;
                    case 1:
                        printf("OoO You're Moving the Mouse over a Child OoO   \r");
                        break;
                    case 2:
                        printf("ooo You're Moving the Mouse over a Child ooo   \r");
                        break;
                    default:
                        break;
                }
                fflush(stdout);
            }
        }
        retVal = 0;
    }

    if (e->type == ButtonRelease) {
        if ((e->xbutton.button == 1)) {
        }
        retVal = 0;
    }

    // printf("Made it to the end of checkMouse");
    return retVal;
}

int check_keys(XEvent *e) {
    int key;

    if (e->type != KeyPress && e->type != KeyRelease)
        return 0;
    key = XLookupKeysym(&e->xkey, 0);
    if (e->type == KeyPress) {
        switch (key) {
            case XK_1:          
                rand_color();
                break;
            case XK_Escape: {

                // only let parent use escape button
                // children must be sent the signal from the parent
                
                MsgData msgD;
                if (g.isParent) {

                    #ifdef DEBUG
                    printf("sending a kill sig...\n");
                    #endif // DEBUG                    
                    
                    msgD.t = KILL_SIG;
                    for (int i = 0; i < g.num_children; i++) {
                        msgD.box_num = i;

                        #ifdef DEBUG
                        printf("sending kill sig to %d on fd: %d", i, g.p2c_pipes[SEND]); fflush(stdout);
                        #endif // DEBUG
                        
                        write(g.p2c_pipes[SEND], &msgD, sizeof(msgD));
                    }
                    // g.thread_active = 0;
                    return 1;
                } else {
                    #ifdef DEBUG
                    printf("quiting child only...\n");
                    #endif // DEBUG  
                    msgD.t = REMOVE_CHILD;
                    write(g.c2p_pipes[SEND], &msgD, sizeof(msgD));

                    return 1;

                }
            }
            
            case XK_c:
                if (g.isParent && !g.isShowingBoxes) {
                    int cpid = fork();
                    if (cpid == 0) {
                        start_child_win(g.num_children);
                    } else {
                        // record all the cpids of the children
                        // close(g.p2c_pipes[g.num_children][RCV]);
                        // close(g.c2p_pipes[g.num_children][SEND]);
                        g.isShowingBoxes = true;
                        g.num_children++;

                        #ifdef DEBUG
                        printf("g.num_children: %d\n",g.num_children); fflush(stdout);
                        #endif // DEBUG
                        
                        MsgData msgD;
                        msgD.t = MOVE;
                        msgD.pos = g.myPos;
                        write(g.p2c_pipes[SEND], &msgD, sizeof(msgD));
                        // printf("g.num_children: %d\n",g.num_children);
                        // fflush(stdout);
                    }
                }
                break;
        }
    }
    return 0;
}

void start_child_win(int idx) {
    char read_fd_buf[16];
    char write_fd_buf[16];


    sprintf(read_fd_buf, "%d", g.p2c_pipes[RCV]);
    sprintf(write_fd_buf, "%d", g.c2p_pipes[SEND]);

    char *argv[] = {g.fname, read_fd_buf, write_fd_buf, NULL};
    execve(g.fname, argv, g.envp);
}

void render(void) {
    char buf[128];
    char buf2[128];
    char buf3[128];
    char buf4[32];
   

    if (g.isParent == 1 ) {
        // parent after children spawn with controls
        strcpy(buf, "Parent Window");
        if (g.isShowingBoxes) {
            memset(buf2, 0, 128);
        } else {
            strcpy(buf2, "Press c to spawn a child window");

        }
        sprintf(buf3, "Window Coords: (%d, %d)", g.myPos.x, g.myPos.y);
    } else {
        // children
        strcpy(buf, "Child Window");
        if (g.isShowingBoxes) {
            memset(buf2, 0, 128);
        } else {
            strcpy(buf2, "Left Mouse Click to spawn boxes");

        }
        sprintf(buf3, "Window Coords: (%d, %d)", g.myPos.x, g.myPos.y);
    }

    // draw background
    XSetForeground(g.dpy, g.gc, g.background_color); 
    XFillRectangle(g.dpy, g.win, g.gc, 0, 0, g.xres, g.yres);

    if (g.isShowingBoxes) {
        for (int i = 0; i < NUM_BOXES; i++) {
            // printf("xpos: %d\n", boxes[i].pos.x);
            if (boxes[i].enabled) {
                sprintf(buf4, "(%d,%d)", boxes[i].pos.x, boxes[i].pos.y);
                XSetForeground(g.dpy, g.gc, boxes[i].color);
                XFillRectangle(g.dpy, g.win, g.gc, boxes[i].pos.x, boxes[i].pos.y, boxes[i].dim.x, boxes[i].dim.y);
                XSetForeground(g.dpy, g.gc, boxes[i].text_color);
                x11_setFont(3);
                XDrawString(g.dpy, g.win, g.gc, 
                                (boxes[i].pos.x)+5, (boxes[i].pos.y)+40, buf4, strlen(buf4));
            }
        }
    }


    // draw text
    XSetForeground(g.dpy, g.gc, g.text_color);
    x11_setFont(14);
    if (strlen(buf) > 0)
        XDrawString(g.dpy, g.win, g.gc, (g.xres/2)-50, (g.yres/2), buf, strlen(buf));
    if (strlen(buf2) > 0)
        XDrawString(g.dpy, g.win, g.gc, (g.xres/2)-80, (g.yres/2)+20, buf2, strlen(buf2));
    if (strlen(buf3) > 0)
        XDrawString(g.dpy, g.win, g.gc, (g.xres/2)-80, (g.yres/2)+40, buf3, strlen(buf3));


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
void *readerThread(void* n) {

    MsgData msgD;

    if (g.isParent)
        makePipes();
    
    g.thread_active = 1;

    while(g.thread_active) {

        // respond to pipe
        if (g.isParent) {
            while (read(g.c2p_pipes[RCV], &msgD, sizeof(msgD)) > 0) {
                // switch on message type
                #ifdef DEBUG
                printf("got a message in the parent\n");
                #endif // DEBUG
                
                switch (msgD.t)
                {
                case REMOVE_CHILD: {
                    // parent - move above where child is
                    g.num_children--;
                    if (--g.num_children <= 0) {
                        g.num_children = 0;
                        g.isShowingBoxes = 0;
                    }

                    break;
                }
                case COLOR_CHANGE: {
                    #ifdef DEBUG
                    printf("parent got a COLOR CHANGE message from %d\n",msgD.box_index);
                    #endif // DEBUG
                    boxes[msgD.box_num].color = msgD.box_color;
                    boxes[msgD.box_num].text_color = msgD.text_color;

                    break;
                }
                case COLOR_SIG: {
                    #ifdef DEBUG
                    printf("parent got a COLOR_SIG message from %d\n",msgD.box_index);
                    #endif // DEBUG
                    g.background_color = msgD.box_color;
                    g.text_color = msgD.text_color;
                    break;
                }
                default:
                    break;
                }
            }

        } else {
            // child
            // bool coordsChanged = false;
            while (read(g.p2c_pipes[RCV], &msgD, sizeof(msgD)) > 0) {
                    // switch on message

                #ifdef DEBUG
                printf("got a message in the child\n");
                #endif // DEBUG                    

                switch (msgD.t)
                {

                case KILL_SIG: {
                    g.thread_active = 0;
                    break;
                }
                case COLOR_SIG: {
                    #ifdef DEBUG
                    printf("parent got a COLOR_SIG message from %d\n",msgD.box_index);
                    #endif // DEBUG

                    g.background_color = msgD.box_color;
                    g.text_color = msgD.text_color;
                    break;
                }
                case MOVE: {
                // child - move above where parent is
                    Vec2 newCoords = {msgD.pos.x, msgD.pos.y+g.yres};
                    XMoveWindow(g.dpy, g.win, newCoords.x, newCoords.y);
                    break;
                }
                default:
                    break;
                }
               
            }

        }
         usleep(4000);
    }

    return (void*)0;
}


MsgData checkBoxClick(int mx, int my) {
    MsgData m = {   .t = COLOR_SIG,
        .box_color = -1, 
        .text_color = -1,
        .box_num = -1};

    // iterate over all colored boxes on parent
    for (int i = 0; i < NUM_BOXES; i++) {
        if (((mx > boxes[i].pos.x) && (mx < (boxes[i].pos.x + boxes[i].dim.x)))
                && ((my > boxes[i].pos.y) && (my < (boxes[i].pos.y + boxes[i].dim.y)))) {
            m.box_color = boxes[i].color;
            m.text_color = boxes[i].text_color;
            m.box_num = i;

            return m;
        }
    }
    return m;
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

// TODO:
// need to translate between box and screen space coords
// currently makes incorrect assumptions

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

// comes in as box position, leaves as screen position
Vec2 box2Screen(const MsgData * msgD)
{
    float newX = ((msgD->pos.x)*(g.screenResolution.x))/(g.xres*1.0);
    float newY = ((msgD->pos.y)*(g.screenResolution.y))/(g.yres*1.0);

    Vec2 ret = {(int)newX, (int)newY};

    return ret;
}

// comes in as screen position, leaves as box position 
Vec2 screen2Box(const MsgData * msgD)
{

    float newX = ((msgD->pos.x)*(g.xres))/(g.screenResolution.x*1.0);
    float newY = ((msgD->pos.y)*(g.yres))/(g.screenResolution.y*1.0);

    Vec2 ret = {(int)newX, (int)newY};

    return ret;
}

void *getWindowCoords(void* n) {
    Window root = DefaultRootWindow(g.dpy);
    Window child;
    MsgData msgD;
    static bool init = false;

    static Vec2 savedWinCoords = {-1, -1};

    while(g.thread_active) {
        // get coords
        XGetWindowAttributes(g.dpy, root, &g.xwa);
        XTranslateCoordinates(g.dpy, g.win, root, g.xwa.x, g.xwa.y, &g.myPos.x, &g.myPos.y, &child);
        
        if (!init) {
            savedWinCoords = g.myPos;
            init = true;
        }

        if ((savedWinCoords.x != g.myPos.x)
            || (savedWinCoords.y != g.myPos.y)) {

            savedWinCoords = g.myPos;
            msgD.t = MOVE;
            msgD.pos = g.myPos;

            if (g.isParent) {
                // send update message to child
                write(g.p2c_pipes[SEND], &msgD, sizeof(msgD));
            } 
            // else {
            //     write(g.c2p_pipes[SEND], &msgD, sizeof(msgD));
            // }
        }
        usleep(4000);
    }


    return (void*)0;
}