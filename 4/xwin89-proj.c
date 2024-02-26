/*
 *
    Name: Michael Kausch
    Assignment: Class assignment - fork window call main
    date: 2/16/24
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
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h> // for threads


//#define DEBUG
//#define USE_MSQ
//#define USE_SEM
#define USE_SHM

enum CollisionType { LEFT=1, RIGHT=2, TOP=3, BOTTOM=4};
enum SharedStatus { NONE=1, CHILD=2, PARENT=3, BOTH=4 };

struct Global {
	Display *dpy;
	Window win;
	GC gc;
    XWindowAttributes xwa;
	int xres, yres;
    int mx, my;
    int cur_color;
    //int bx;
    //int by;
    //int bwidth;
    //int bheight;
    int winner;
    //int xdirection, ydirection;
    int cpid;
    int hasSpawned;
    int mqid, semid, shmid;
    //enum SharedStatus *hasBall; // shared between windows
    struct sembuf grab[2], release[1];
    struct Shared * shr;
    int runThread;
    pthread_t tid;
} g;

struct Vec2 {
    int x;
    int y;
};

/*

struct WinInfo {
    struct Vec2 dir;// velocity
    struct Vec2 boxScreen; // screen coords of box
    enum CollisionType ct; // left/right or up/down collision
};

struct WinMsg {
    long type;
    struct WinInfo w;
} mymsg;

*/
struct Shared {
    struct Vec2 dir;
    struct Vec2 pos;
    struct Vec2 dim;
    struct Vec2 newPos;
    //enum CollisionType ct;
    enum SharedStatus hasBall;
    struct Vec2 pWinPos, cWinPos;

    /*
	Display *parent_dpy, *child_dpy;
	Window parent_win, child_win;
	GC parent_gc, child_gc;
    
    */
};

union {
	int val;
	struct semid_ds *buf;
	unsigned short *array;
	struct seminfo *__buf;
} my_semun;

// need shared mem with semaphore for jump ball

void x11_cleanup_xwindows(void);
void x11_init_xwindows(void);
void x11_clear_window(void);
void check_mouse(XEvent *e);
int check_keys(XEvent *e);
void render(void);
void physics(void);
int check_winner(void);
void x11_setFont(unsigned int idx);
void* getWindowCoords(void *);
void setupMQ(void);
void teardownMQ(void);
void setupSEM(void);
void teardownSEM(void);
void setupSHM(void);
void teardownSHM(void);
void checkMSG(void);
struct Vec2 translateCoords(struct Vec2 c);
int checkLRCollision(void); 
int checkUDCollision(void); 
int checkExternalCollision(enum CollisionType ct);

int myargc;
char **myargv;
char **myenvp;

int main(int argc, char *argv[], char *envp[])
{
    
    myargc = argc;
    myargv = argv;
    myenvp = envp;

    int mytimer = 0;
    if (argc == 2) {
        mytimer = atoi(argv[1]);
        if ( mytimer > 0 ) {
            mytimer = mytimer; 
        }
    }

	XEvent e;
	int done = 0;
	x11_init_xwindows();

#ifdef USE_SEM
    setupSEM();
#endif

#ifdef USE_MSQ
    setupMQ();
#endif

#ifdef USE_SHM
    setupSHM();
#endif

	while (!done) {
		/* Check the event queue */
		while (XPending(g.dpy)) {
			XNextEvent(g.dpy, &e);
			check_mouse(&e);
			done = check_keys(&e);
		}
        render();
        physics();
#ifdef USE_MSQ
        checkMSG();
#endif
        if (mytimer > 0) {
            if (--mytimer == 0)
                done = 1;
        }
		usleep(4000);
    }

    void* t_status;
    pthread_join(g.tid, &t_status);
	x11_cleanup_xwindows();

#ifdef USE_MSQ
    teardownMQ();
#endif

#ifdef USE_SEM
    teardownSEM();
#endif

#ifdef USE_SHM
    teardownSHM();
    int p_status;
    wait(&p_status);
    shmctl(g.shmid, IPC_RMID, 0);
#endif

	return 0;
}

/*
struct Vec2 translateCoords(struct Vec2 c)
{
    struct Vec2 out_coords;
    int x,y;
    //getWindowCoords(g.dpy, &g.win, &x,&y);

    
    //out_coords.x = c.x-x;
    //out_coords.y = c.y-y;
    out_coords.x = x-c.x;
    out_coords.y = y-c.y;
    return out_coords;
}
*/

#ifdef USE_MSQ
void checkMSG(void)
{
    
    if (g.hasSpawned == 0 || g.cpid > 0) {
        //printf("checking for msg in parent\t");
        // child
        if (msgrcv(g.mqid, &mymsg, sizeof(mymsg), 
                    (long)PARENT, IPC_NOWAIT) > 0) {// on success
            printf("Got a message in the parent\n");
            printf("OLD box.x: %d box.y: %d dir.x: %d dir.y: %d\n",
                        g.bx, g.by, g.xdirection, g.ydirection);
            g.xdirection = mymsg.w.dir.x;
            g.ydirection = mymsg.w.dir.y;
            //struct Vec2 BoxScreen = translateCoords(mymsg.w.boxScreen);
            //g.bx = BoxScreen.x;
            //g.by = BoxScreen.y;
            g.bx = mymsg.w.boxScreen.x;
            g.by = mymsg.w.boxScreen.y;
            printf("NEW box.x: %d box.y: %d dir.x: %d dir.y: %d\n",
                        g.bx, g.by, g.xdirection, g.ydirection);
            //printf("press enter to continue...\n");
            //getchar();
            return; // leave without touching other queues
        }

    } else {
        // parent
        //printf("checking for msg in the child\n");
        if (msgrcv(g.mqid, &mymsg, sizeof(mymsg), 
                    (long)CHILD, IPC_NOWAIT) > 0) {// on success
            printf("Got a message in the child\n");
            printf("OLD box.x: %d box.y: %d dir.x: %d dir.y: %d\n",
                        g.bx, g.by, g.xdirection, g.ydirection);
            g.xdirection = mymsg.w.dir.x;
            g.ydirection = mymsg.w.dir.y;
            //struct Vec2 BoxScreen = translateCoords(mymsg.w.boxScreen);
            //g.bx = BoxScreen.x;
            //g.by = BoxScreen.y;
            g.bx = mymsg.w.boxScreen.x;
            g.by = mymsg.w.boxScreen.y;
            printf("NEW box.x: %d box.y: %d dir.x: %d dir.y: %d\n",
                        g.bx, g.by, g.xdirection, g.ydirection);
            //printf("press enter to continue...\n");
            //getchar();
            return;
        }

    }

    if (msgrcv(g.mqid, &mymsg, sizeof(mymsg), 
                    (long)BOTH, IPC_NOWAIT) > 0) {// on success
            printf("Got a message intended for both\n");
            g.xdirection = mymsg.w.dir.x;
            g.ydirection = mymsg.w.dir.y;
            struct Vec2 BoxScreen = translateCoords(mymsg.w.boxScreen);
            g.bx = BoxScreen.x;
            g.by = BoxScreen.y;
            return;
    }

    if (msgrcv(g.mqid, &mymsg, sizeof(mymsg),
                (long)NONE, IPC_NOWAIT) > 0) {
        printf("Got a message intended for none\n");

    }

}
#endif

void x11_cleanup_xwindows(void)
{
	XDestroyWindow(g.dpy, g.win);
	XCloseDisplay(g.dpy);
}

void setupMQ(void)
{
    printf("Setting up MQ\n");
    char pathname[128];
    getcwd(pathname, 128);
    strcat(pathname, "/foo");   /* make sure file foo is in your directory*/
    //int g.mqid;
    key_t ipckey = ftok(pathname, 69);

    // check for failure when generating ipc key
    g.mqid = msgget(ipckey, IPC_CREAT | 0666);
    if (g.mqid < 0) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }
}

void teardownMQ(void)
{
    printf("Tearing Down MQ\n");
    msgctl(g.mqid, IPC_RMID, 0);
}

void setupSEM(void)
{
    printf("Setting up SEM\n");
    char pathname[128];
    getcwd(pathname, 128);
    strcat(pathname, "/foo");   /* make sure file foo is in your directory*/
    //int g.mqid;
    key_t ipckey = ftok(pathname, 69);
    //g.semid = semget(ipckey, 1, 0666 | IPC_CREAT);
    
    if ((g.semid = semget(ipckey, 1, 0666 | IPC_CREAT)) == -1) {
        printf("Error - %s\n", strerror(errno));
        _exit(1);
    }
    
    g.grab[0].sem_num = 0;
    g.grab[1].sem_num = 0;
    g.grab[0].sem_flg = SEM_UNDO;
    g.grab[1].sem_flg = SEM_UNDO;
    g.grab[0].sem_op = 0;
    g.grab[1].sem_op = 1;
    g.release[0].sem_num = 0;
    g.release[0].sem_flg = SEM_UNDO;
    g.release[0].sem_op = -1;
    my_semun.val = 0;
    semctl(g.semid, 0, SETVAL, my_semun);
}
void teardownSEM(void)
{
    printf("Tearing Down SEM\n");
    semctl(g.semid, 0, IPC_RMID);
}

void setupSHM(void)
{
    printf("Setting up SHM\n");
    char pathname[128];
    getcwd(pathname, 128);
    strcat(pathname, "/foo");   /* make sure file foo is in your directory*/
    //int g.mqid;
    key_t ipckey = ftok(pathname, 69);
    //g.semid = semget(ipckey, 1, 0666 | IPC_CREAT);
    
    if ((g.shmid = shmget(ipckey, sizeof(struct Shared), IPC_CREAT | 0666)) == -1){
        printf("Error - %s\n", strerror(errno));
        _exit(1);
    }
    
    g.shr = (struct Shared *)shmat(g.shmid, (void*)0,0);
    g.shr->dir.x = (rand() % 2 == 0) ? 1 : -1;
    g.shr->dir.y = (rand() % 2 == 0) ? 1 : -1;
    g.shr->dim.x = 80;
    g.shr->dim.y = 80;
    g.shr->pos.x = rand() % (g.xres-g.shr->dim.x);
    g.shr->pos.y = rand() % (g.yres-g.shr->dim.y);
    //g.

    
    if (g.hasSpawned == 0 || g.cpid > 0) {
        printf("creating thread for parent\n");

    } else {
        printf("creating thread for child\n");
    }
    
    g.runThread = 1;
    pthread_create(&g.tid, NULL, (void*)getWindowCoords, NULL);

}

void teardownSHM(void)
{
    printf("Tearing Down SHM\n");
    shmdt(g.shr); 

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
	g.yres = 250;
	g.win = XCreateSimpleWindow(g.dpy, RootWindow(g.dpy, scr), 1, 1,
							g.xres, g.yres, 0, 0x00FFFFFF, 0x00000000);

	XStoreName(g.dpy, g.win, "mkausch project");
	g.gc = XCreateGC(g.dpy, g.win, 0, NULL);
    //g.hasSpawned = 0;
    //g.bx = rand() % (g.xres-g.bwidth);
    //g.by = rand() % (g.yres-g.bheight);
    g.cur_color = 0x00FFC72C;
    g.winner = 0;
    //g.xdirection = (rand() % 2 == 0) ? 1 : -1;
    //g.ydirection = (rand() % 2 == 0) ? 1 : -1;
    //*g.hasBall = NONE;
	XMapWindow(g.dpy, g.win);
	XSelectInput(g.dpy, g.win, ExposureMask | StructureNotifyMask |
								PointerMotionMask | ButtonPressMask |
								ButtonReleaseMask | KeyPressMask);
    
}

void check_mouse(XEvent *e)
{
	static int savex = 0;
	static int savey = 0;
    static int sum = 0;

	int mx = e->xbutton.x;
	int my = e->xbutton.y;

	if (e->type != ButtonPress
		&& e->type != ButtonRelease
		&& e->type != MotionNotify)
		return;
	if (e->type == ButtonPress) {
		if (e->xbutton.button==1) { }
		if (e->xbutton.button==3) { }
	}
	if (e->type == MotionNotify) {
		if (savex != mx || savey != my) {
			/*mouse moved*/
			savex = mx;
			savey = my;
            g.mx = mx;
            g.my = my;
            sum += 1;
            if (g.hasSpawned == 0 || g.cpid > 0) {
                if (sum % 10 == 0) {
                    printf("m");
                    fflush(stdout);
                }

            } else {
                if (sum % 10 == 0) {
                    printf("c");
                    fflush(stdout);
                }

            }
		}
	}
}

int check_keys(XEvent *e)
{
    //static int color = 1;
    //static int jumpBall = 1;
	int key;


	if (e->type != KeyPress && e->type != KeyRelease)
		return 0;
	key = XLookupKeysym(&e->xkey, 0);
	if (e->type == KeyPress) {
		switch (key) {
			case XK_1:          //              rrggbb
#ifdef DEBUG 
                printf("hit the 1 key, g.hasSpawned: %d", g.hasSpawned);
                fflush(stdout);
#endif
                if (g.hasSpawned == 0)
                    break;
               
                if (g.cpid == 0) {
#ifdef DEBUG
                    printf("sending ball from child\n");
                    fflush(stdout);
#endif
                    g.shr->hasBall = (g.shr->hasBall == CHILD ? PARENT : CHILD);

                } else {
#ifdef DEBUG
                    printf("sending ball from parent\n");
                    fflush(stdout);
#endif
                    g.shr->hasBall = (g.shr->hasBall == PARENT ? CHILD : PARENT);
                }

#ifdef USE_MSQ
                // send ball via msg
                mymsg.type = (long)(g.shr->hasBall); 
#ifdef DEBUG
                
                if (g.cpid == 0) {
                    printf("child sending msg with type %ld\n", mymsg.type);
                } else {
                    printf("parent sending msg with type %ld\n", mymsg.type);
                }
                fflush(stdout);
#endif

                // set vel of box
                mymsg.w.dir.x = g.xdirection;
                mymsg.w.dir.y = g.ydirection;

                // set x,y of box
                struct Vec2 b;
                getWindowCoords(&(b.x), &(b.y));
                b.x = g.bx; 
                b.y = g.by;
                mymsg.w.boxScreen = b;

                // send msg
                int ret =  msgsnd(g.mqid, &mymsg, sizeof(mymsg), 0);
                if (ret == 0) {
                    printf("msgsnd was successful\n");
                    fflush(stdout);
                }
#endif

                break;
            case XK_2:
                if (g.hasSpawned == 0)
                    break;
                
                if (g.cpid == 0) {
                    // child
                    g.shr->hasBall = CHILD;
                } else {
                    // parent
                    g.shr->hasBall = PARENT;
                }
                g.shr->pos.x = g.xres/2;
                g.shr->pos.y = g.yres/2;
                g.winner = 0;

                break;
			case XK_Escape:
                g.runThread = 0;
				return 1;

            case XK_c:
                if (g.hasSpawned == 0) {
                    g.hasSpawned = 1;
                    g.cpid = fork();
                    if (g.cpid == 0) {
                        g.winner = 0;
                        main(myargc, myargv, myenvp);
                        exit(0);
                    }   
                }
                break;
		}
	}
	return 0;
}

void render(void)
{

    char buf[128];
    char buf2[128];
    char buf3[10];
    char buf4[128];
    char buf5[128];
    char buf6[128];
    int color1 = 0x00ffc72c;
    int color2 = 0x00003594;
    //int wx =0, wy=0;
    
    if (g.hasSpawned == 0 || g.cpid > 0) {
        // parent or hasn't spawned a child

            //strcpy(buf, "Parent Window");
            sprintf(buf, "Parent Window.. cpid: %d g.hasSpawned: %d", g.cpid, g.hasSpawned);
            strcpy(buf2, "Press 'c' for a child window");
            strcpy(buf3, "Parent");
            g.cur_color = color1;
            //sprintf(buf2, "mouse[x]: %d | mouse[y]: %d", g.mx, g.my);
            sprintf(buf4, "window[x]: %d | window[y]: %d", 
                                g.shr->pWinPos.x, g.shr->pWinPos.y);
            sprintf(buf5, "hasBall: %d", (g.shr->hasBall == CHILD ? 1 : 0));

    } else if (g.cpid == 0) {
        // child
            //strcpy(buf, "Child Window");
            sprintf(buf, "Child Window.. cpid: %d g.hasSpawned: %d", g.cpid, g.hasSpawned);
            strcpy(buf2, "Press 'esc' to close");
            strcpy(buf3, "Child");
            g.cur_color = color2;
            sprintf(buf4, "window[x]: %d | window[y]: %d", 
                                g.shr->cWinPos.x, g.shr->cWinPos.y);
            sprintf(buf5, "hasBall: %d", (g.shr->hasBall == PARENT ? 1 : 0));

    }

    XSetForeground(g.dpy, g.gc, g.cur_color); 
    XFillRectangle(g.dpy, g.win, g.gc, 0, 0, g.xres, g.yres);

    //char buf[] = "Press 1 to change the background Color!";

    XSetForeground(g.dpy, g.gc, g.cur_color);
    XFillRectangle(g.dpy, g.win, g.gc, 0, 0, g.xres, g.yres);
    
    int c = ((g.cur_color == color1) ? color2 : color1);
    XSetForeground(g.dpy, g.gc, c);

    //x11_setFont(14);
    //void x11_setFont(unsigned int idx)
    // draw prompts and screen info
    XDrawString(g.dpy, g.win, g.gc, 80, 40, buf, strlen(buf));
    XDrawString(g.dpy, g.win, g.gc, 80, 80, buf2, strlen(buf2));
    XDrawString(g.dpy, g.win, g.gc, 80, 120, buf4, strlen(buf4));
    XDrawString(g.dpy, g.win, g.gc, 80, 160, buf5, strlen(buf5));
    
    if (((g.cpid == 0) && (g.shr->hasBall == CHILD))
            || ((g.cpid != 0) && (g.shr->hasBall == PARENT))) {
        // draw box
        XSetForeground(g.dpy, g.gc, c);
        XFillRectangle(g.dpy, g.win, g.gc, g.shr->pos.x, g.shr->pos.y, g.shr->dim.x, g.shr->dim.y);

        // draw box tex
        XSetForeground(g.dpy, g.gc, g.cur_color);
        XDrawString(g.dpy, g.win, g.gc, (g.shr->pos.x + (g.shr->dim.x/2) - 24), (g.shr->pos.y + (g.shr->dim.y/2) + 5), buf3, strlen(buf3));

        XSetForeground(g.dpy, g.gc, c);
        sprintf(buf6, "ball[x]: %d ball[y]: %d\n", g.shr->pos.x, g.shr->pos.y);
        XDrawString(g.dpy, g.win, g.gc, 80, 200, buf6, strlen(buf6));
        
    }

}

void physics(void)
{
    //if (g.hasBall) {

    if (((g.hasSpawned == 0 || g.cpid > 0) && (g.shr->hasBall == PARENT))
    //if (((g.cpid == 0) && (g.shr->hasBall == CHILD))
            || ((g.cpid == 0) && (g.shr->hasBall == CHILD))) {

        if (g.winner) {
            //g.winner = xtouch && ytouch;
            return;
        }

        // move ball

        // critical section 

        int xtouch = checkLRCollision();
        int ytouch = checkUDCollision();
        g.winner = xtouch && ytouch;

        if (xtouch && !g.winner) {
            // check if can go to other box
            int b = checkExternalCollision((enum CollisionType)xtouch);
            if (b == 1) {
                printf("There was an external collision\n");
                fflush(stdout);
                g.shr->hasBall = (g.shr->hasBall == CHILD ? PARENT : CHILD);

            } else {
                g.shr->dir.x *= -1;
            }

        } else if (ytouch && !g.winner) {
            // check if can go to other box
            
            int b = checkExternalCollision((enum CollisionType)ytouch);
            if (b == 1) {
                printf("There was a collision with the top/bottom wall");
                fflush(stdout);
                g.shr->hasBall = (g.shr->hasBall == CHILD ? PARENT : CHILD);
            } else {
                g.shr->dir.y *= -1;

            }
            
            //b;

            // else 


        } /*   else {
            // some events that occur in the middle of the window
        } */

        g.shr->pos.x += 1 * g.shr->dir.x;
        g.shr->pos.y += 1 * g.shr->dir.y;

        // end critical section
    }
}

int checkLRCollision(void) {

    //int epsilon = 0;
    int ct;
    if (g.shr->pos.x > (g.xres-g.shr->dim.x)) {
        ct = (int)RIGHT;
    } else if (g.shr->pos.x < 0) {
        ct = (int)LEFT;
    } else {
        ct = 0;
    }

    //return (g.shr->pos.x > (g.xres-g.shr->dim.x) || g.shr->pos.x < 0);
    //return (g.xres - (g.shr->pos.x + g.shr->dim.x) <= epsilon) || ((g.shr->pos.x) <= epsilon);
    return ct;
}


int checkUDCollision(void) {
    
    //int epsilon = 0;
    //return (g.yres - (g.shr->pos.y + g.shr->dim.y) <= epsilon) || ((g.shr->pos.y) <= epsilon);
    int ct;

    if (g.shr->pos.y > (g.yres-g.shr->dim.y)) {
        ct = (int)BOTTOM;
    } else if (g.shr->pos.y < 0) {
        ct = (int)TOP;
    } else {
        ct = 0;
    }
    //return (g.shr->pos.y > (g.yres-g.shr->dim.y) || g.shr->pos.y < 0);
    return ct;
}



int check_winner(void)
{

    int epsilon = 0;
    int xtouch = (g.xres - (g.shr->pos.x + g.shr->dim.x) <= epsilon) || ((g.shr->pos.x) <= epsilon);
    int ytouch = (g.yres - (g.shr->pos.y + g.shr->dim.y) <= epsilon) || ((g.shr->pos.y) <= epsilon);

#ifdef DEBUG
    printf("(g.xres(%d) - (g.shr->pos.x(%d) + g.bwidth(%d)) (*%d*) < epsilon(%d) = %d               \n", 
            g.xres, g.shr->pos.x, g.bwidth, g.xres - (g.shr->pos.x + g.bwidth), epsilon, 
            (g.xres - (g.shr->pos.x + g.bwidth) < epsilon)); 

    printf("g.shr->pos.x(%d) - g.bwidth(%d) (*%d*) < epsilon(%d) = %d               \n", 
            g.shr->pos.x, g.bwidth, (g.shr->pos.x - g.bwidth), epsilon, 
            (g.shr->pos.x + g.bwidth) < epsilon); 

    printf("g.yres(%d) - (g.shr->pos.y(%d) + g.bheight(%d) (*%d*) < epsilon(%d) = %d               \n", 
            g.yres, g.shr->pos.y, g.bheight, g.yres - (g.shr->pos.y + g.bheight), epsilon, 
            (g.yres - (g.shr->pos.y + g.bheight) < epsilon)); 

    printf("g.shr->pos.y(%d) < epsilon(%d) = %d               \n", 
            g.shr->pos.y, epsilon, (g.shr->pos.y)  < epsilon); 

    //if (!g.winner)
        printf("xtouch: %d, ytouch %d             \n", xtouch, ytouch);
#endif

    return (xtouch && ytouch);

}


void x11_setFont(unsigned int idx)
{
    char *fonts[] = { "fixed","5x8","6x9","6x10","6x12","6x13","6x13bold",
    "7x13","7x13bold","7x14","8x13","8x13bold","8x16","9x15","9x15bold",
    "10x20","12x24" };
    Font f = XLoadFont(g.dpy, fonts[idx]);
    XSetFont(g.dpy, g.gc, f);
}


void * getWindowCoords(void* arg)
{
    
    Window root = DefaultRootWindow(g.dpy);
    Window child;
    XWindowAttributes xwa;
    
    XGetWindowAttributes(g.dpy, root, &xwa);
    
    if (g.hasSpawned == 0 || g.cpid > 0) {
        // parent
        printf("In getWindowCoords from parent\n");

        while(g.runThread == 1) {

            XTranslateCoordinates(g.dpy, g.win, root, xwa.x, xwa.y, 
                &(g.shr->pWinPos.x), &(g.shr->pWinPos.y), &child);
            usleep(1000);

        }
        


    } else {
        // parent

        printf("In getWindowCoords from child\n");
        while(g.runThread == 1) {

            XTranslateCoordinates(g.dpy, g.win, root, xwa.x, xwa.y, 
                &(g.shr->cWinPos.x), &(g.shr->cWinPos.y), &child);
            usleep(1000);
        }

    }

    return (void*)0;

}

int checkExternalCollision(enum CollisionType ct) {
    struct Vec2 pass_pos, rec_pos, boxScreen_pos;
    int slope;
    char buf[100];

    // assign vars depending on perspective
   
    if (g.hasSpawned == 0 || g.cpid > 0) {
        // parent
        sprintf(buf, "Checking collision in parent\n");
        rec_pos = g.shr->cWinPos;
        pass_pos = g.shr->pWinPos;
    } else {
        //child
        sprintf(buf, "Checking collision in child\n");
        pass_pos = g.shr->cWinPos;
        rec_pos = g.shr->pWinPos;
    }
    
    boxScreen_pos.x = pass_pos.x + g.shr->pos.x;
    boxScreen_pos.y = pass_pos.y + g.shr->pos.y;
    
    printf("%s", buf);
    printf("passer[x]: %d, passer[y]: %d\n", pass_pos.x, pass_pos.y);
    printf("receiver[x]: %d, receiver[y]: %d\n", rec_pos.x, rec_pos.y);
    //printf("Press return...\n");
    fflush(stdout);
    
    //getchar();
    int dist;
    int y;
    int xscreen, yscreen;
    int isCollision1 = 0, isCollision2 = 0;
    int begin, end;
    // change in y over change in x
    //

    if (ct == LEFT) {

        printf("**checking external collision from left wall**\n");
        if (pass_pos.x >= rec_pos.x) {

            /*
            if ((rec_pos.y <= boxScreen_pos.y) && (g.shr->dir.y > 0))
                return 0;
            if ((rec_pos.y >= boxScreen_pos.y) && (g.shr->dir.y < 0))
                return 0;
                */

            fflush(stdout);
            dist = boxScreen_pos.x - (rec_pos.x + g.xres);


            slope = (int)(g.shr->dir.y / abs(g.shr->dir.x));
            printf("\tdist: %d  | slope: %d\n", dist, slope);
            fflush(stdout);
            y = dist * slope;
            yscreen = boxScreen_pos.y + y;
            xscreen = rec_pos.x + g.xres - g.shr->dim.x - 1;
            if (xscreen > boxScreen_pos.x) return 0;
            
            printf("\ty: %d  | yscreen: %d\n", y, yscreen);
            fflush(stdout);
            begin = (yscreen >= rec_pos.y); 
            end = (yscreen <= (rec_pos.y + g.yres - g.shr->dim.y));

            isCollision1 = begin && end;
            printf("\tbegin: %d && end: %d = %d\n", begin, end, isCollision1);
            fflush(stdout);

            if (isCollision1 == 1) {
                printf("**\t\tCollision Found**\n");
                fflush(stdout);
                g.shr->pos.x = xscreen - rec_pos.x;
                g.shr->pos.y = yscreen - rec_pos.y;
            }

        }

    } else if (ct == RIGHT) {
        // return if other box is too far left
        //    printf("checking right external collision\n");

        printf("**checking external collision from right wall**\n");
        if (pass_pos.x <= rec_pos.x) {
            /*
            if ((rec_pos.y <= boxScreen_pos.y) && (g.shr->dir.y > 0))
                return 0;
            if ((rec_pos.y >= boxScreen_pos.y) && (g.shr->dir.y < 0))
                return 0;
                */

            fflush(stdout);
            dist = (rec_pos.x - boxScreen_pos.x - g.shr->dim.x);


            slope = (int)(g.shr->dir.y / abs(g.shr->dir.x));
            printf("\tdist: %d  | slope: %d\n", dist, slope);
            fflush(stdout);
            y = dist * slope;
            yscreen = boxScreen_pos.y + y;

            xscreen = rec_pos.x +1;
            if (xscreen < boxScreen_pos.x) return 0;

            printf("\ty: %d  | yscreen: %d\n", y, yscreen);
            fflush(stdout);
            begin = (yscreen >= rec_pos.y); 
            end = (yscreen <= (rec_pos.y + g.yres - g.shr->dim.y));

            isCollision1 = begin && end;
            printf("\tbegin: %d && end: %d = %d\n", begin, end, isCollision1);
            fflush(stdout);

            if (isCollision1 == 1) {
                printf("**\t\tCollision Found**\n");
                fflush(stdout);
                g.shr->pos.x = 1;
                g.shr->pos.y = yscreen - rec_pos.y;
            }

        }

    } else if (ct == TOP){

        printf("**checking external collision from Top wall**\n");
        if (pass_pos.y >= rec_pos.y) {
            /*
            if ((rec_pos.x <= boxScreen_pos.x) && (g.shr->dir.x > 0))
                return 0;
            if ((rec_pos.x >= boxScreen_pos.x) && (g.shr->dir.x < 0))
                return 0;
                */

            fflush(stdout);
            dist = boxScreen_pos.y - (rec_pos.y + g.yres);

            slope = (int)(g.shr->dir.x / abs(g.shr->dir.y)); //dx/dy for this one
            printf("\tdist: %d  | slope: %d\n", dist, slope);
            fflush(stdout);
            y = dist * slope;
            yscreen = boxScreen_pos.x + y;

            xscreen = rec_pos.y + g.yres - g.shr->dim.y;
            if (xscreen > boxScreen_pos.y) return 0;

            printf("\ty: %d  | yscreen: %d\n", y, yscreen);
            fflush(stdout);
            begin = (yscreen >= rec_pos.x); 
            end = (yscreen <= (rec_pos.x + g.xres - g.shr->dim.x));

            isCollision1 = begin && end;
            printf("\tbegin: %d && end: %d = %d\n", begin, end, isCollision1);
            fflush(stdout);

            if (isCollision1 == 1) {
                printf("**\t\tCollision Found going out the top**\n");
                fflush(stdout);
                g.shr->pos.x = yscreen - rec_pos.x;
                g.shr->pos.y = xscreen - rec_pos.y - 1;
            }

        }


    } else if (ct == BOTTOM){

        if (pass_pos.y <= rec_pos.y) {
            /*
            if ((rec_pos.x <= boxScreen_pos.x) && (g.shr->dir.x > 0))
                return 0;
            if ((rec_pos.x >= boxScreen_pos.x) && (g.shr->dir.x < 0))
                return 0;
                */

            fflush(stdout);
            dist = rec_pos.y - boxScreen_pos.y - g.shr->dim.y;
            slope = (int)(g.shr->dir.x / abs(g.shr->dir.y)); //dx/dy for this one
            printf("\tdist: %d  | slope: %d\n", dist, slope);
            fflush(stdout);
            y = dist * slope;
            yscreen = boxScreen_pos.x + y;

            xscreen = rec_pos.y;
            if (xscreen < boxScreen_pos.y) return 0;

            printf("\ty: %d  | yscreen: %d\n", y, yscreen);
            fflush(stdout);
            begin = (yscreen >= rec_pos.x); 
            end = (yscreen <= (rec_pos.x + g.xres - g.shr->dim.x));

            isCollision1 = begin && end;
            printf("\tbegin: %d && end: %d = %d\n", begin, end, isCollision1);
            fflush(stdout);

            if (isCollision1 == 1) {
                printf("**\t\tCollision Found going out the top**\n");
                fflush(stdout);
                g.shr->pos.x = yscreen - rec_pos.x;
                g.shr->pos.y = xscreen - rec_pos.y + 1; 
            }

        }

    } else {

        printf("checkExternCollision Param Error\n");

    }

    return isCollision1 || isCollision2;

}
