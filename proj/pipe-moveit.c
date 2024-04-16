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

#define DEBUG
// #define BOX_DEBUG

#ifndef NUM_BOXES
#define NUM_BOXES 4
#endif //  NUM_BOXES

// #ifndef MAX_CHILDREN
// #define MAX_CHILDREN 4
// #endif // !MAX_CHILDREN

typedef enum {false, true} bool;
enum MsgType { PTOC = 25, CTOP = 50, CLICK=10, KILL_SIG=20, REMOVE_CHILD=30, COLOR_CHANGE=40, NONE=99};

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
    Vec2 pos;
} BoxClickData;

struct Box {
    Vec2 pos;
    Vec2 dim;
    Vec2 dir;
    int color;
    int text_color;
    int winner;
    int cpid;
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
    int num_children;
    int index;
    int c2p_fd_FIFO[NUM_BOXES];
    int p2c_fd_FIFO[NUM_BOXES];
    int cpid_buf[NUM_BOXES]; // has all the cpids of children (max 20)
                                // not really used now, probably will send child
                                // specific signals with them
    int thread_active;
    char fname[128];    // file path from main
    char ** envp;       // environment params 
   
    // mask vars
    struct sigaction sa;
    sigset_t mask;
    pthread_t tid;
	pthread_mutex_t mut;
    int wait;

} g;

void x11_cleanup_xwindows(void);
void x11_init_xwindows(void);
void x11_clear_window(void);
int check_mouse(XEvent *e);
int check_keys(XEvent *e);
void render(void);
void physics(void);
void x11_setFont(unsigned int idx);
void *getWindowCoords(void* n);
void init_globals();
void makeFifos();
void initPipe();
void closePipe();
void start_child_win(int idx);
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
    if (argc == 2) {
        // child window - will make thread to look for window coords
        g.index = atoi(argv[1]);
        g.isParent = 0;

    } else if (argc == 1) {
        // parent window - will setupMQ
        g.isParent = 1;
        g.index = -1;
    
    } else {
        printf("Usage: %s [pipe-index]\n", argv[0]);
    }

    XEvent e;
    int kdone = 0, mdone = 0;
    x11_init_xwindows();
    init_globals();
    // pthread_t tid;
    
    // initPipe();

    if (g.isParent == 1) {
        makeFifos();
        // getchar();
        createChildWindows();
    }

    g.thread_active = 1;
    pthread_create(&g.tid, NULL, getWindowCoords, (void*)NULL);

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

    void* status;
    pthread_join(g.tid, &status);
    #ifdef DEBUG
    printf("%d finished joining threads\n", g.index);
    fflush(stdout);
    #endif


    closePipe();
    #ifdef DEBUG
    printf("%d finished closing pipe", g.index);
    fflush(stdout);
    #endif

    if (g.isParent == 1) {
        // parent

        int wStatus;
        char buf[100];
        pid_t childProcID = 0;

        #ifdef DEBUG
        printf("parent waiting for children\n");
        fflush(stdout);
        #endif

        for(int i = 0; i < NUM_BOXES; i++) {
            childProcID = wait(&wStatus);
            sprintf(buf, "Child (%d) exited with code: %X\n", childProcID, WEXITSTATUS(wStatus)); 
            printf("%s",buf);
            g.num_children--;
        }
        printf("num_children left: %d\n", g.num_children);

    } 

    x11_cleanup_xwindows();
    exit(0);

}

void makeFifos() 
{
    char p2c_fifo_buf[100];
    char c2p_fifo_buf[100];

    if (g.isParent) {
        // Parent
        // make and attach to all
        printf("in makeFifos from parent\n");
        for (int i = 0; i < NUM_BOXES; i++) {
            // make  PtoC FIFOs
            int ret;
            printf("index: %d\n", i);
            sprintf(p2c_fifo_buf, "p2c_%i", i);
            sprintf(c2p_fifo_buf, "c2p_%i", i);
            
            ret = mkfifo(p2c_fifo_buf, 0666);
            if (ret == -1) {
                // error making fifo
                // error quit
                perror("fifo-p2c");
                exit(1);
            }
            ret = mkfifo(c2p_fifo_buf, 0666);
            if ( ret == -1) {
                // error making fifo
                // error quit
                perror("fifo-c2p");
                exit(1);
            }
            printf("finished %d\n", i);
        }   

    } 
}

void initPipe()
{
    char p2c_fifo_buf[100];
    char c2p_fifo_buf[100];

    if (g.isParent) {
        // Parent
        // make and attach to all
        printf("in initPipe from parent\n");
        for (int i = 0; i < NUM_BOXES; i++) {
            // make  PtoC FIFOs
            printf("int initPipe, i: %d\n", i);
            sprintf(p2c_fifo_buf, "p2c_%i", i);
            sprintf(c2p_fifo_buf, "c2p_%i", i);

            printf("opening the fifos\n");
            g.c2p_fd_FIFO[i] = open(c2p_fifo_buf, O_RDONLY | O_NONBLOCK); // reads c2p
            g.p2c_fd_FIFO[i] = open(p2c_fifo_buf, O_WRONLY); // writes p2c
        

            if (g.p2c_fd_FIFO[i] < 0) {
                perror("open p2c parent");
                exit(EXIT_FAILURE);
            }
            if (g.c2p_fd_FIFO[i] < 0) {
                perror("open c2p parent");
                exit(EXIT_FAILURE);
            }

            printf("finished %d\n", i);
            // getchar();
        }   

    } else {
        // child
        printf("in initPipe from child\n");

        // just attach to the one the parent made
        sprintf(p2c_fifo_buf ,"p2c_%i", g.index);
        sprintf(c2p_fifo_buf, "c2p_%i", g.index);

        g.p2c_fd_FIFO[g.index] = open(p2c_fifo_buf, O_RDONLY | O_NONBLOCK);  // reads p2c
        g.c2p_fd_FIFO[g.index] = open(c2p_fifo_buf, O_WRONLY);  // write c2p
        

        if (g.p2c_fd_FIFO[g.index] < 0) {
            char b[32];
            sprintf(b, "open p2c child %i", g.index);
            perror(b);
            exit(EXIT_FAILURE);
        }
        if (g.c2p_fd_FIFO[g.index] < 0) {
            char b[32];
            sprintf(b, "open p2c child %i", g.index);
            perror(b);
            exit(EXIT_FAILURE);
        }

    }
}

void closePipe()
{
    char p2c_fifo_buf[100];
    char c2p_fifo_buf[100];

    if (g.isParent) {
        // det
        for (int i = 0; i < NUM_BOXES; i++) {
            sprintf(p2c_fifo_buf, "p2c_%i", i);
            sprintf(c2p_fifo_buf, "c2p_%i", i);

            close(g.c2p_fd_FIFO[i]);
            close(g.p2c_fd_FIFO[i]);
            unlink(p2c_fifo_buf);
            unlink(c2p_fifo_buf);
        }

    } else {
        sprintf(p2c_fifo_buf, "p2c_%i", g.index);
        sprintf(c2p_fifo_buf, "c2p_%i", g.index);

        close(g.c2p_fd_FIFO[g.index]);
        close(g.p2c_fd_FIFO[g.index]);
        unlink(p2c_fifo_buf);
        unlink(c2p_fifo_buf);
    }
}

void createChildWindows(void)
{
    pid_t childProcID;
    memset(g.cpid_buf, 0, NUM_BOXES*sizeof(int));

    for (int i = 0; i < NUM_BOXES; i++) {
        g.wait = 1;
        childProcID = fork();
        if (childProcID == 0) {
            // child
            start_child_win(i);
        } else {
            g.cpid_buf[g.num_children++] = childProcID;
            boxes[i].cpid = childProcID;

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
    g.xres = 480;
    g.yres = 270;
    g.win = XCreateSimpleWindow(g.dpy, RootWindow(g.dpy, g.scr), 1, 1,
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
        g.background_color = rand()&MAX_COLOR;
        g.text_color = rand()&MAX_COLOR;
     
        
        // send on pipe to children

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
    int HEIGHT = 80, XSTART = 12, YPAD = 20;
    int WIDTH = 160;
    int XPAD = (int)((g.xres - (XSTART) - (NUM_BOXES*WIDTH))/NUM_BOXES);
    int YSTART = (g.yres - HEIGHT - YPAD);
    printf("width: %d\n",WIDTH);


    for (int i = 0; i < NUM_BOXES; i++) {

        boxes[i].dim.x = WIDTH;
        boxes[i].dim.y = HEIGHT;
        boxes[i].pos.x = XSTART + (i * WIDTH) + (i* XPAD);
                    printf("xpos: %d\n",boxes[i].pos.x);

        boxes[i].pos.y = YSTART;
        boxes[i].color = rand_color();
        boxes[i].text_color = rand_color();
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

}

int check_mouse(XEvent *e) {
    static int savex = 0;
    static int savey = 0;
    static BoxClickData bcd;
    int retVal = 0;

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
                checkBoxClick(mx, my, &bcd);
                if (bcd.t == CLICK) {

                    #ifdef DEBUG
                    printf("You clicked on box w/ pid: %d... setting isClick\n", bcd.pid);
                    printf("mx: %d my: %d\n", g.mx, g.my);
                    fflush(stdout);
                    #endif
                    
                    g.isClickingOnBox = 1;
                } 
                retVal = 0;
            } else {

                retVal = 0;
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
                if (g.isClickingOnBox) {
                    // move box in window
                    Vec2 delta = {(savex - g.mx), (savey - g.my)};

                    // pthread_mutex_lock(&g.mut);
                    boxes[bcd.box_index].pos.x -= delta.x;
                    boxes[bcd.box_index].pos.y -= delta.y;
                    floorCeil(&(boxes[bcd.box_index]));
                    printf("parent sending with type: <insert type here>");



                }

            } else {
                    //update box


            }
            savex = mx;
            savey = my;
        }
        retVal = 0;
    }

    // bug is in here
    if (e->type == ButtonRelease) {
        if ((e->xbutton.button == 1) && (bcd.t == CLICK)) {
            g.isClickingOnBox = 0;
            int newX = 0, newY = 0;
            printf("m.box_index: %d\n", bcd.box_index);
            // newX = (boxes[bcd.box_index]->pos.x)*((g.screenResolution.x-g.xres)/(g.xres-boxes[bcd.box_index]->dim.x));
            // newY = (boxes[bcd.box_index]->pos.y)*((g.screenResolution.y-g.yres)/(g.yres-boxes[bcd.box_index]->dim.y));

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
                randomize_colors();
                break;
            case XK_Escape:
                // only let parent use escape button
                // children must be sent the signal from the parent
                

                g.thread_active = 0;
                return 1;
            case XK_c:
                
                break;
        }
    }
    return 0;
}

void start_child_win(int idx) {
    char iBuf[12];

    sprintf(iBuf, "%d", idx);

    char *argv[] = {g.fname, iBuf, NULL};
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
        strcpy(buf2, "Left-click mouse on boxes");
        memset(buf3, 0, 128);
    } else {
        // children
        strcpy(buf, "Child Window");
        strcpy(buf2, "You can't do much with me");
        sprintf(buf3, "Window Coords: (%d, %d)", g.myPos.x, g.myPos.y);
    }

    // draw background
    XSetForeground(g.dpy, g.gc, g.background_color); 
    XFillRectangle(g.dpy, g.win, g.gc, 0, 0, g.xres, g.yres);

    if (g.isParent == 1) {
        // render boxes
        

        for (int i = 0; i < g.num_children; i++) {
            // printf("xpos: %d\n", boxes[i].pos.x);
            sprintf(buf4, "(%d,%d)", boxes[i].pos.x, boxes[i].pos.y);
            XSetForeground(g.dpy, g.gc, boxes[i].color);
            XFillRectangle(g.dpy, g.win, g.gc, boxes[i].pos.x, boxes[i].pos.y, boxes[i].dim.x, boxes[i].dim.y);
            XSetForeground(g.dpy, g.gc, 0x00ffffff);
             x11_setFont(3);
            XDrawString(g.dpy, g.win, g.gc, 
                            (boxes[i].pos.x)+5, (boxes[i].pos.y)+40, buf4, strlen(buf4));
        }


    } 

    // draw text
    XSetForeground(g.dpy, g.gc, g.text_color);
    x11_setFont(14);
    if (strlen(buf) > 0)
        XDrawString(g.dpy, g.win, g.gc, 30, 40, buf, strlen(buf));
    if (strlen(buf2) > 0)
        XDrawString(g.dpy, g.win, g.gc, 60, 90, buf2, strlen(buf2));
    if (strlen(buf3) > 0)
        XDrawString(g.dpy, g.win, g.gc, 60, 140, buf3, strlen(buf3));


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
void *getWindowCoords(void* n) {
    Window root = DefaultRootWindow(g.dpy);
    Window child;

    BoxClickData bcd;

    initPipe();

    while(g.thread_active) {
        XGetWindowAttributes(g.dpy, root, &g.xwa);
        XTranslateCoordinates(g.dpy, g.win, root, g.xwa.x, g.xwa.y, &g.myPos.x, &g.myPos.y, &child);
       
        if (g.isParent) {
            for (int i = 0; i < NUM_BOXES; i++) {
                while (read(g.c2p_fd_FIFO[i], &bcd, sizeof(bcd)) > 0) {
                    // switch on message
                        // make sure box indices match for verification?



                        // if color message
                            // change color (one box)



                        // if move message, 
                            // convert to box coords
                            // move box associated with pipe



                        // if quit message
                            // delete the box from the screen


                }
            }
        } else {
            while (read(g.p2c_fd_FIFO[g.index], &bcd, sizeof(bcd)) > 0) {
                    // switch on message
                        // if color message
                            // change color (rand colors)

                        // if move message
                            // convert to screen coords
                            // change position

                        // if kill message
                            // quit thread and terminate process
                        
            }
        }
         usleep(4000);
    }

    return (void*)0;
}


bool checkBoxClick(int mx, int my, BoxClickData* bcd) {
    // bcd->t = NONE;
    bcd->box_color = -1;
    bcd->text_color = -1;
    bcd->pid = -1;


    // iterate over all colored boxes on parent
    for (int i = 0; i < NUM_BOXES; i++) {
        if (((mx > boxes[i].pos.x) && (mx < (boxes[i].pos.x + boxes[i].dim.x)))
                && ((my > boxes[i].pos.y) && (my < (boxes[i].pos.y + boxes[i].dim.y)))) {
            bcd->t = CLICK;
            printf("setting t to: %d\n", bcd->t);
            bcd->box_color = rand_color();
            bcd->text_color = rand_color();

            bcd->pid = boxes[i].cpid;
            bcd->box_index = i;
            return true;
        }
    }
    return false;
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