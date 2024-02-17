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
	int xres, yres;
    int mx, my;
    int cur_color;
    int bx;
    int by;
    int bwidth;
    int bheight;
    int winner;
    int xdirection, ydirection;
} g;

void x11_cleanup_xwindows(void);
void x11_init_xwindows(void);
void x11_clear_window(void);
void check_mouse(XEvent *e);
int check_keys(XEvent *e);
void render(void);
void physics(void);
int check_winner(void);

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

	XStoreName(g.dpy, g.win, "cs3600 xwin sample");
	g.gc = XCreateGC(g.dpy, g.win, 0, NULL);
    g.bwidth = 50;
    g.bheight = 50;
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
            if (sum % 10 == 0) {
                printf("m ");
                fflush(stdout);
            }
		}
	}
}

int check_keys(XEvent *e)
{
    static int color = 1;
	int key, cpid;


	if (e->type != KeyPress && e->type != KeyRelease)
		return 0;
	key = XLookupKeysym(&e->xkey, 0);
	if (e->type == KeyPress) {
		switch (key) {
			case XK_1:          //              rrggbb
                switch (color) {
                    case 0:
                        /*XSetForeground(g.dpy, g.gc, 0x008f751b);*/
                        XSetForeground(g.dpy, g.gc, 0x00FFC72C); 
                        XFillRectangle(g.dpy, g.win, g.gc, 0, 0, g.xres, g.yres);
                        color += 1;
                        g.cur_color = 0x00ffc72c;
                        break;
                    case 1:
                        XSetForeground(g.dpy, g.gc, 0x00003594); 
                        XFillRectangle(g.dpy, g.win, g.gc, 0, 0, g.xres, g.yres);
                        color -= 1;
                        g.cur_color = 0x00003594;
                        break;
                }
				break;
			case XK_Escape:
				return 1;

            case XK_c:
                cpid = fork();
                if (cpid == 0) {
                    main(1, myargv, myenvp);
                    g.winner = 0;
                    exit(0);
                }   
                break;
                    
		}
	}
	return 0;
}

void render(void)
{

        char buf2[128];
        sprintf(buf2, "mouse[x]: %d | mouse[y]: %d", g.mx, g.my);
        char buf3[] = "DVD";

    if (!g.winner) {
        
        char buf[] = "Press 1 to change the background Color!";

        XSetForeground(g.dpy, g.gc, g.cur_color);
        XFillRectangle(g.dpy, g.win, g.gc, 0, 0, g.xres, g.yres);

        XSetForeground(g.dpy, g.gc, 0x00ff0000);
        XDrawString(g.dpy, g.win, g.gc, 40, 40, buf, strlen(buf));
        XDrawString(g.dpy, g.win, g.gc, 80, 80, buf2, strlen(buf2));

        XSetForeground(g.dpy, g.gc, 0x00ffffff);
        XFillRectangle(g.dpy, g.win, g.gc, g.bx, g.by, g.bwidth, g.bheight);

        XSetForeground(g.dpy, g.gc, 0x00ff0000);
        XDrawString(g.dpy, g.win, g.gc, (g.bx + (g.bwidth/2) - 10), (g.by + (g.bheight/2) + 5), buf3, strlen(buf3));
    } else {

        char buf[] = "WINNER!";

        XSetForeground(g.dpy, g.gc, 0x00FFFFFF);
        XFillRectangle(g.dpy, g.win, g.gc, 0, 0, g.xres, g.yres);

        XSetForeground(g.dpy, g.gc, 0x00000000);
        XDrawString(g.dpy, g.win, g.gc, (g.xres/2), 40, buf, strlen(buf));
        XDrawString(g.dpy, g.win, g.gc, 80, 80, buf2, strlen(buf2));

        XSetForeground(g.dpy, g.gc, 0x00000000);
        XFillRectangle(g.dpy, g.win, g.gc, g.bx, g.by, g.bwidth, g.bheight);

        XSetForeground(g.dpy, g.gc, 0x00ffffff);
        XDrawString(g.dpy, g.win, g.gc, (g.bx + (g.bwidth/2) - 10), (g.by + (g.bheight/2) + 5), buf3, strlen(buf3));
    }

}

void physics(void)
{


    if (!g.winner)
        g.winner = check_winner();


    if (!g.winner) {
        g.bx += 2 * g.xdirection;
        g.by += 2 * g.ydirection;

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
