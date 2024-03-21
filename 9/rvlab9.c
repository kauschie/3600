// Modified by: Michael Kausch
// Mod Date: 3/21/24
// Assignment: lab9
//
// CMPS-3600 Fall 2023
// program: crash2.c
// author:  Gordon Griesel
// purpose: Framework for animated graphics
//          Using XWindows (Xlib) for drawing pixels
//          Close-with-click-on-x is implemented <-----
//          (credit: Taylor Hooser Spring 2023)
//          Double buffer used for smooth animation
//          No flickering
// 
// DBE means double buffer extension.
// It's an extersion of X11 that uses a video back-buffer.
// Draw to a back-buffer, then swap to the video memory when ready.
// 
// compile like this:
// 
//     $ gcc crash2.c -Wall -lX11 -lXext -lpthread
// 
// Press 'C' to see collisions of the cars in the intersection.
// Press 'S' to slow the cars in the intersection.
// The position of the collision is marked.
// Stop the collisions using POSIX or SysV semaphores.
//
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/Xdbe.h>
#include <X11/Xatom.h> /* for intercepting X click to close */

#ifndef NCARS
#define NCARS 8
#endif

void init();
void init_xwindows(int, int);
void cleanup_xwindows(void);
void check_resize(XEvent *e);
void check_mouse(XEvent *e);
int check_keys(XEvent *e);
void physics(void);
void render(void);
//
//---------------------------------
//globals to make your life easier.
struct Global {
	Display *dpy;
	Window win;
	GC gc;
	XdbeBackBuffer backBuffer;
	XdbeSwapInfo swapInfo;
	Atom wm_delete_window; /* credit: Taylor Hooser */
	int xres, yres;
	int collision_flag;
	int collision[NCARS];
	int crash[2];
	int show_collisions;
	int ncollisions;
	int slow_mode;
	pthread_mutex_t isOccupied;
	size_t numCross[4];
} g;

struct Box {
	double pos[2];
	double vel[2];
	int w, h;
} intersection, cars[NCARS];

int main(void)
{
	init();
	init_xwindows(460, 460);
	//
	pthread_t tid[NCARS];
	void *traffic(void *arg);
	int i;
	for (i=0; i<NCARS; i++)
		pthread_create(&tid[i], NULL, traffic, (void *)(long)i );
	int done = 0;
	pthread_mutex_init(&g.isOccupied, NULL);
	while (!done) {
		//Handle all events in queue...
		while(XPending(g.dpy)) {
			XEvent e;
			XNextEvent(g.dpy, &e);
			check_resize(&e);
			check_mouse(&e);
			done = check_keys(&e);
		}
		//Process physics and rendering every frame
		physics();
		render();
		XdbeSwapBuffers(g.dpy, &g.swapInfo, 1);
		usleep(4000);
	}
	cleanup_xwindows();
	return 0;
}

int fib(int n)
{
	if (n == 1 || n == 2)
		return 1;
	return fib(n-1) + fib(n-2);
}

int overlap(struct Box *c, struct Box *i)
{
	//Does one rectangle overlap another rectangle?
	if (c->pos[0] + (c->w >> 1) < i->pos[0] - (i->w >> 1)) return 0;
	if (c->pos[0] - (c->w >> 1) > i->pos[0] + (i->w >> 1)) return 0;
	if (c->pos[1] + (c->h >> 1) < i->pos[1] - (i->h >> 1)) return 0;
	if (c->pos[1] - (c->h >> 1) > i->pos[1] + (i->h >> 1)) return 0;
	return 1;
}

void *traffic(void *arg)
{
	//Each car will have its own thread. This thread.
	int carnum = (int)(long)arg;
	//This thread will run forever.
	//Calls to fib() are used to slow down.
	while (1) {
		fib(rand() % 5 + 2);
		//move the car...
		cars[carnum].pos[0] += cars[carnum].vel[0]; 
		cars[carnum].pos[1] += cars[carnum].vel[1];
		//Is car in the intersection???
		if (overlap(&cars[carnum], &intersection)) {
			// critical section
			//Car is in the intersection.
			pthread_mutex_lock(&g.isOccupied);
			g.numCross[(carnum%4)]++;
			while (overlap(&cars[carnum], &intersection)) {
				//Loop here until out of the intersection.
				fib(rand() % 5 + 2);
				if (g.slow_mode)
					fib(16);
				//move the car...
				cars[carnum].pos[0] += cars[carnum].vel[0]; 
				cars[carnum].pos[1] += cars[carnum].vel[1];
			}
			pthread_mutex_unlock(&g.isOccupied);
			// end critical section
		
		}
		//Check for this car outside of the window.
		//If outside, car will enter from other side of window.
		//left
		if (cars[carnum].pos[0] < -20 && cars[carnum].vel[0] < 0.0) {
			cars[carnum].pos[0] += g.xres + 40.0;
			cars[carnum].vel[0] = -(rand() % 3 + 1);
			cars[carnum].vel[0] *= 0.0002;
		}
		//top
		if (cars[carnum].pos[1] < -20 && cars[carnum].vel[1] < 0.0) {
			cars[carnum].pos[1] += g.yres + 40.0;
			cars[carnum].vel[1] = -(rand() % 3 + 1);
			cars[carnum].vel[1] *= 0.0002;
		}
		//right
		if (cars[carnum].pos[0] > g.xres + 20 && cars[carnum].vel[0] > 0.0) {
			cars[carnum].pos[0] -= (g.xres + 40.0);
			cars[carnum].vel[0] = (rand() % 3 + 1);
			cars[carnum].vel[0] *= 0.0002;
		}
		//bottom
		if (cars[carnum].pos[1] > g.yres + 20 && cars[carnum].vel[1] > 0.0) {
			cars[carnum].pos[1] -= (g.yres + 40.0);
			cars[carnum].vel[1] = (rand() % 3 + 1);
			cars[carnum].vel[1] *= 0.0002;
		}
	}
	return (void *)0;
}

void cleanup_xwindows(void)
{
	//Deallocate back buffer
	if(!XdbeDeallocateBackBufferName(g.dpy, g.backBuffer)) {
		fprintf(stderr,"Error : unable to deallocate back buffer.\n");
	}
	XFreeGC(g.dpy, g.gc);
	XDestroyWindow(g.dpy, g.win);
	XCloseDisplay(g.dpy);
}

void set_window_title()
{
	char ts[256];
	sprintf(ts, "3600 Intersection %ix%i", g.xres, g.yres);
	XStoreName(g.dpy, g.win, ts);
}

void init_xwindows(int w, int h)
{
	g.xres = w;
	g.yres = h;
	XSetWindowAttributes attributes;
	//int screen;
	int major, minor;
	XdbeBackBufferAttributes *backAttr;
	//XGCValues gcv;
	g.dpy = XOpenDisplay(NULL);
    //Use default screen
	//screen = DefaultScreen(dpy);
    //List of events we want to handle
	attributes.event_mask = ExposureMask | StructureNotifyMask |
							PointerMotionMask | ButtonPressMask |
							ButtonReleaseMask | KeyPressMask | KeyReleaseMask;
	//Various window attributes
	attributes.backing_store = Always;
	attributes.save_under = True;
	attributes.override_redirect = False;
	attributes.background_pixel = 0x00000000;
	//Get default root window
	Window root;
	root = DefaultRootWindow(g.dpy);
	//Create a window
	g.win = XCreateWindow(g.dpy, root, 0, 0, g.xres, g.yres, 0,
					    CopyFromParent, InputOutput, CopyFromParent,
					    CWBackingStore | CWOverrideRedirect | CWEventMask |
						CWSaveUnder | CWBackPixel, &attributes);
	//Create gc
	g.gc = XCreateGC(g.dpy, g.win, 0, NULL);
	//Get DBE version
	if (!XdbeQueryExtension(g.dpy, &major, &minor)) {
		fprintf(stderr, "Error : unable to fetch Xdbe Version.\n");
		XFreeGC(g.dpy, g.gc);
		XDestroyWindow(g.dpy, g.win);
		XCloseDisplay(g.dpy);
		exit(1);
	}
	printf("Xdbe version %d.%d\n", major, minor);
	//Get back buffer and attributes (used for swapping)
	g.backBuffer = XdbeAllocateBackBufferName(g.dpy, g.win, XdbeUndefined);
	backAttr = XdbeGetBackBufferAttributes(g.dpy, g.backBuffer);
    g.swapInfo.swap_window = backAttr->window;
    g.swapInfo.swap_action = XdbeUndefined;
	XFree(backAttr);
	//Map and raise window
	set_window_title();
	XMapWindow(g.dpy, g.win);
	XRaiseWindow(g.dpy, g.win);
	//
	//To intercept user clicking x in title bar.
    g.wm_delete_window = XInternAtom(g.dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g.dpy, g.win, &g.wm_delete_window, 1);
}

void fillRectangle(int x, int y, int w, int h)
{
	XFillRectangle(g.dpy, g.backBuffer, g.gc, x, y, w, h);
}

void drawRectangle(int x, int y, int w, int h)
{
	XDrawRectangle(g.dpy, g.backBuffer, g.gc, x, y, w, h);
}

void drawLine(int x0, int y0, int x1, int y1)
{
	XDrawLine(g.dpy, g.backBuffer, g.gc, x0, y0, x1, y1);
}

void drawString(int x, int y, char *str)
{
	XDrawString(g.dpy, g.backBuffer, g.gc, x, y, str, strlen(str));
}

void init(void)
{
	//Initialize cars direction, speed, etc.
	srand((unsigned)time(NULL));
	g.collision_flag = 0;
	g.show_collisions = 0;
	g.ncollisions = 0;
	g.slow_mode = 0;
	//the intersection
	intersection.w = 112;
	intersection.h = 112;
	intersection.pos[0] = g.xres / 2;
	intersection.pos[1] = g.yres / 2;
	intersection.vel[0] = 0;
	intersection.vel[1] = 0;
	//
	//cars...
	//
	//         1
	//         |
	//         v
	//       +-----+
	//       |     | <--2
	//  0--> |     |
	//       +-----+
	//           ^
	//           |
	//           3
	//
	int i;
	//initial size and position of each car
	for (i=0; i<NCARS; i++) {
		cars[i].w = 18;
		cars[i].h = 18;
		cars[i].pos[0] = intersection.pos[0];
		cars[i].pos[1] = intersection.pos[1];
		cars[i].vel[0] = 0;
		cars[i].vel[1] = 0;

		int offset = 21;
		// offset = 10;

		//Car heading West
		if (i%4 == 0) {
			cars[i].w += rand() % 4 + 14;
			cars[i].vel[0] = -(rand() % 3 + 1);
			cars[i].vel[1] = 0;
			if (i%4 == i) {
				cars[i].pos[0] = g.xres + 30;
				cars[i].pos[1] -= offset;
			} else {
				cars[i].pos[0] = g.xres + 30;
				cars[i].pos[1] -= 2*offset;
			}
		} else if (i%4 == 1) {
			//Car heading South
			// i = 1;
			cars[i].h += rand() % 4 + 14;
			cars[i].vel[0] = 0;
			cars[i].vel[1] = rand() % 3 + 1;
			if (i%4 == i) {
				cars[i].pos[0] -= offset;
				cars[i].pos[1] = -30;

			} else {
				cars[i].pos[0] -= 2*offset;
				cars[i].pos[1] = -30;
			}

		} else if (i%4 == 2) {
			//Car heading East
			// i = 2;
			cars[i].w += rand() % 4 + 14;
			cars[i].vel[0] = rand() % 3 + 1;
			cars[i].vel[1] = 0;

			if (i%4 == i) {
				cars[i].pos[0] = -40;
				cars[i].pos[1] += offset;
			} else {
				cars[i].pos[0] = -40;
				cars[i].pos[1] += 2* offset;
			}
		} else if (i%4 == 3) {
			//Car heading North
			// i = 3;
			cars[i].h += rand() % 4 + 14;
			cars[i].vel[0] = 0;
			cars[i].vel[1] = -(rand() % 3 + 1);

			if (i%4 == i) {
				cars[i].pos[0] += offset;
				cars[i].pos[1] = g.yres + 30;

			} else {
				cars[i].pos[0] += 2* offset;
				cars[i].pos[1] = g.yres + 30;
			}
		}



	}
	
	memset(g.numCross, 0, sizeof(size_t)*4);
	
	//
//	//Another car heading West
//	i = 4;
//	cars[i].w += rand() % 4 + 14;
//	cars[i].pos[0] = g.xres + 30;
//	cars[i].pos[1] -= (offset + 21);
//	cars[i].vel[0] = -(rand() % 3 + 1);
//	cars[i].vel[1] = 0;
	//Scale the velocity...
	for (i=0; i<NCARS; i++) {
		cars[i].vel[0] *= 0.0002;
		cars[i].vel[1] *= 0.0002;
	}
}

void check_resize(XEvent *e)
{
	//ConfigureNotify is sent when the window is resized.
	if (e->type != ConfigureNotify)
		return;
	XConfigureEvent xce = e->xconfigure;
	g.xres = xce.width;
	g.yres = xce.height;
	init();
	set_window_title();
}

void clear_screen(void)
{
	//XClearWindow(dpy, win);
	XSetForeground(g.dpy, g.gc, 0x00050505);
	XFillRectangle(g.dpy, g.backBuffer, g.gc, 0, 0, g.xres, g.yres);
}

void check_mouse(XEvent *e)
{
	static int savex = 0;
	static int savey = 0;

    if (e->type != ButtonPress && e->type != ButtonRelease &&
										e->type != MotionNotify) {
        return;
	}
	if (e->type == ButtonRelease)
		return;
	if (e->type == ButtonPress) {
		//Log("ButtonPress %i %i\n", e->xbutton.x, e->xbutton.y);
		if (e->xbutton.button==1) { }
		if (e->xbutton.button==3) { }
	}
	if (e->type == MotionNotify) {
		if (savex != e->xbutton.x || savey != e->xbutton.y) {
			//mouse moved
			savex = e->xbutton.x;
			savey = e->xbutton.y;
		}
	}
}

int check_keys(XEvent *e)
{
    //======================================================
    //To intercept user clicking X in title bar to close.
	//Not a keyboard event, but placed here for convenience.
	if (e->type == ClientMessage) {
		// if x button clicked, DO NOT CLOSE WINDOW
		// instead, overwrite default handler
	    if ((Atom)e->xclient.data.l[0] == g.wm_delete_window)
    	    return 1;
	}
    //======================================================
	//
	if (e->type != KeyPress && e->type != KeyRelease)
		return 0;
	int key = XLookupKeysym(&e->xkey, 0);
	if (e->type == KeyPress) {
		switch (key) {
			case XK_c:
				g.show_collisions ^= 1;
				break;
			case XK_s:
				g.slow_mode ^= 1;
				break;
			case XK_Escape:
				return 1;
		}
	}
	return 0;
}

void physics()
{
	//check for car collisions...
	g.collision_flag = 0;
	int i, j;
	for (i=0; i<NCARS; i++) {
		for (j=0; j<NCARS; j++) {
			if (i == j)
				continue;
			if (overlap(&cars[i], &cars[j])) {
				g.collision_flag = 1;
				g.collision[0] = cars[i].pos[0];
				g.collision[1] = cars[i].pos[1];
				g.collision[2] = cars[j].pos[0];
				g.collision[3] = cars[j].pos[1];
				g.crash[0] = i;
				g.crash[1] = j;
				++g.ncollisions;
			}
		}
	}
}

void render(void)
{
	clear_screen();
	XSetForeground(g.dpy, g.gc, 0x00ff0000);
	//draw intersection
	//XSetForeground(g.dpy, g.gc, 0x00ffff55);
	XSetForeground(g.dpy, g.gc, 0x00aaaa55);
	drawRectangle(intersection.pos[0] - (intersection.w >> 1),
					intersection.pos[1] - (intersection.h >> 1),
					intersection.w, intersection.h);
	//roadway color
	XSetForeground(g.dpy, g.gc, 0x00333333);
	//roadway north
	fillRectangle(	intersection.pos[0] - (intersection.w >> 1),
					0,
					intersection.w,
					(g.yres >> 1) - (intersection.h >> 1) - 1);
	//roadway south
	fillRectangle(	intersection.pos[0] - (intersection.w >> 1),
					(g.yres>>1) + (intersection.h >> 1) + 2,
					intersection.w,
					(g.yres >> 1) - (intersection.h >> 1));
	//roadway east
	fillRectangle(	0,
					(g.yres>>1) - (intersection.h >> 1),
					(g.xres >> 1) - (intersection.w >> 1) - 1,
					intersection.h);
	//roadway west
	fillRectangle(	(g.xres >> 1) + (intersection.w >> 1) + 2,
					(g.yres>>1) - (intersection.h >> 1),
					(g.xres >> 1) - (intersection.w >> 1) - 1,
					intersection.h);
	//Highway dashed lines
	XSetForeground(g.dpy, g.gc, 0x00666655);
	//dashed lines north
	int i;
	int y1 = 0;
	int y2 = 20;
	for (i=0; i<5; i++) {
		fillRectangle(intersection.pos[0] - 2, y1, 4, y2);
		y1 += y2 + 9;
	}
	//dashed lines south
	y1 = 20;
	y2 = 20;
	for (i=0; i<5; i++) {
		fillRectangle(intersection.pos[0] - 2, g.yres - 1 - y1, 4, y2);
		y1 += 29;
	}
	//dashed lines west
	int x1 = 0;
	int x2 = 20;
	for (i=0; i<5; i++) {
		fillRectangle(x1, intersection.pos[1] - 2, x2, 4);
		x1 += x2 + 9;
	}
	//dashed lines east
	x1 = 20;
	x2 = 20;
	for (i=0; i<5; i++) {
		fillRectangle(g.xres - 1 - x1, intersection.pos[1] - 2, x2, 4);
		x1 += 29;
	}

	// print crosses
		int line_start = 40;
		int spacing = 20;
		char * dirs[] = {"Up", "Down", "Left", "Right"};
		char pbuff[100];

		XSetForeground(g.dpy, g.gc, 0x00FFFFFF);
		for (size_t j = 0; j < 4; j++) {
			sprintf(pbuff, "%s: %ld", dirs[j], g.numCross[j]);
			drawString(g.xres*(3.0/4.0), line_start + (j*spacing), pbuff);
		}



	//draw cars
	unsigned int col[] = {
		0x00ff0000, 0x0000ff00, 0x004444ff, 0x00ff00ff, 0x00ffcc88,
		0x00ff0000, 0x0000ff00, 0x004444ff, 0x00ff00ff, 0x00ffcc88};
	//int i;
	char carNumBuf[5];
	for (i=0; i<NCARS; i++) {
		XSetForeground(g.dpy, g.gc, col[i]);
		fillRectangle(cars[i].pos[0] - (cars[i].w >> 1),
						cars[i].pos[1] - (cars[i].h >> 1),
						cars[i].w, cars[i].h);
		XSetForeground(g.dpy, g.gc, 0x00000000);
		sprintf(carNumBuf, "%d", i);
		drawString(cars[i].pos[0]-1, cars[i].pos[1]+1, carNumBuf);
	}


	//Key options...
	int y = 20;
	char str[100];
	sprintf(str, "'C' = see collisions");
	XSetForeground(g.dpy, g.gc, 0x0000ff00);
	drawString(20, y, str);
	y += 16;
	sprintf(str, "'S' = slow mode");
	XSetForeground(g.dpy, g.gc, 0x0000ff00);
	drawString(20, y, str);
	y += 16;
	sprintf(str, " n collisions: %i", g.ncollisions);
	XSetForeground(g.dpy, g.gc, 0x00ffff00);
	drawString(20, y, str);
	if (g.show_collisions) {
		if (g.collision_flag) {
			//show collision with lines drawn fron corner.
			XSetForeground(g.dpy, g.gc, col[g.crash[0]]);
			drawLine(g.xres-1, 0, g.collision[0], g.collision[1]);
			XSetForeground(g.dpy, g.gc, col[g.crash[1]]);
			drawLine(g.xres-1, 0, g.collision[2], g.collision[3]);
		}
	}
	
}


























