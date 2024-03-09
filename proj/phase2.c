/*
 *
Name: Michael Kausch
Assignment: Project
date: 3/1/24
professor: Gordon
class: Operating Systems cmps3600

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


// #define DEBUG

#ifndef NUM_BOXES
#define NUM_BOXES 10
#endif //  NUM_BOXES

#ifndef MAX_CHILDREN
#define MAX_CHILDREN 20
#endif // !MAX_CHILDREN

enum MsgType { PTOC = 1, CTOP = 2, COLOR_SIG=10, KILL_SIG=20, REMOVE_CHILD=30 };

typedef struct {
    int x;
    int y;
} Vec2;

typedef struct {
    enum MsgType t;
    int box_color;
    int text_color;
    int box_num;
} MsgData;

struct WinMsg{
    long type;
    MsgData m;
} mymsg;

struct Box {
    Vec2 pos;
    Vec2 dim;
    int color;
    int text_color;
} boxes[NUM_BOXES];

struct Global {
    Display *dpy;
    Window win;
    GC gc;
    XWindowAttributes xwa;
    int xres, yres;

    int mx, my; // mouse positions
    int background_color;
    int text_color;

    int mqid;
    int master; // master controls all child windows
    int num_children;
    int cpid_buf[MAX_CHILDREN]; // has all the cpids of children (max 20)
                                // not really used now, probably will send child
                                // specific signals with them
    int thread_active;

    int mytimer;        // current timer // 0 -> infinite
    char fname[128];    // file path from main
    char ** envp;       // environment params so windows can open via ssh 
} g;

void x11_cleanup_xwindows(void);
void x11_init_xwindows(void);
void x11_clear_window(void);
int check_mouse(XEvent *e);
int check_keys(XEvent *e);
void render(void);
void physics(void);
void x11_setFont(unsigned int idx);
void getWindowCoords(int *x, int *y);
void init_globals();
void setupMQ(void);
void teardownMQ(void);
void checkMSG(void);
void start_child_win();
void parentCheckMSG();

MsgData checkBoxClick(int mx, int my);


int main(int argc, char *argv[], char *envp[]) {


    // PARSE COMMAND LINE ARGS
    if (argc == 3) {
        g.mytimer = atoi(argv[1]);
        g.mqid = atoi(argv[2]);
        if ( g.mytimer < 0 ) {
            g.mytimer = 0; 
            printf("Usage: %s [timer duration (positive)]\n", argv[0]);
        }
        g.master = 0;

        // make thread to look for messages
    } else if (argc == 2) {
        g.mytimer = atoi(argv[1]);
        g.master = 1;
        // will create mqid
        if ( g.mytimer < 0 ) {
            g.mytimer = 0; 
            printf("Usage: %s [timer duration (positive)]\n", argv[0]);
        }
    } else if (argc == 1) {
        g.mytimer = 0;
        g.master = 1;
        // will create mqid and no timer
        printf("Usage: %s [timer duration (positive)]\n", argv[0]);

    } else {
        printf("Usage: %s [timer duration (positive)]\n", argv[0]);
    }

    XEvent e;
    int kdone = 0, mdone = 0;
    x11_init_xwindows();
    init_globals();
    pthread_t tid;
    g.envp = envp;
    strcpy(g.fname, argv[0]);

    if (g.master == 1) {
        setupMQ();
    } else {
        g.thread_active = 1;
        pthread_create(&tid, NULL, (void*)checkMSG, NULL);
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
        if (g.mytimer > 0) {
            if (--g.mytimer == 0)
                kdone = 1;
        }
        if ((g.master == 0) && (g.thread_active == 0)) {
            kdone = 1;  // quit msg sent by parents
        }
        if (g.master == 1) {
            parentCheckMSG();
        }
        usleep(4000);
    }

    x11_cleanup_xwindows();

    if (g.master == 1) {
        teardownMQ();
    } else {
        void* status;
        g.thread_active = 0;
        pthread_join(tid, &status);
        #ifdef DEBUG
        printf("finished joining threads\n");
        fflush(stdout);
        #endif
    }
    printf("                                                   \r"); // clear line from mouse movement print statements
    fflush(stdout);

    return 0;
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
    g.yres = 200;
    g.win = XCreateSimpleWindow(g.dpy, RootWindow(g.dpy, scr), 1, 1,
            g.xres, g.yres, 0, 0x00FFFFFF, 0x00000000);

    XStoreName(g.dpy, g.win, "mkausch phase2");
    g.gc = XCreateGC(g.dpy, g.win, 0, NULL);
    XMapWindow(g.dpy, g.win);
    XSelectInput(g.dpy, g.win, ExposureMask | StructureNotifyMask |
            PointerMotionMask | ButtonPressMask |
            ButtonReleaseMask | KeyPressMask);

}

void init_globals()
{
    srand(time(NULL));
    int MAX_COLOR = 0x00FFFFFF;

    // init box stuff
    int colors[NUM_BOXES];
    int text_colors[NUM_BOXES];
    for (int i = 0; i < NUM_BOXES; i++) {
        colors[i] = rand()&MAX_COLOR;
        text_colors[i] = rand()&MAX_COLOR;
    }

    // int colors[5] = { 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00ffff00, 0x00000000};
    // int text_colors[5] = { 0x00ffffff, 0x00000000, 0x00ffffff, 0x00000000, 0x00ffffff};
    // const int WIDTH = 50, HEIGHT = 40, XSTART = 22, 
    //             XPAD = (int)((g.xres - (XSTART) - (5*WIDTH))/NUM_BOXES), 
    //             YPAD = 40,
    //             YSTART = (g.yres - HEIGHT - YPAD);
    const int XPAD = 10, HEIGHT = 80, XSTART = 12, 
          WIDTH = (int)((g.xres - (XSTART) - (NUM_BOXES*XPAD))/NUM_BOXES), 
          YPAD = 20,
          YSTART = (g.yres - HEIGHT - YPAD);

    for (int i = 0; i < NUM_BOXES; i++) {
        boxes[i].dim.x = WIDTH;
        boxes[i].dim.y = HEIGHT;
        boxes[i].pos.x = XSTART + (i * WIDTH) + (i* XPAD);
        boxes[i].pos.y = YSTART;
        boxes[i].color = colors[i];
        boxes[i].text_color = text_colors[i];
    }

    g.num_children = 0;

    // set some initial colors
    if (g.master == 1) {
        g.background_color = 0x00FFC72C;
        g.text_color = 0x00000000;
    } else {
        g.background_color = 0x00003594;
        g.text_color = 0x00ffffff;
    }

}

int check_mouse(XEvent *e) {
    static int savex = 0;
    static int savey = 0;
    static int sum = 0;

    int mx = e->xbutton.x;
    int my = e->xbutton.y;

    if (e->type != ButtonPress
            && e->type != ButtonRelease
            && e->type != MotionNotify)
        return 0;
    if (e->type == ButtonPress) {
        if (e->xbutton.button==1) { 
            // check for clicking boxes
            if (g.master == 1) {
                MsgData m = checkBoxClick(mx, my);
                if ((m.box_num == (NUM_BOXES -1))
                    && (g.num_children > 0) ){

                    // kill children boxes
                    mymsg.type = (long)PTOC;
                    mymsg.m = m;
                    mymsg.m.t = KILL_SIG;
                    // send enough signals for all children
                    // for (int i = 0; i < g.num_children; i++) {
                    #ifdef DEBUG
                    int ret =  msgsnd(g.mqid, &mymsg, sizeof(mymsg), 0);
                    printf("clicked clicked quit box\n");
                    fflush(stdout);
                    if (ret == 0) {
                        printf("msgsnd was successful\n");
                        fflush(stdout);
                    }
                    #endif
                    #ifndef DEBUG
                    msgsnd(g.mqid, &mymsg, sizeof(mymsg), 0);
                    #endif
                    // }
                    // only send signals if there are children so that there
                    // arent erroneous signals just waiting in the msg queue
                    if (g.num_children > 0)
                        g.cpid_buf[--g.num_children] = 0;
                    //memset(g.cpid_buf, 0, sizeof(g.cpid_buf));  // clear cpid list
                    //g.num_children = 0;

                    return 0;
                } else if ((m.box_num >= 0 && m.box_num < (NUM_BOXES-1))
                            && (g.num_children > 0)) {
                    // send msg with box color
                    mymsg.type = (long)PTOC;
                    mymsg.m = m;

                    // for (int i = 0; i < g.num_children; i++) {

                    #ifdef DEBUG
                    int ret =  msgsnd(g.mqid, &mymsg, sizeof(mymsg), 0);
                    printf("clicked a box other than 4\n");
                    fflush(stdout);
                    if (ret == 0) {
                        printf("msgsnd was successful\n");
                        fflush(stdout);
                    }
                    #endif
                    #ifndef DEBUG
                    msgsnd(g.mqid, &mymsg, sizeof(mymsg), 0);
                    #endif

                } 
                return 0;
            }
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
            if (g.master == 1) {
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
        return 0;
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
                break;
            case XK_Escape:
                // only let parent use escape button
                // children must be sent the signal from the parent
                if (g.master == 1) {

                    mymsg.type = (long)PTOC;
                    mymsg.m.t = KILL_SIG;
                    for (int i = 0; i < g.num_children; i++) {
                        msgsnd(g.mqid, &mymsg, sizeof(mymsg), 0);
                    }

                    return 1;
                } else if (g.master == 0) {

                    mymsg.type = (long)CTOP;
                    mymsg.m.t = REMOVE_CHILD;
                    msgsnd(g.mqid, &mymsg, sizeof(mymsg), 0);

                    return 1;
                }
            case XK_c:
                if (g.master == 1 && g.num_children < MAX_CHILDREN) {
                    int cpid = fork();
                    if (cpid == 0) {
                        start_child_win();
                    } else {
                        // record all the cpids of the children
                        g.cpid_buf[g.num_children++] = cpid;
                    }
                }
                break;
        }
    }
    return 0;
}

void start_child_win() {
    char timer[12];
    char mqid[12];

    // make str of timer and cpid
    sprintf(timer, "%d", g.mytimer); 
    sprintf(mqid, "%d", g.mqid);

    char *argv[] = {g.fname, timer, mqid, NULL};
    //char *envp[2] = {"DISPLAY=:0.0", NULL};
    execve(g.fname, argv, g.envp);
}

void render(void) {
    char buf[128];
    char buf2[128];
    char buf3[] = "X";
    char buf4[32] = {0};
    char buf5[32] = {0};
    struct msqid_ds msgbuf;

    if ((g.master == 1) && (g.num_children == 0)) {
        // parent before children spawn
        strcpy(buf, "Parent Window");
        strcpy(buf2, "Press 'c' to spawn child windows");
        if (g.num_children > 0) {
            sprintf(buf4, "# children: %d", g.num_children);
        } else {
            memset(buf4, 0, sizeof(buf4));
        }
        if (msgctl(g.mqid, IPC_STAT, &msgbuf) == 0){
            sprintf(buf5, "# pending msg: %lu", msgbuf.msg_qnum);
        } else {
            memset(buf5, 0, sizeof(buf5));
        }

    } else if (g.master == 1 && g.num_children > 0) {
        // parent after children spawn with controls
        strcpy(buf, "Parent Window");
        strcpy(buf2, "Left-click mouse on colors");
        if (g.num_children > 0) {
            sprintf(buf4, "# children: %d", g.num_children);
        } else {
            memset(buf4, 0, sizeof(buf4));
        }
        if (msgctl(g.mqid, IPC_STAT, &msgbuf) == 0){
            sprintf(buf5, "# pending msg: %lu", msgbuf.msg_qnum);
        } else {
            memset(buf5, 0, sizeof(buf5));
        }
    } else {
        // children
        strcpy(buf, "Child Window");
        strcpy(buf2, "You can't do much with me");
    }

    // draw background
    XSetForeground(g.dpy, g.gc, g.background_color); 
    XFillRectangle(g.dpy, g.win, g.gc, 0, 0, g.xres, g.yres);

    // draw boxes
    if (g.master == 1 && g.num_children > 0) {
        for (int i = 0; i < NUM_BOXES; i++) {
            XSetForeground(g.dpy, g.gc, boxes[i].color);
            XFillRectangle(g.dpy, g.win, g.gc, 
                    boxes[i].pos.x, boxes[i].pos.y,
                    boxes[i].dim.x, boxes[i].dim.y);
            
            if (i == (NUM_BOXES-1)) {
                XSetForeground(g.dpy, g.gc, g.text_color);
                x11_setFont(14);
                XDrawString(g.dpy, g.win, g.gc, 
                            boxes[i].pos.x + (0.4 * boxes[i].dim.x), 
                            boxes[i].pos.y + (0.6 * boxes[i].dim.y), 
                            buf3, strlen(buf3));
            }
        }

    }

    // draw text
    XSetForeground(g.dpy, g.gc, g.text_color);
    x11_setFont(14);
    if (strlen(buf) > 0)
        XDrawString(g.dpy, g.win, g.gc, 30, 40, buf, strlen(buf));
    if (strlen(buf2) > 0)
        XDrawString(g.dpy, g.win, g.gc, 60, 90, buf2, strlen(buf2));
    // #ifdef DEBUG
    if (strlen(buf4) > 0) {

        XDrawString(g.dpy, g.win, g.gc, 
                                (int)(g.xres*0.6), 
                                // (int)(g.yres*0.25), 
                                40,
                                buf4, strlen(buf4));
    }
    if (strlen(buf5) > 0) {
        XDrawString(g.dpy, g.win, g.gc, 
                                (int)(g.xres*0.6), 
                                70,
                                buf5, strlen(buf5));
    }
    // #endif
}

void physics(void)
{

    // no animation for now

}

void x11_setFont(unsigned int idx) {
    char *fonts[] = { "fixed","5x8","6x9","6x10","6x12","6x13","6x13bold",
        "7x13","7x13bold","7x14","8x13","8x13bold","8x16","9x15","9x15bold",
        "10x20","12x24" };
    Font f = XLoadFont(g.dpy, fonts[idx]);
    XSetFont(g.dpy, g.gc, f);
}

// unused as of now... probably will use to move windows from parent
void getWindowCoords(int *x, int *y) {
    Window root = DefaultRootWindow(g.dpy);
    Window child;

    XGetWindowAttributes(g.dpy, root, &g.xwa);
    XTranslateCoordinates(g.dpy, g.win, root, g.xwa.x, g.xwa.y, x, y, &child);

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

            #ifdef DEBUG
            printf("You clicked on box %d... sending colors as a msg\n", i);
            fflush(stdout);
            #endif

            return m;
        }
    }
    return m;
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

void checkMSG(void) {


    #ifdef DEBUG
    printf("checkMSG started\n");
    fflush(stdout);
    #endif


    while (g.thread_active == 1) {
        // color change message
        if (msgrcv(g.mqid, &mymsg, sizeof(mymsg), 
                    (long)PTOC, 0) > 0) {// on success

            #ifdef DEBUG
            printf("Got a message\n");
            fflush(stdout);
            #endif

            switch (mymsg.m.t) {
                case COLOR_SIG:
                    g.background_color = mymsg.m.box_color;
                    g.text_color = mymsg.m.text_color;
                    break;
                case KILL_SIG:
                    g.thread_active = 0;
                    break;
                default:
                    break;
            }

        }

        //usleep(4000);

    }

    #ifdef DEBUG
    printf("checkMSG terminating\n");
    fflush(stdout);
    #endif

} 

void parentCheckMSG() {
    
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
                printf("Signal Sent to remove child\n");
                fflush(stdout);
                #endif
                break;
            default:
                break;
        }

    }
}
