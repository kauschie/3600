/*
 *
 
    Name: Michael Kausch

 cs3600 Spring 2022
 original author:  Gordon Griesel
            date:  Jan 24, 2022
         purpose:  C program for students to practice their C programming
		           over the Winter break. Also, introduce students to the
                   X Window System. We will use the X Window protocol or
                   API to generate output in some of our lab and homework
                   assignments.

 Instructions:

      1. If you make changes to this file, put your name at the top of
	     the file. Use one C style multi-line comment to hold your full
		 name. Do not remove the original author's name from this or 
		 other source files please.

      2. Build and run this program by using the provided Makefile.

	     At the command-line enter make.
		 Run the program by entering ./a.out
		 Quit the program by pressing Esc.

         The compile line will look like this:
            gcc xwin89.c -Wall -Wextra -Werror -pedantic -ansi -lX11

		 To run this program on the Odin server, you will have to log in
		 using the -YC option. Example: ssh myname@odin.cs.csub.edu -YC

      3. See the assignment page associated with this program for more
	     instructions.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <unistd.h>

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
} g;

void x11_cleanup_xwindows(void);
void x11_init_xwindows(void);
void x11_clear_window(void);
void check_mouse(XEvent *e);
int check_keys(XEvent *e);
void render(void);
void physics(void);

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

	if (!(g.dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "ERROR: could not open display!\n");
		exit(EXIT_FAILURE);
	}
	scr = DefaultScreen(g.dpy);
	g.xres = 400;
	g.yres = 200;
	g.win = XCreateSimpleWindow(g.dpy, RootWindow(g.dpy, scr), 1, 1,
							g.xres, g.yres, 0, 0x00FFC72C, 0x00FFC72C);

	XStoreName(g.dpy, g.win, "cs3600 xwin sample");
	g.gc = XCreateGC(g.dpy, g.win, 0, NULL);
    g.bwidth = 50;
    g.bheight = 50;
    g.bx = 150;
    g.by = 150;
    g.cur_color = 0x00FFC72C;
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
                        /*
                    case 2:
                        XSetForeground(g.dpy, g.gc, 0x00ff0077); 
                        XFillRectangle(g.dpy, g.win, g.gc, 0, 0, g.xres, g.yres);
                        color = 0;
                        g.cur_color = 0x00ff0077;
                        break;
                        */
                }
				break;
			case XK_Escape:
				return 1;

            case XK_c:
                cpid = fork();
                if (cpid == 0) {
                    main(1, myargv, myenvp);
                    exit(0);
                }   
                break;
                    
		}
	}
	return 0;
}

void render(void)
{
    char buf[] = "Press 1 to change the background Color!";

    char buf2[128];

    sprintf(buf2, "mouse[x]: %d | mouse[y]: %d", g.mx, g.my);

    XSetForeground(g.dpy, g.gc, g.cur_color);
    XFillRectangle(g.dpy, g.win, g.gc, 0, 0, g.xres, g.yres);

    XSetForeground(g.dpy, g.gc, 0x00ff0000);
    XDrawString(g.dpy, g.win, g.gc, 40, 40, buf, strlen(buf));
    XDrawString(g.dpy, g.win, g.gc, 80, 80, buf2, strlen(buf2));

    XSetForeground(g.dpy, g.gc, 0x00ffffff);
    XFillRectangle(g.dpy, g.win, g.gc, g.bx, g.by, g.bwidth, g.bheight);

}

void physics(void)
{
    static int xdirection = 1;
    static int ydirection = 1;
    g.bx += 2 * xdirection;
    g.by += 2 * ydirection;

    if (g.bx > (g.xres-g.bwidth) || g.bx < 0) {
        xdirection *= -1;
    }
    if (g.by > (g.yres-g.bheight) || g.by < 0) {
        ydirection *= -1;
    }    

    
}
