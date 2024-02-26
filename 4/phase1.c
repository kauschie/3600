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


//#define DEBUG

struct Global {
	Display *dpy;
	Window win;
	GC gc;
    XWindowAttributes xwa;
	int xres, yres;
    int mx, my;
    int cur_color;
    int bx;
    int by;
    int bwidth;
    int bheight;
    int winner;
    int xdirection, ydirection;
    int cpid;
    int hasSpawned;
} g;

void x11_cleanup_xwindows(void);
void x11_init_xwindows(void);
void x11_clear_window(void);
void check_mouse(XEvent *e);
int check_keys(XEvent *e);
void render(void);
void physics(void);
int check_winner(void);
void x11_setFont(unsigned int idx);
void getWindowCoords(int *x, int *y);

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
    } else {
        printf("Usage: %s n\n", argv[0]);
    }

	XEvent e;
	int done = 0;
	x11_init_xwindows();

	while (!done) {
		/* Check the event queue */
		while (XPending(g.dpy)) {
			XNextEvent(g.dpy, &e);
			check_mouse(&e);
			done = check_keys(&e);
		}
        render();
        physics();
        if (mytimer > 0) {
            if (--mytimer == 0)
                done = 1;
        }
		usleep(4000);
    }
	x11_cleanup_xwindows();
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
	g.yres = 250;
	g.win = XCreateSimpleWindow(g.dpy, RootWindow(g.dpy, scr), 1, 1,
							g.xres, g.yres, 0, 0x00FFFFFF, 0x00000000);

	XStoreName(g.dpy, g.win, "mkausch project");
	g.gc = XCreateGC(g.dpy, g.win, 0, NULL);
    //g.hasSpawned = 0;
    g.bwidth = 80;
    g.bheight = 80;
    g.bx = rand() % (g.xres-g.bwidth);
    g.by = rand() % (g.yres-g.bheight);
    g.cur_color = 0x00FFC72C;
    g.winner = 0;
    g.xdirection = (rand() % 2 == 0) ? 1 : -1;
    g.ydirection = (rand() % 2 == 0) ? 1 : -1;
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
	int key;


	if (e->type != KeyPress && e->type != KeyRelease)
		return 0;
	key = XLookupKeysym(&e->xkey, 0);
	if (e->type == KeyPress) {
		switch (key) {
			case XK_1:          //              rrggbb
                g.winner = 0;
                break;
			case XK_Escape:
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
    //char buf4[128];
    //char buf5[128];
    int color1 = 0x00ffc72c;
    int color2 = 0x00003594;
    //int wx =0, wy=0;
    
    if (g.hasSpawned == 0 || g.cpid > 0) {
        // parent or hasn't spawned a child

            strcpy(buf, "Parent Window");
            strcpy(buf2, "Press 'c' for a child window");
            strcpy(buf3, "Parent");
            g.cur_color = color1;
            //sprintf(buf2, "mouse[x]: %d | mouse[y]: %d", g.mx, g.my);



    } else if (g.cpid == 0) {
        // child
            strcpy(buf, "Child Window");
            strcpy(buf2, "Press 'esc' to close");
            strcpy(buf3, "Child");
            g.cur_color = color2;

    }

    //getWindowCoords(&wx, &wy);
    //sprintf(buf4, "window[x]: %d | window[y]: %d", wx, wy);
    //sprintf(buf5, "window[width]: %d | window[height]: %d", g.xwa.width, g.xwa.height);
    
    XSetForeground(g.dpy, g.gc, g.cur_color); 
    XFillRectangle(g.dpy, g.win, g.gc, 0, 0, g.xres, g.yres);

    //char buf[] = "Press 1 to change the background Color!";

    XSetForeground(g.dpy, g.gc, g.cur_color);
    XFillRectangle(g.dpy, g.win, g.gc, 0, 0, g.xres, g.yres);
    
    int c = ((g.cur_color == color1) ? color2 : color1);
    XSetForeground(g.dpy, g.gc, c);

    x11_setFont(14);
    //void x11_setFont(unsigned int idx)
    // draw prompts and screen info
    XDrawString(g.dpy, g.win, g.gc, 80, 40, buf, strlen(buf));
    XDrawString(g.dpy, g.win, g.gc, 80, 80, buf2, strlen(buf2));
    //XDrawString(g.dpy, g.win, g.gc, 80, 120, buf4, strlen(buf4));
    //XDrawString(g.dpy, g.win, g.gc, 160, 160, buf5, strlen(buf5));

    // draw box
    XSetForeground(g.dpy, g.gc, c);
    XFillRectangle(g.dpy, g.win, g.gc, g.bx, g.by, g.bwidth, g.bheight);

    // draw box tex
    XSetForeground(g.dpy, g.gc, g.cur_color);
    XDrawString(g.dpy, g.win, g.gc, (g.bx + (g.bwidth/2) - 24), (g.by + (g.bheight/2) + 5), buf3, strlen(buf3));

}

void physics(void)
{


    if (!g.winner)
        g.winner = check_winner();


    if (!g.winner) {
        g.bx += 1 * g.xdirection;
        g.by += 1 * g.ydirection;

        if (g.bx > (g.xres-g.bwidth) || g.bx < 0) {
            g.xdirection *= -1;
        }
        if (g.by > (g.yres-g.bheight) || g.by < 0) {
            g.ydirection *= -1;
        }    

    }
    

    
}

int check_winner(void)
{

    int epsilon = 0;
    int xtouch = (g.xres - (g.bx + g.bwidth) <= epsilon) || ((g.bx) <= epsilon);
    int ytouch = (g.yres - (g.by + g.bheight) <= epsilon) || ((g.by) <= epsilon);

#ifdef DEBUG
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
#if DEBUG 
    printf("wx: %d, wy: %d\n", wx, wy);
#endif

}
