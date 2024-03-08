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
#define NUM_BOXES 5
#define MAX_CHILDREN 20

enum {COLOR_SIG=10, KILL_SIG=20};

typedef struct {
    int x;
    int y;
} Vec2;

typedef struct {
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
void start_thread(void);
void checkMSG(void);

MsgData checkBoxClick(int mx, int my);


int myargc;
char **myargv;
char **myenvp;

int main(int argc, char *argv[], char *envp[])
{
    g.envp = envp;
    g.mytimer = 0;
    strcpy(g.fname, argv[0]);


    // PARSE COMMAND LINE ARGS
    if (argc == 3) {
        g.mytimer = atoi(argv[1]);
        g.mqid = atoi(argv[2]);
        if ( g.mytimer < 0 ) {
            g.mytimer = 0; 
            printf("Usage: %s [time duration (pos)] [mqid]\n", argv[0]);
        }
        g.master = 0;

        // make thread to look for messages
    } else if (argc == 2) {
        g.mytimer = atoi(argv[1]);
        g.master = 1;
        // will create mqid
        if ( g.mytimer < 0 ) {
            g.mytimer = 0; 
            printf("Usage: %s [time duration (pos)] [mqid]\n", argv[0]);
        }

        // setup mq

    } else if (argc == 1) {
        g.mytimer = 0;
        g.master = 1;
        // will create mqid and no timer
        printf("Usage: %s [time duration (pos)] [mqid]\n", argv[0]);

    } else {
        printf("Usage: %s [time duration (pos)] [mqid]\n", argv[0]);
    }


	XEvent e;
	int kdone = 0, mdone = 0;
	x11_init_xwindows();
    init_globals();
    pthread_t tid;
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

    srand(time(NULL));
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
    int colors[5] = { 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00ffff00, 0x00000000};
    int text_colors[5] = { 0x00ffffff, 0x00000000, 0x00ffffff, 0x00000000, 0x00ffffff};
    const int WIDTH = 50, HEIGHT = 40, XSTART = 22, 
                XPAD = (int)((g.xres - (XSTART) - (5*WIDTH))/NUM_BOXES), 
                YPAD = 40,
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

    if (g.master == 1) {
        g.background_color = 0x00FFC72C;
        g.text_color = 0x00000000;
    } else {
        g.background_color = 0x00003594;
        g.text_color = 0x00ffffff;
    }

    // int c = (g.background_color == gold) ? 0x00ffffff : 0x00000000;

}

int check_mouse(XEvent *e)
{
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
            MsgData m = checkBoxClick(mx, my);
            if (m.box_num == (NUM_BOXES -1)) {
                
                // kill children boxes
                mymsg.type = (long)KILL_SIG;
                mymsg.m = m;
                for (int i = 0; i < g.num_children; i++) {
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
                }
                memset(g.cpid_buf, 0, sizeof(g.cpid_buf));  // clear cpid list
                g.num_children = 0;

                return 0;
            } else if (m.box_num >= 0 && m.box_num < (NUM_BOXES-1)) {
                // send msg with box color
                mymsg.type = (long)COLOR_SIG;
                mymsg.m = m;

                for (int i = 0; i < g.num_children; i++) {

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
            if (g.master == 1) {
                int n = sum%3;
                switch (n)
                {
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
                switch (n)
                {
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

int check_keys(XEvent *e)
{
    //static int color = 1;
	int key;

	if (e->type != KeyPress && e->type != KeyRelease)
		return 0;
	key = XLookupKeysym(&e->xkey, 0);
	if (e->type == KeyPress) {
		switch (key) {
			case XK_1:          //              rrggbb
                break;
			case XK_Escape:
				return 1;

            case XK_c:
                if (g.master == 1 && g.num_children < MAX_CHILDREN) {
                    int cpid = fork();
                    if (cpid == 0) {
                        // main(myargc, myargv, myenvp);
                        // ./a.out 
                        char timer[10];
                        char mqid[10];

                        sprintf(timer, "%d", g.mytimer); // make str of timer and cpid
                        sprintf(mqid, "%d", g.mqid);

                        char *argv[] = {g.fname, timer, mqid, NULL};
                        //char *envp[2] = {"DISPLAY=:0.0", NULL};
                        execve(g.fname, argv, g.envp);

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

void render(void)
{
    // int blue = 0x00FFC72C;
    // int gold = 0x00003594;
    char buf[128];
    char buf2[128];
    //char buf4[128];
    //char buf5[128];
    
    if ((g.master == 1) && (g.num_children == 0)) {
        // parent before children spawn

            strcpy(buf, "Parent Window");
            strcpy(buf2, "Press 'c' to spawn child windows");


    } else if (g.master == 1 && g.num_children > 0) {
        // parent after children spawn with controls
            strcpy(buf, "Parent Window");
            strcpy(buf2, "Left-click mouse on colors");

        
    } else {
        // children
            strcpy(buf, "Child Window");
            strcpy(buf2, "You can't do much with me");

    }

    // draw background
    XSetForeground(g.dpy, g.gc, g.background_color); 
    XFillRectangle(g.dpy, g.win, g.gc, 0, 0, g.xres, g.yres);
    
    // draw boxes
    if (g.master == 1) {
        for (int i = 0; i < NUM_BOXES; i++) {
            XSetForeground(g.dpy, g.gc, boxes[i].color);
            XFillRectangle(g.dpy, g.win, g.gc, 
                            boxes[i].pos.x, boxes[i].pos.y,
                            boxes[i].dim.x, boxes[i].dim.y);
        }
        
    }

    // draw text
    // int c = (g.background_color == gold) ? 0x00ffffff : 0x00000000;
    XSetForeground(g.dpy, g.gc, g.text_color);
    x11_setFont(14);
    if (strlen(buf) > 0)
        XDrawString(g.dpy, g.win, g.gc, 120, 40, buf, strlen(buf));
    if (strlen(buf2) > 0)
        XDrawString(g.dpy, g.win, g.gc, 40, 70, buf2, strlen(buf2));

}

void physics(void)
{



    
}




void x11_setFont(unsigned int idx)
{
    char *fonts[] = { "fixed","5x8","6x9","6x10","6x12","6x13","6x13bold",
    "7x13","7x13bold","7x14","8x13","8x13bold","8x16","9x15","9x15bold",
    "10x20","12x24" };
    Font f = XLoadFont(g.dpy, fonts[idx]);
    XSetFont(g.dpy, g.gc, f);
}


void getWindowCoords(int *x, int *y)
{
    Window root = DefaultRootWindow(g.dpy);
    Window child;
    
    XGetWindowAttributes(g.dpy, root, &g.xwa);
    XTranslateCoordinates(g.dpy, g.win, root, g.xwa.x, g.xwa.y, x, y, &child);

}

MsgData checkBoxClick(int mx, int my)
{
    MsgData m = {.box_color = -1, 
                    .text_color = -1,
                    .box_num = -1};
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

void setupMQ(void)
{
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

void teardownMQ(void)
{
#ifdef DEBUG
    printf("Tearing Down MQ\n");
    fflush(stdout);
#endif
    msgctl(g.mqid, IPC_RMID, 0);
}

void start_thread(void)
{

}

void checkMSG(void)
{


#ifdef DEBUG
    printf("checkMSG started\n");
    fflush(stdout);
#endif


    while (g.thread_active == 1) {
            // color change message
        if (msgrcv(g.mqid, &mymsg, sizeof(mymsg), 
                    (long)COLOR_SIG, IPC_NOWAIT) > 0) {// on success


#ifdef DEBUG
            printf("Got a message\n");
            fflush(stdout);
#endif

            g.background_color = mymsg.m.box_color;
            g.text_color = mymsg.m.text_color;
            
            // return; // leave without touching other msgs
        }
            // quit message
        if (msgrcv(g.mqid, &mymsg, sizeof(mymsg), 
                        (long)KILL_SIG, IPC_NOWAIT) > 0) {
            g.thread_active = 0;
        }

        usleep(4000);

    }

#ifdef DEBUG
            printf("checkMSG terminating\n");
            fflush(stdout);
#endif

} 
