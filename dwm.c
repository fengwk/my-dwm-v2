/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <ctype.h> /* for making tab label lowercase, very tiny standard library */
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>
#include <sys/time.h>
#include <math.h>

#include "drw.h"
#include "util.h"

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
                               * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy)))
#define ISVISIBLE(C)            ((C->mon->isoverview || C->tags & C->mon->tagset[C->mon->seltags]))
#define HIDDEN(C)               ((getstate(C->win) == IconicState))
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
#define TEXTW(X)                (drw_fontset_getwidth(drw, (X)) + lrpad)
#define ISTAG(tag) ((tag & TAGMASK) == (selmon->tagset[selmon->seltags] & TAGMASK))

#define SYSTEM_TRAY_REQUEST_DOCK    0

/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY      0
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_FOCUS_IN             4
#define XEMBED_MODALITY_ON         10

#define XEMBED_MAPPED              (1 << 0)
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_WINDOW_DEACTIVATE    2

#define VERSION_MAJOR               0
#define VERSION_MINOR               0
#define XEMBED_EMBEDDED_VERSION (VERSION_MAJOR << 16) | VERSION_MINOR

#define OPAQUE                  0xffU

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel, SchemeHid }; /* color schemes */
enum { NetSupported, NetWMName, NetWMState, NetWMCheck,
       NetSystemTray, NetSystemTrayOP, NetSystemTrayOrientation, NetSystemTrayOrientationHorz,
       NetWMFullscreen, NetActiveWindow, NetWMWindowType,
       NetWMWindowTypeDialog, NetClientList, NetLast }; /* EWMH atoms */
enum { Manager, Xembed, XembedInfo, XLast }; /* Xembed atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
       ClkClientWin, ClkRootWin, ClkLast }; /* clicks */
enum { WIN_UP, WIN_DOWN, WIN_LEFT, WIN_RIGHT }; /* movewin */
enum { V_EXPAND, V_REDUCE, H_EXPAND, H_REDUCE }; /* resizewins */
enum { MOUSE_UP, MOUSE_RIGHT, MOUSE_DOWM, MOUSE_LEFT }; /* movemouse */
enum { SWITCH_WIN,  SWITCH_SAME_TAG,  SWITCH_DIFF_TAG,  SWITCH_SMART }; /* switch mode */

typedef union {
  int i;
  unsigned int ui;
  float f;
  const void *v;
} Arg;

typedef struct {
	unsigned int signum;
	void (*func)(const Arg *);
	const Arg arg;
} Signal;

typedef struct {
  unsigned int click;
  unsigned int mask;
  unsigned int button;
  void (*func)(const Arg *arg);
  const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
  char name[256];
  float mina, maxa;
  int x, y, w, h;
  int oldx, oldy, oldw, oldh;
  int basew, baseh, incw, inch, maxw, maxh, minw, minh, hintsvalid;
  int bw, oldbw;
  unsigned int tags;
  int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
  Client *next;
  Client *snext;
  Monitor *mon;
  Window win;
  int fixrender;
  int hid;
};

typedef struct {
  unsigned int mod;
  KeySym keysym;
  void (*func)(const Arg *);
  const Arg arg;
} Key;

typedef struct {
  const char *symbol;
  void (*arrange)(Monitor *);
  const int append; // 是否以追加形式新增client
} Layout;

typedef struct Pertag Pertag;
typedef struct ClientAccNode ClientAccNode;
struct Monitor {
  char ltsymbol[16];
  float mfact;
  int nmaster;
  int num;
  int by;               /* bar geometry */
  int btw;              /* width of tasks portion of bar */
	int bt;               /* number of tasks */
  int mx, my, mw, mh;   /* screen size */
  int wx, wy, ww, wh;   /* window area  */
  int gappih;           /* horizontal gap between windows */
  int gappiv;           /* vertical gap between windows */
  int gappoh;           /* horizontal outer gaps */
  int gappov;           /* vertical outer gaps */
  unsigned int seltags;
  unsigned int sellt;
  unsigned int tagset[2];
  int showbar;
  int topbar;
  Client *clients;
  Client *sel;
  Client *stack;
  Monitor *next;
  Window barwin;
  const Layout *lt[2];
  Pertag *pertag;
  int isoverview; // 是否为预览模式
  ClientAccNode *accstack;
};

typedef struct {
  const char *class;
  const char *instance;
  const char *title;
  unsigned int tags;
  int isfloating;
  int monitor;
  int hideborder;
  int fixrender;
  int x;
  int y;
  int width;
  int height;
} Rule;

typedef struct Systray Systray;
struct Systray {
  Window win;
  Client *icons;
};

typedef struct {
  const char *key;
  const char *val;
} TagMapEntry;

struct ClientAccNode {
  Client *c;
  ClientAccNode *next;
};

/* function declarations */
static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachbottom(Client *c);
static void attachstack(Client *c);
static int isappend(Client *c);
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static void dumpstatus(void);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static void drawbars(void);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusmonbyclient(Client *c);
static void focusstack(const Arg *arg);
static void focusclient(const Arg *arg);
static Atom getatomprop(Client *c, Atom prop);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static unsigned int getsystraywidth();
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static void hide(const Arg *arg);
static void hidewin(Client *c);
static void incnmaster(const Arg *arg);
static void keypress(XEvent *e);
static int fake_signal(void);
static void killclient(const Arg *arg);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static void monoclehid(Monitor *m);
static void motionnotify(XEvent *e);
static void movemouse(const Arg *arg);
static Client *nexttiled(Client *c);
static void pop(Client *c);
static void propertynotify(XEvent *e);
static void quit(const Arg *arg);
static Monitor *recttomon(int x, int y, int w, int h);
static void removesystrayicon(Client *i);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizebarwin(Monitor *m);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizemouse(const Arg *arg);
static void resizerequest(XEvent *e);
static void restack(Monitor *m);
static void run(void);
static void runautosh(const char autoblocksh[], const char autosh[]);
static void scan(void);
static int sendevent(Window w, Atom proto, int m, long d0, long d1, long d2, long d3, long d4);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void fullscreen(const Arg *arg);
static void getgaps(Monitor *m, int *oh, int *ov, int *ih, int *iv, unsigned int *nc);
static void getfacts(Monitor *m, int msize, int ssize, float *mf, float *sf, int *mr, int *sr);
static void setgaps(int oh, int ov, int ih, int iv);
static void incrgaps(const Arg *arg);
static void incrigaps(const Arg *arg);
static void incrogaps(const Arg *arg);
static void incrohgaps(const Arg *arg);
static void incrovgaps(const Arg *arg);
static void incrihgaps(const Arg *arg);
static void incrivgaps(const Arg *arg);
static void togglesmartgaps(const Arg *arg);
static void togglegaps(const Arg *arg);
static void defaultgaps(const Arg *arg);
static void setlayout(const Arg *arg);
static void setmfact(const Arg *arg);
static void setup(void);
static void seturgent(Client *c, int urg);
static void show(const Arg *arg);
static void showall(const Arg *arg);
static void showwin(Client *c, int clearflag);
static void showhide(Client *c);
static void sigchld(int unused);
static int solitary(Client *c);
static void spawn(const Arg *arg);
static Monitor *systraytomon(Monitor *m);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void tile(Monitor *m);
static void grid(Monitor *m);
static void togglebar(const Arg *arg);
static void togglefloating(const Arg *arg);
static void togglefloatingattach(const Arg *arg);
static unsigned int findscratch(Client **c);
static void togglescratch(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void toggleoverview(const Arg *arg);
static void unfocus(Client *c, int setfocus);
static void togglewin(const Arg *arg);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatesystray(void);
static void updatesystrayicongeom(Client *i, int w, int h);
static void updatesystrayiconstate(Client *i, XPropertyEvent *ev);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void viewtoleft(const Arg *arg);
static void viewtoright(const Arg *arg);
static void view(const Arg *arg);
static void listwindowpids(Window w, Window pids[], int sz);
static int inwindowpids(Window w, Window pids[], int sz);
static int isdialog(Client *c);
static int isprevclient(int switchmode, Client *src, Client *prev);
static void setselmon(Monitor *newselmon);
static void setmonsel(Monitor *m, Client *c);
static void switchprevclient(const Arg *arg);
static void switchclient(Client *c);
static void addaccstack(Client *c);
static void removeaccstack(Client *c);
static void switchenternotify(const Arg *arg);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static Client *wintosystrayicon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void xinitvisual();
static void zoom(const Arg *arg);
static int inarea(int x, int y, int rx, int ry, int rw, int rh);
static void movewin(const Arg *arg);
static void resizewin(const Arg *arg);
static void mousefocus(const Arg *arg);
static void mousemove(const Arg *arg);
static const char* gettagdisplayname(Client* c);

/* variables */
static Systray *systray =  NULL;
static const char broken[] = "broken";
static const char autostartblocksh[] = "autostart_blocking.sh"; // 这个脚本会以阻塞的形式执行
static const char autostartsh[] = "autostart.sh"; // 这个脚本会以非阻塞形式执行，就是加了&
static const char autostopblocksh[] = "autostop_blocking.sh"; // 这个脚本会以阻塞的形式执行
static const char autostopsh[] = "autostop.sh"; // 这个脚本会以非阻塞形式执行，就是加了&
static const char dwmdir[] = "dwm"; // 自启动脚本的dir名称
static const char localshare[] = ".local/share"; // 自启动脚本的相对路径
static char stext[256];
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static int bh;               /* bar height */
static int lrpad;            /* sum of left and right padding for text */
static int smartgaps  = 1;   /* 1 means no outer gap when there is only one window */
static int enablegaps = 1;   /* enables gaps, used by togglegaps */
static long long beginmousemove = 0; // 开始movemouse的时间戳
static long long prevmousemove = 0; // 前一次movemouse的时间戳
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent]) (XEvent *) = {
  [ButtonPress] = buttonpress,
  [ClientMessage] = clientmessage,
  [ConfigureRequest] = configurerequest,
  [ConfigureNotify] = configurenotify,
  [DestroyNotify] = destroynotify,
  [EnterNotify] = enternotify, // 注释这行可以禁止鼠标悬浮聚焦
  [Expose] = expose,
  [FocusIn] = focusin,
  [KeyPress] = keypress,
  [MappingNotify] = mappingnotify,
  [MapRequest] = maprequest,
  [MotionNotify] = motionnotify,
  [PropertyNotify] = propertynotify,
  [ResizeRequest] = resizerequest,
  [UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast], xatom[XLast];
static int running = 1;
static Cur *cursor[CurLast];
static Clr **scheme;
static Display *dpy;
static Drw *drw;
static Monitor *mons, *selmon;
static Window root, wmcheckwin;

static int useargb = 0;
static Visual *visual;
static int depth;
static Colormap cmap;

static int enableenternotify = 1;

/* configuration, allows nested code to access above variables */
#include "config.h"

struct Pertag {
  unsigned int curtag, prevtag; /* current and previous tag */
  int nmasters[LENGTH(tags) + 1]; /* number of windows in master area */
  float mfacts[LENGTH(tags) + 1]; /* mfacts per tag */
  unsigned int sellts[LENGTH(tags) + 1]; /* selected layouts */
  const Layout *ltidxs[LENGTH(tags) + 1][2]; /* matrix of tags and layouts indexes  */
  int showbars[LENGTH(tags) + 1]; /* display bar for the current tag */
};

static unsigned int scratchtag = 1 << LENGTH(tags);

unsigned int tagw[LENGTH(tags)];

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

/* function implementations */
void
applyrules(Client *c)
{
  const char *class, *instance;
  unsigned int i;
  const Rule *r;
  Monitor *m;
  XClassHint ch = { NULL, NULL };

  /* rule matching */
  c->isfloating = 0;
  c->tags = 0;
  c->fixrender = 0;
  XGetClassHint(dpy, c->win, &ch);
  class    = ch.res_class ? ch.res_class : broken;
  instance = ch.res_name  ? ch.res_name  : broken;

  for (i = 0; i < LENGTH(rules); i++) {
    r = &rules[i];
    if ((!r->title || strstr(c->name, r->title))
    && (!r->class || strstr(class, r->class))
    && (!r->instance || strstr(instance, r->instance)))
    {
      c->isfloating = r->isfloating;
      c->tags |= r->tags;
      c->bw = r->hideborder ? 0 : borderpx;
      c->fixrender = r->fixrender ? 1 : 0;
      for (m = mons; m && m->num != r->monitor; m = m->next);
      if (m)
        c->mon = m;
      if (r->isfloating) {
        if (r->width > 0)
          c->w = r->width;
        if (r->height > 0)
          c->h = r->height;
        if (r->x > 0) 
          c->x = r->x;
        else if (r->x < 0) 
          c->x = c->mon->wx + c->mon->ww + r->x;
        if (r->y > 0) 
          c->y = r->y;
        else if (r->y < 0)
          c->y = c->mon->wy + c->mon->wh + r->y;
      }
    }
  }
  if (ch.res_class)
    XFree(ch.res_class);
  if (ch.res_name)
    XFree(ch.res_name);
  c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : (c->mon->tagset[c->mon->seltags] & TAGMASK);
}

int
applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact)
{
  int baseismin;
  Monitor *m = c->mon;

  /* set minimum possible */
  *w = MAX(1, *w);
  *h = MAX(1, *h);
  if (interact) {
    if (*x > sw)
      *x = sw - WIDTH(c);
    if (*y > sh)
      *y = sh - HEIGHT(c);
    if (*x + *w + 2 * c->bw < 0)
      *x = 0;
    if (*y + *h + 2 * c->bw < 0)
      *y = 0;
  } else {
    if (*x >= m->wx + m->ww)
      *x = m->wx + m->ww - WIDTH(c);
    if (*y >= m->wy + m->wh)
      *y = m->wy + m->wh - HEIGHT(c);
    if (*x + *w + 2 * c->bw <= m->wx)
      *x = m->wx;
    if (*y + *h + 2 * c->bw <= m->wy)
      *y = m->wy;
  }
  if (*h < bh)
    *h = bh;
  if (*w < bh)
    *w = bh;
  if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
    if (!c->hintsvalid)
      updatesizehints(c);
    /* see last two sentences in ICCCM 4.1.2.3 */
    baseismin = c->basew == c->minw && c->baseh == c->minh;
    if (!baseismin) { /* temporarily remove base dimensions */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for aspect limits */
    if (c->mina > 0 && c->maxa > 0) {
      if (c->maxa < (float)*w / *h)
        *w = *h * c->maxa + 0.5;
      else if (c->mina < (float)*h / *w)
        *h = *w * c->mina + 0.5;
    }
    if (baseismin) { /* increment calculation requires this */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for increment value */
    if (c->incw)
      *w -= *w % c->incw;
    if (c->inch)
      *h -= *h % c->inch;
    /* restore base dimensions */
    *w = MAX(*w + c->basew, c->minw);
    *h = MAX(*h + c->baseh, c->minh);
    if (c->maxw)
      *w = MIN(*w, c->maxw);
    if (c->maxh)
      *h = MIN(*h, c->maxh);
  }
  return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void
arrange(Monitor *m)
{
  if (m)
    showhide(m->stack);
  else for (m = mons; m; m = m->next)
    showhide(m->stack);
  if (m) {
    arrangemon(m);
    restack(m);
  } else for (m = mons; m; m = m->next)
    arrangemon(m);
}

void
arrangemon(Monitor *m)
{
  if (m->isoverview) {
    // 如果是overview模式则使用网格布局查看
    strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
    grid(m);
  } else {
    strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
    if (m->lt[m->sellt]->arrange)
      m->lt[m->sellt]->arrange(m);
  }
}

 void
attachbottom(Client *c)
{
  Client **tc;
  c->next = NULL;
  for (tc = &c->mon->clients; *tc; tc = &(*tc)->next);
  *tc = c;
}

void
attach(Client *c)
{
  c->next = c->mon->clients;
  c->mon->clients = c;
}

void
attachstack(Client *c)
{
  c->snext = c->mon->stack;
  c->mon->stack = c;
}

int isappend(Client *c) {
  Monitor *m = c->mon;
  return (selmon && selmon->isoverview) || (m && m->lt[m->sellt] && m->lt[m->sellt]->append);
}

void
buttonpress(XEvent *e)
{
  unsigned int i, x, click;
  Arg arg = {0};
  Client *c;
  Monitor *m;
  XButtonPressedEvent *ev = &e->xbutton;

  click = ClkRootWin;
  /* focus monitor if necessary */
  if ((m = wintomon(ev->window)) && m != selmon) {
    unfocus(selmon->sel, 1);
    setselmon(m);
    focus(NULL);
  }
  if (ev->window == selmon->barwin) {
    i = x = 0;
    unsigned int occ = 0;
    for(c = m->clients; c; c=c->next)
      occ |= c->tags;
    do {
      /* Do not reserve space for vacant tags */
      if (!(occ & 1 << i || m->tagset[m->seltags] & 1 << i))
        continue;
      x += tagw[i];
    } while (ev->x >= x && ++i < LENGTH(tags));
    // 处理bar点击事件
    if (i < LENGTH(tags)) {
      click = ClkTagBar;
      arg.ui = 1 << i;
    } else if (ev->x < x + TEXTW(selmon->ltsymbol))
      click = ClkLtSymbol;
    else if (ev->x > selmon->ww - TEXTW(stext) - getsystraywidth() + lrpad - 2) /* 2px right padding */
      click = ClkStatusText;
    else {
      x += TEXTW(selmon->ltsymbol);
			c = m->clients;

			if (c) {
				do {
					if (!ISVISIBLE(c))
						continue;
					else
						x +=(1.0 / (double)m->bt) * m->btw;
				} while (ev->x > x && (c = c->next));

				click = ClkWinTitle;
				arg.v = c;
			}
    }
  } else if ((c = wintoclient(ev->window))) {
    focus(c);
    restack(selmon);
    XAllowEvents(dpy, ReplayPointer, CurrentTime);
    click = ClkClientWin;
  }
  for (i = 0; i < LENGTH(buttons); i++)
    if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
    && CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
      buttons[i].func((click == ClkTagBar || click == ClkWinTitle) && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}

void
checkotherwm(void)
{
  xerrorxlib = XSetErrorHandler(xerrorstart);
  /* this causes an error if some other window manager is running */
  XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
  XSync(dpy, False);
  XSetErrorHandler(xerror);
  XSync(dpy, False);
}

void
cleanup(void)
{
  Arg a = {.ui = ~0};
  Layout foo = { "", NULL };
  Monitor *m;
  size_t i;

  view(&a);
  selmon->lt[selmon->sellt] = &foo;
  for (m = mons; m; m = m->next)
    while (m->stack)
      unmanage(m->stack, 0);
  XUngrabKey(dpy, AnyKey, AnyModifier, root);
  while (mons)
    cleanupmon(mons);
  if (showsystray) {
    XUnmapWindow(dpy, systray->win);
    XDestroyWindow(dpy, systray->win);
    free(systray);
  }
  for (i = 0; i < CurLast; i++)
    drw_cur_free(drw, cursor[i]);
  for (i = 0; i < LENGTH(colors); i++)
    free(scheme[i]);
  free(scheme);
  XDestroyWindow(dpy, wmcheckwin);
  drw_free(drw);
  XSync(dpy, False);
  XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
  XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

void
cleanupmon(Monitor *mon)
{
  Monitor *m;

  if (mon == mons)
    mons = mons->next;
  else {
    for (m = mons; m && m->next != mon; m = m->next);
    m->next = mon->next;
  }
  XUnmapWindow(dpy, mon->barwin);
  XDestroyWindow(dpy, mon->barwin);
  if (mon->pertag) {
    free(mon->pertag);
  }
  ClientAccNode *accnode = mon->accstack;
  while (accnode) {
    ClientAccNode *next = accnode->next;
    free(accnode);
    accnode = next;
  }
  free(mon);
}

void
clientmessage(XEvent *e)
{
  XWindowAttributes wa;
  XSetWindowAttributes swa;
  XClientMessageEvent *cme = &e->xclient;
  Client *c = wintoclient(cme->window);

  if (showsystray && cme->window == systray->win && cme->message_type == netatom[NetSystemTrayOP]) {
    /* add systray icons */
    if (cme->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK) {
      if (!(c = (Client *)calloc(1, sizeof(Client))))
        die("fatal: could not malloc() %u bytes\n", sizeof(Client));
      if (!(c->win = cme->data.l[2])) {
        free(c);
        return;
      }
      c->mon = selmon;
      c->next = systray->icons;
      systray->icons = c;
      XGetWindowAttributes(dpy, c->win, &wa);
      c->x = c->oldx = c->y = c->oldy = 0;
      c->w = c->oldw = wa.width;
      c->h = c->oldh = wa.height;
      c->oldbw = wa.border_width;
      c->bw = 0;
      c->isfloating = True;
      /* reuse tags field as mapped status */
      c->tags = 1;
      updatesizehints(c);
      updatesystrayicongeom(c, wa.width, wa.height);
      XAddToSaveSet(dpy, c->win);
      XSelectInput(dpy, c->win, StructureNotifyMask | PropertyChangeMask | ResizeRedirectMask);
      XReparentWindow(dpy, c->win, systray->win, 0, 0);
      /* use parents background color */
      swa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
      XChangeWindowAttributes(dpy, c->win, CWBackPixel, &swa);
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_EMBEDDED_NOTIFY, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
      /* FIXME not sure if I have to send these events, too */
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_FOCUS_IN, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_MODALITY_ON, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
      XSync(dpy, False);
      resizebarwin(selmon);
      updatesystray();
      setclientstate(c, NormalState);
    }
    return;
  }
  if (!c)
    return;
  if (cme->message_type == netatom[NetWMState]) {
    if (cme->data.l[1] == netatom[NetWMFullscreen]
    || cme->data.l[2] == netatom[NetWMFullscreen])
      setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
        || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
  } else if (cme->message_type == netatom[NetActiveWindow]) {
    if (c != selmon->sel && !c->isurgent)
      seturgent(c, 1);
    // 收到通知后切换到指定client
    switchclient(c);
  }
}

void
configure(Client *c)
{
  XConfigureEvent ce;

  ce.type = ConfigureNotify;
  ce.display = dpy;
  ce.event = c->win;
  ce.window = c->win;
  ce.x = c->x;
  ce.y = c->y;
  ce.width = c->w;
  ce.height = c->h;
  ce.border_width = c->bw;
  ce.above = None;
  ce.override_redirect = False;
  XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void
configurenotify(XEvent *e)
{
  Monitor *m;
  Client *c;
  XConfigureEvent *ev = &e->xconfigure;
  int dirty;

  /* TODO: updategeom handling sucks, needs to be simplified */
  if (ev->window == root) {
    dirty = (sw != ev->width || sh != ev->height);
    sw = ev->width;
    sh = ev->height;
    if (updategeom() || dirty) {
      drw_resize(drw, sw, bh);
      updatebars();
      for (m = mons; m; m = m->next) {
        for (c = m->clients; c; c = c->next)
          if (c->isfullscreen)
            resizeclient(c, m->mx, m->my, m->mw, m->mh);
        resizebarwin(m);
      }
      focus(NULL);
      arrange(NULL);
    }
  }
}

void
configurerequest(XEvent *e)
{
  Client *c;
  Monitor *m;
  XConfigureRequestEvent *ev = &e->xconfigurerequest;
  XWindowChanges wc;

  if ((c = wintoclient(ev->window))) {
    if (ev->value_mask & CWBorderWidth)
      c->bw = ev->border_width;
    else if (c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
      m = c->mon;
      if (ev->value_mask & CWX) {
        c->oldx = c->x;
        c->x = m->mx + ev->x;
      }
      if (ev->value_mask & CWY) {
        c->oldy = c->y;
        c->y = m->my + ev->y;
      }
      if (ev->value_mask & CWWidth) {
        c->oldw = c->w;
        c->w = ev->width;
      }
      if (ev->value_mask & CWHeight) {
        c->oldh = c->h;
        c->h = ev->height;
      }
      if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
        c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
      if ((c->y + c->h) > m->my + m->mh && c->isfloating)
        c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
      if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
        configure(c);
      if (ISVISIBLE(c))
        XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
    } else
      configure(c);
  } else {
    wc.x = ev->x;
    wc.y = ev->y;
    wc.width = ev->width;
    wc.height = ev->height;
    wc.border_width = ev->border_width;
    wc.sibling = ev->above;
    wc.stack_mode = ev->detail;
    XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
  }
  XSync(dpy, False);
}

Monitor *
createmon(void)
{
  Monitor *m;
  unsigned int i;

  m = ecalloc(1, sizeof(Monitor));
  m->tagset[0] = m->tagset[1] = 1;
  m->mfact = mfact;
  m->nmaster = nmaster;
  m->showbar = showbar;
  m->topbar = topbar;
  m->gappih = gappih;
  m->gappiv = gappiv;
  m->gappoh = gappoh;
  m->gappov = gappov;
  m->lt[0] = &layouts[0];
  m->lt[1] = &layouts[1 % LENGTH(layouts)];
  strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
  m->pertag = ecalloc(1, sizeof(Pertag));
  m->pertag->curtag = m->pertag->prevtag = 1;
  m->isoverview = 0;
  m->accstack = NULL;

  for (i = 0; i <= LENGTH(tags); i++) {
    m->pertag->nmasters[i] = m->nmaster;
    m->pertag->mfacts[i] = m->mfact;

    m->pertag->ltidxs[i][0] = m->lt[0];
    m->pertag->ltidxs[i][1] = m->lt[1];
    m->pertag->sellts[i] = m->sellt;

    m->pertag->showbars[i] = m->showbar;
  }

  return m;
}

// dump当前dwm的状态
static
void dumpstatus(void) {
  char cmd[1024];

  if (selmon) {
    snprintf(cmd, sizeof(cmd),
        "/bin/bash -c 'mkdir -p ~/.cache/dwm/status && echo %d > ~/.cache/dwm/status/selmon'", selmon->num);
    system(cmd);
  }

  if (selmon && selmon->sel) {
    snprintf(cmd, sizeof(cmd),
        "/bin/bash -c 'mkdir -p ~/.cache/dwm/status && echo %lu > ~/.cache/dwm/status/selwin'", selmon->sel->win);
    system(cmd);
  }
}

void
destroynotify(XEvent *e)
{
  Client *c;
  XDestroyWindowEvent *ev = &e->xdestroywindow;

  if ((c = wintoclient(ev->window)))
    unmanage(c, 1);
  else if ((c = wintosystrayicon(ev->window))) {
    removesystrayicon(c);
    resizebarwin(selmon);
    updatesystray();
  }
}

void
detach(Client *c)
{
  Client **tc;

  for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
  *tc = c->next;

  removeaccstack(c);
}

void
detachstack(Client *c)
{
  Client **tc, *t;

  for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
  *tc = c->snext;

  if (c == c->mon->sel) {
    for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext);
    setmonsel(c->mon, t);
  }
}

Monitor *
dirtomon(int dir)
{
  Monitor *m = NULL;

  if (dir > 0) {
    if (!(m = selmon->next))
      m = mons;
  } else if (selmon == mons)
    for (m = mons; m->next; m = m->next);
  else
    for (m = mons; m->next != selmon; m = m->next);
  return m;
}

// 获取用于标签的客户端名称
const char* 
gettagdisplayname(Client* c) {
  XClassHint ch = { NULL, NULL };
  XGetClassHint(dpy, c->win, &ch);
  const char* name = ch.res_class;
  for (int i = 0; i < LENGTH(tagnamemap); i++) {
    if (name && strcmp(tagnamemap[i].key, name) == 0) {
      name = tagnamemap[i].val;
      break;
    }
  }
  return name;
}

void
drawbar(Monitor *m)
{
  /*
   - `x`：绘制状态栏时的横坐标。
   - `tw`：状态栏中文本的宽度。
   - `stw`：系统托盘的宽度。
   - `mw`：每个客户端名称的最大宽度。
   - `ew`：超出最大宽度的宽度总和。
   - `boxs`：浮动客户端名称旁边小方块的大小。
   - `boxw`：浮动客户端名称旁边小方块的宽度。
   - `i`：循环计数器。
   - `occ`：当前可见的客户端所在标签的二进制表示。
   - `urg`：当前有紧急状态的客户端所在标签的二进制表示。
   - `n`：当前可见的客户端数量。
   - `c`：指向客户端的指针。
   - `tagdisp`：标签名称和 master 客户端名称的组
   */
  int x, w, tw = 0, stw = 0, mw, ew = 0, scm;
  int boxs = drw->fonts->h / 9;
  int boxw = drw->fonts->h / 6 + 2;
  unsigned int i, occ = 0, urg = 0, n = 0;
  Client *c;
  char tagdisp[64];
  const char *masterclientontag[LENGTH(tags)];

  if(showsystray && m == systraytomon(m))
    stw = getsystraywidth();

  // 绘制状态栏
  /* draw status first so it can be overdrawn by tags later */
  if (m == selmon) { /* status is only drawn on selected monitor */
    drw_setscheme(drw, scheme[SchemeNorm]);
    tw = TEXTW(stext) - lrpad / 2 + 2; /* 2px right padding */
    drw_text(drw, m->ww - tw - stw, 0, tw, bh, lrpad / 2 - 2, stext, 0);
  }

  resizebarwin(m);

  // 初始化各个标签master客户端数组
  for (i = 0; i < LENGTH(tags); i++)
    masterclientontag[i] = NULL;

  for (c = m->clients; c; c = c->next) {
    if (ISVISIBLE(c))
      n++; // 计算可展示的客户端数量
    occ |= c->tags;
    if (c->isurgent)
      urg |= c->tags;
    // 获取master客户端的名称
    for (i = 0; i < LENGTH(tags); i++)
      if (!masterclientontag[i] && c->tags & (1<<i)) {
        masterclientontag[i] = gettagdisplayname(c);
      }
  }
  x = 0;
  // 绘制tags
  if (m->isoverview) {
      w = TEXTW(overviewtag);
      drw_setscheme(drw, scheme[SchemeSel]);
      drw_text(drw, x, 0, w, bh, lrpad / 2, overviewtag, 0);
      x += w;
  } else {
    for (i = 0; i < LENGTH(tags); i++) {
      /* Do not draw vacant tags */
      if(!(occ & 1 << i || m->tagset[m->seltags] & 1 << i))
        continue;
      if (masterclientontag[i])
        snprintf(tagdisp, 64, ptagf, tags[i], masterclientontag[i]);
      else
        snprintf(tagdisp, 64, etagf, tags[i]);
      masterclientontag[i] = tagdisp;
      tagw[i] = w = TEXTW(masterclientontag[i]);
      drw_setscheme(drw, scheme[m->tagset[m->seltags] & 1 << i ? SchemeSel : SchemeNorm]);
      drw_text(drw, x, 0, w, bh, lrpad / 2, masterclientontag[i], urg & 1 << i);
      x += w;
    }
  }

  // 绘制布局名称
  w = TEXTW(m->ltsymbol);
  drw_setscheme(drw, scheme[SchemeNorm]);
  x = drw_text(drw, x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);

  // 绘制clients
  if ((w = m->ww - tw - stw - x) > bh) {
    if (n > 0) {
      int remainder = w % n;
      int tabw = (1.0 / (double)n) * w + 1;
      for (c = m->clients; c; c = c->next) {
        if (!ISVISIBLE(c))
          continue;
        if (m->sel == c) {
          scm = SchemeSel;
        } else if (HIDDEN(c)) {
          scm = SchemeHid;
        } else {
          scm = SchemeNorm;
        }
        drw_setscheme(drw, scheme[scm]);

        if (remainder >= 0) {
          if (remainder == 0) {
            tabw--;
          }
          remainder--;
        }

        // 绘制标题
        if (c->hid) {
          char *hidename = ecalloc(1, strlen(c->name) + strlen(HIDETAG) + 1);
          sprintf(hidename, "%s%s", HIDETAG, c->name);
          drw_text(drw, x, 0, tabw, bh, lrpad / 2, hidename, 0);
          free(hidename);
        } else {
          drw_text(drw, x, 0, tabw, bh, lrpad / 2, c->name, 0);
        }
        // 绘制浮动标
        if (c->isfloating) {
          drw_rect(drw, x + boxs, boxs, boxw, boxw, c->isfixed, 0);
        }
        x += tabw;
      }
    } else {
      drw_setscheme(drw, scheme[SchemeNorm]);
      drw_rect(drw, x, 0, w, bh, 1, 1);
    }
  }
  m->bt = n; // 可见客户端数量
	m->btw = w;  // clients可用的宽度
  drw_map(drw, m->barwin, 0, 0, m->ww - stw, bh);
}

void
drawbars(void)
{
  Monitor *m;

  for (m = mons; m; m = m->next)
    drawbar(m);
}

void
enternotify(XEvent *e)
{
  if (!enableenternotify) {
    return;
  }
  Client *c;
  Monitor *m;
  XCrossingEvent *ev = &e->xcrossing;

  if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
    return;
  c = wintoclient(ev->window);
  m = c ? c->mon : wintomon(ev->window);
  if (m != selmon) {
    unfocus(selmon->sel, 1);
    setselmon(m); // 这会导致使用rofi时错误的monitor聚焦（按照光标所在位置聚焦），但去除后会导致自动聚焦桌面失败
    // 目前暂时无法复现自动聚焦桌面失败的情况 344b87f4ebd9d0c8894ec419f5d149e3426fbc54，但确实会使selmon语义错误
  } else if (!c || c == selmon->sel)
    return;
  focus(c);
}

void
expose(XEvent *e)
{
  Monitor *m;
  XExposeEvent *ev = &e->xexpose;

  if (ev->count == 0 && (m = wintomon(ev->window))) {
    drawbar(m);
    if (m == selmon)
      updatesystray();
  }
}

void
focus(Client *c)
{
  // 如果c为NULL或c在当前选中tags下不可见，则找到第一个可见的c
  if (!c || !ISVISIBLE(c))
    for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext);
  // 如果当前选中的窗口不是c则先unfocus当前选中窗口
  if (selmon->sel && selmon->sel != c) {
    unfocus(selmon->sel, 0);
  }
  if (c) {
    if (c->mon != selmon)
      setselmon(c->mon);
    // 取消urgent标识
    if (c->isurgent)
      seturgent(c, 0); 
    // 如果当前窗口是隐藏的，暂时清理当前hid
    if (HIDDEN(c))
      showwin(c, 0);
    // 浮动窗口或浮动布局在聚焦时将窗口置顶
    // if (c->isfloating || (c->mon && c->mon->sellt && !c->mon->lt[c->mon->sellt]->arrange))
    //   XRaiseWindow(dpy, c->win);
    detachstack(c);
    attachstack(c);
    grabbuttons(c, 1);
    // 设置边框
    XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
    // 下面的代码为了避免边框闪烁，但这会使得monocle模式切换回title时因为无法重新触发focus导致边框不显示
    // 我暂时还没有遇到边框闪烁的问题，因此暂时注释这个补丁的内容
    /* Avoid flickering when another client appears and the border
     * is restored */
    // if (!solitary(c)) {
    //   XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
    // }
    setfocus(c);
  } else {
    XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
  }
  setmonsel(selmon, c);
  drawbars();
  addaccstack(c);
}

/* there are some broken focus acquiring clients needing extra handling */
void
focusin(XEvent *e)
{
  XFocusChangeEvent *ev = &e->xfocus;

  if (selmon->sel && ev->window != selmon->sel->win)
    setfocus(selmon->sel);
}

void
focusmon(const Arg *arg)
{
  Monitor *m;

  if (!mons->next)
    return;
  if ((m = dirtomon(arg->i)) == selmon)
    return;
  unfocus(selmon->sel, 0);
  setselmon(m);
  focus(NULL);
}

void
focusmonbyclient(Client *c) {
  Monitor *m;
  if (!c || !mons->next || (m = c->mon) == selmon) {
    return;
  }
  unfocus(selmon->sel, 0);
  setselmon(m);
  focus(NULL);
}

void
focusstack(const Arg *arg)
{
  Client *c = NULL, *i;
  int inc = arg->i;

  // if no client selected AND exclude hidden client; if client selected but fullscreened
  if ((!selmon->sel) || (selmon->sel && selmon->sel->isfullscreen && lockfullscreen))
    return;
  if (!selmon->clients)
    return;
  if (inc > 0) {
    if (selmon->sel)
      for (c = selmon->sel->next; c && (!ISVISIBLE(c)); c = c->next);
    if (!c)
      for (c = selmon->clients; c && (!ISVISIBLE(c)); c = c->next);
  } else {
    if (selmon->sel) {
      for (i = selmon->clients; i != selmon->sel; i = i->next)
        if (ISVISIBLE(i))
          c = i;
    } else
      c = selmon->clients;
    if (!c)
      for (; i; i = i->next)
        if (ISVISIBLE(i))
          c = i;
  }
  if (c) {
    switchclient(c);
  }
}

void
focusclient(const Arg *arg) {
  if (arg && arg->v) {
    Client *c = (Client *) arg->v;
    switchclient(c);
  }
}

Atom
getatomprop(Client *c, Atom prop)
{
  int di;
  unsigned long dl;
  unsigned char *p = NULL;
  Atom da, atom = None;
  /* FIXME getatomprop should return the number of items and a pointer to
   * the stored data instead of this workaround */
  Atom req = XA_ATOM;
  if (prop == xatom[XembedInfo])
    req = xatom[XembedInfo];

  if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, req,
    &da, &di, &dl, &dl, &p) == Success && p) {
    atom = *(Atom *)p;
    if (da == xatom[XembedInfo] && dl == 2)
      atom = ((Atom *)p)[1];
    XFree(p);
  }
  return atom;
}

int
getrootptr(int *x, int *y)
{
  int di;
  unsigned int dui;
  Window dummy;

  return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long
getstate(Window w)
{
  int format;
  long result = -1;
  unsigned char *p = NULL;
  unsigned long n, extra;
  Atom real;

  if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
    &real, &format, &n, &extra, (unsigned char **)&p) != Success)
    return -1;
  if (n != 0)
    result = *p;
  XFree(p);
  return result;
}

unsigned int
getsystraywidth()
{
  unsigned int w = 0;
  Client *i;
  if(showsystray)
    for(i = systray->icons; i; w += i->w + systrayspacing, i = i->next) ;
  return w ? w + systrayspacing : 1;
}

int
gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
  char **list = NULL;
  int n;
  XTextProperty name;

  if (!text || size == 0)
    return 0;
  text[0] = '\0';
  if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
    return 0;
  if (name.encoding == XA_STRING) {
    strncpy(text, (char *)name.value, size - 1);
  } else if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
    strncpy(text, *list, size - 1);
    XFreeStringList(list);
  }
  text[size - 1] = '\0';
  XFree(name.value);
  return 1;
}

void
grabbuttons(Client *c, int focused)
{
  updatenumlockmask();
  {
    unsigned int i, j;
    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
    XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
    if (!focused)
      XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
        BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
    for (i = 0; i < LENGTH(buttons); i++)
      if (buttons[i].click == ClkClientWin)
        for (j = 0; j < LENGTH(modifiers); j++)
          XGrabButton(dpy, buttons[i].button,
            buttons[i].mask | modifiers[j],
            c->win, False, BUTTONMASK,
            GrabModeAsync, GrabModeSync, None, None);
  }
}

void
grabkeys(void)
{
  updatenumlockmask();
  {
    unsigned int i, j;
    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
    KeyCode code;

    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    for (i = 0; i < LENGTH(keys); i++)
      if ((code = XKeysymToKeycode(dpy, keys[i].keysym)))
        for (j = 0; j < LENGTH(modifiers); j++)
          XGrabKey(dpy, code, keys[i].mod | modifiers[j], root,
            True, GrabModeAsync, GrabModeAsync);
  }
}

void
hide(const Arg *arg)
{
	hidewin(selmon->sel);
	focus(NULL);
	arrange(selmon);
}

void
hidewin(Client *c) {
	if (!c || HIDDEN(c))
		return;

	Window w = c->win;
	static XWindowAttributes ra, ca;

	// more or less taken directly from blackbox's hide() function
	XGrabServer(dpy);
	XGetWindowAttributes(dpy, root, &ra);
	XGetWindowAttributes(dpy, w, &ca);
	// prevent UnmapNotify events
	XSelectInput(dpy, root, ra.your_event_mask & ~SubstructureNotifyMask);
	XSelectInput(dpy, w, ca.your_event_mask & ~StructureNotifyMask);
	XUnmapWindow(dpy, w);
	setclientstate(c, IconicState);
	XSelectInput(dpy, root, ra.your_event_mask);
	XSelectInput(dpy, w, ca.your_event_mask);
	XUngrabServer(dpy);
  c->hid = 1;
}

void
incnmaster(const Arg *arg)
{
  unsigned int i;
  selmon->nmaster = MAX(selmon->nmaster + arg->i, 0);
  for(i=0; i<LENGTH(tags); ++i)
    if(selmon->tagset[selmon->seltags] & 1<<i)
      selmon->pertag->nmasters[i+1] = selmon->nmaster;
  
  if(selmon->pertag->curtag == 0)
  {
    selmon->pertag->nmasters[0] = selmon->nmaster;
  }
  arrange(selmon);
}

#ifdef XINERAMA
static int
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
  while (n--)
    if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
    && unique[n].width == info->width && unique[n].height == info->height)
      return 0;
  return 1;
}
#endif /* XINERAMA */

void
keypress(XEvent *e)
{
  unsigned int i;
  KeySym keysym;
  XKeyEvent *ev;

  ev = &e->xkey;
  keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
  for (i = 0; i < LENGTH(keys); i++)
    if (keysym == keys[i].keysym
    && (keys[i].mod == NOMODKEY || CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)) // 支持无mod快捷键
    && keys[i].func) {
      keys[i].func(&(keys[i].arg));
      // 仅应用一个快捷键函数就退出，这样通过合适编排快捷键顺序来避免快捷键冲突
      break;
    }
}

int
fake_signal(void)
{
	char fsignal[256];
	char indicator[9] = "fsignal:";
	char str_signum[16];
	int i, v, signum;
	size_t len_fsignal, len_indicator = strlen(indicator);

	// Get root name property
	if (gettextprop(root, XA_WM_NAME, fsignal, sizeof(fsignal))) {
		len_fsignal = strlen(fsignal);

		// Check if this is indeed a fake signal
		if (len_indicator > len_fsignal ? 0 : strncmp(indicator, fsignal, len_indicator) == 0) {
			memcpy(str_signum, &fsignal[len_indicator], len_fsignal - len_indicator);
			str_signum[len_fsignal - len_indicator] = '\0';

			// Convert string value into managable integer
			for (i = signum = 0; i < strlen(str_signum); i++) {
				v = str_signum[i] - '0';
				if (v >= 0 && v <= 9) {
					signum = signum * 10 + v;
				}
			}

			// Check if a signal was found, and if so handle it
			if (signum)
				for (i = 0; i < LENGTH(signals); i++)
					if (signum == signals[i].signum && signals[i].func)
						signals[i].func(&(signals[i].arg));

			// A fake signal was sent
			return 1;
		}
	}

	// No fake signal was sent, so proceed with update
	return 0;
}

void
killclient(const Arg *arg)
{
  if (!selmon->sel)
    return;
  if (!sendevent(selmon->sel->win, wmatom[WMDelete], NoEventMask, wmatom[WMDelete], CurrentTime, 0 , 0, 0)) {
    XGrabServer(dpy);
    XSetErrorHandler(xerrordummy);
    XSetCloseDownMode(dpy, DestroyAll);
    XKillClient(dpy, selmon->sel->win);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }
}

void
manage(Window w, XWindowAttributes *wa)
{
  Client *c, *t = NULL;
  Window trans = None;
  XWindowChanges wc;

  c = ecalloc(1, sizeof(Client));
  c->win = w;
  /* geometry */
  c->x = c->oldx = wa->x;
  c->y = c->oldy = wa->y;
  c->w = c->oldw = wa->width;
  c->h = c->oldh = wa->height;
  c->oldbw = wa->border_width;
  c->bw = borderpx; // 先设置bw，以便可以在applayrules中重定义，原先在下面的位置设置，如果提前有问题再行处理

  updatetitle(c);
  if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
    c->mon = t->mon;
    c->tags = (t->tags & TAGMASK);
  } else {
    c->mon = selmon;
    applyrules(c);
  }

  if (c->x + WIDTH(c) > c->mon->wx + c->mon->ww)
    c->x = c->mon->wx + c->mon->ww - WIDTH(c);
  if (c->y + HEIGHT(c) > c->mon->wy + c->mon->wh)
    c->y = c->mon->wy + c->mon->wh - HEIGHT(c);
  c->x = MAX(c->x, c->mon->wx);
  c->y = MAX(c->y, c->mon->wy);
  // c->bw = borderpx;

  // 处理浮动布局下的窗口
  // if (!c->mon->lt[c->mon->sellt] && c->x == 0 && c->y == 0) {
  //   c->w = c->mon->ww * 3 / 5;
  //   c->h = c->mon->wh * 3 / 5;
  //   c->x = c->mon->wx + (c->mon->ww - c->w) / 2;
  //   c->y = c->mon->wy + (c->mon->wh - c->h) / 2;
  // }

  if (!strcmp(c->name, scratchpadname)) {
    c->mon->tagset[c->mon->seltags] |= c->tags = scratchtag;
    c->isfloating = True;
    c->x = c->mon->wx + (c->mon->ww / 2 - WIDTH(c) / 2);
    c->y = c->mon->wy + (c->mon->wh / 2 - HEIGHT(c) / 2);
  }

  wc.border_width = c->bw;
  XConfigureWindow(dpy, w, CWBorderWidth, &wc);
  XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
  configure(c); /* propagates border_width, if size doesn't change */
  updatewindowtype(c);
  updatesizehints(c);
  updatewmhints(c);
  XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
  grabbuttons(c, 0);
  if (!c->isfloating)
    c->isfloating = c->oldstate = trans != None || c->isfixed;
  if (c->isfloating)
    XRaiseWindow(dpy, c->win);
  if (isappend(c)) {
    attachbottom(c);
  } else {
    attach(c);
  }
  attachstack(c);
  XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
    (unsigned char *) &(c->win), 1);
  XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */
  if (!HIDDEN(c)) {
		setclientstate(c, NormalState);
  }
  if (c->mon == selmon) {
    unfocus(selmon->sel, 0);
  }
  setmonsel(c->mon, c);
  arrange(c->mon);
	if (!HIDDEN(c)) {
		XMapWindow(dpy, c->win);
  }
  focus(NULL);
}

void
mappingnotify(XEvent *e)
{
  XMappingEvent *ev = &e->xmapping;

  XRefreshKeyboardMapping(ev);
  if (ev->request == MappingKeyboard)
    grabkeys();
}

void
maprequest(XEvent *e)
{
  static XWindowAttributes wa;
  XMapRequestEvent *ev = &e->xmaprequest;
  Client *i;
  if ((i = wintosystrayicon(ev->window))) {
    sendevent(i->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0, systray->win, XEMBED_EMBEDDED_VERSION);
    resizebarwin(selmon);
    updatesystray();
  }

  if (!XGetWindowAttributes(dpy, ev->window, &wa) || wa.override_redirect)
    return;
  if (!wintoclient(ev->window))
    manage(ev->window, &wa);
}

void
monocle(Monitor *m)
{
  unsigned int n = 0;
  Client *c;

  for (c = m->clients; c; c = c->next)
    if (ISVISIBLE(c))
      n++;
  if (n > 0) /* override layout symbol */
    snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
  for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
    resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

void
monoclehid(Monitor *m)
{
  unsigned int n = 0;
  Client *c;

  for (c = m->clients; c; c = c->next)
    if (ISVISIBLE(c))
      n++;
  if (n > 0) /* override layout symbol */
    snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
  for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
    resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

void
motionnotify(XEvent *e)
{
  static Monitor *mon = NULL;
  Monitor *m;
  XMotionEvent *ev = &e->xmotion;

  if (ev->window != root)
    return;
  if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
    unfocus(selmon->sel, 1);
    setselmon(m);
    focus(NULL);
  }
  mon = m;
}

void
movemouse(const Arg *arg)
{
  int x, y, ocx, ocy, nx, ny;
  Client *c;
  Monitor *m;
  XEvent ev;
  Time lasttime = 0;

  if (!(c = selmon->sel))
    return;
  if (c->isfullscreen) /* no support moving fullscreen windows by mouse */
    return;
  restack(selmon);
  ocx = c->x;
  ocy = c->y;
  if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
    None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
    return;
  if (!getrootptr(&x, &y))
    return;
  do {
    XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
    switch(ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[ev.type](&ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;

      nx = ocx + (ev.xmotion.x - x);
      ny = ocy + (ev.xmotion.y - y);
      if (abs(selmon->wx - nx) < snap)
        nx = selmon->wx;
      else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
        nx = selmon->wx + selmon->ww - WIDTH(c);
      if (abs(selmon->wy - ny) < snap)
        ny = selmon->wy;
      else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
        ny = selmon->wy + selmon->wh - HEIGHT(c);
      if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
      && (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
        togglefloating(NULL);
      if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
        resize(c, nx, ny, c->w, c->h, 1);
      break;
    }
  } while (ev.type != ButtonRelease);
  XUngrabPointer(dpy, CurrentTime);
  if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
    sendmon(c, m);
    setselmon(m);
    focus(NULL);
  }
}

/**
 * 从c开始（包含c）找到下一个可见的平铺client
 */
Client *
nexttiled(Client *c)
{
  // 从客户端链表中找到下一个不是浮动且可见的client实例
  for (; c && (c->isfloating || !ISVISIBLE(c) || HIDDEN(c)); c = c->next);
  return c;
}

void
pop(Client *c)
{
  detach(c);
  attach(c);
  focus(c);
  arrange(c->mon);
}

void
propertynotify(XEvent *e)
{
  Client *c;
  Window trans;
  XPropertyEvent *ev = &e->xproperty;

  if ((c = wintosystrayicon(ev->window))) {
    if (ev->atom == XA_WM_NORMAL_HINTS) {
      updatesizehints(c);
      updatesystrayicongeom(c, c->w, c->h);
    }
    else
      updatesystrayiconstate(c, ev);
    resizebarwin(selmon);
    updatesystray();
  }
  if ((ev->window == root) && (ev->atom == XA_WM_NAME)) {
    if (!fake_signal())
      updatestatus();
  }
  else if (ev->state == PropertyDelete)
    return; /* ignore */
  else if ((c = wintoclient(ev->window))) {
    switch(ev->atom) {
    default: break;
    case XA_WM_TRANSIENT_FOR:
      if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) &&
        (c->isfloating = (wintoclient(trans)) != NULL))
        arrange(c->mon);
      break;
    case XA_WM_NORMAL_HINTS:
      c->hintsvalid = 0;
      break;
    case XA_WM_HINTS:
      updatewmhints(c);
      drawbars();
      break;
    }
    if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
      updatetitle(c);
      if (c == c->mon->sel)
        drawbar(c->mon);
    }
    if (ev->atom == netatom[NetWMWindowType])
      updatewindowtype(c);
  }
}

void
quit(const Arg *arg)
{
  // fix: reloading dwm keeps all the hidden clients hidden
	Monitor *m;
	Client *c;
	for (m = mons; m; m = m->next) {
		if (m) {
			for (c = m->stack; c; c = c->next)
				if (c && HIDDEN(c)) showwin(c, 1);
		}
	}

  running = 0;
}

Monitor *
recttomon(int x, int y, int w, int h)
{
  Monitor *m, *r = selmon;
  int a, area = 0;

  for (m = mons; m; m = m->next)
    if ((a = INTERSECT(x, y, w, h, m)) > area) {
      area = a;
      r = m;
    }
  return r;
}

void
removesystrayicon(Client *i)
{
  Client **ii;

  if (!showsystray || !i)
    return;
  for (ii = &systray->icons; *ii && *ii != i; ii = &(*ii)->next);
  if (ii)
    *ii = i->next;
  free(i);
}


void
resize(Client *c, int x, int y, int w, int h, int interact)
{
  if (applysizehints(c, &x, &y, &w, &h, interact)) {
    if (c->fixrender) {
      // 对于一些特殊的应用，例如xmind，存在resize后无法刷新的情况，下面的多次操作可以使这些应用的视图刷新生效，在找到真正的解决办法之前这会是一种无可奈何的方案
      resizeclient(c, x+1, y+1, w, h);
      usleep(25000);
      resizeclient(c, x, y, w, h);
    } else {
      resizeclient(c, x, y, w, h);
    }
  }
}

void
resizebarwin(Monitor *m) {
  unsigned int w = m->ww;
  if (showsystray && m == systraytomon(m))
    w -= getsystraywidth();
  XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, w, bh);
}

void
resizeclient(Client *c, int x, int y, int w, int h)
{
  XWindowChanges wc;

  c->oldx = c->x; c->x = wc.x = x;
  c->oldy = c->y; c->y = wc.y = y;
  c->oldw = c->w; c->w = wc.width = w;
  c->oldh = c->h; c->h = wc.height = h;
  wc.border_width = c->bw;
  if (solitary(c)) {
    c->w = wc.width += c->bw * 2;
    c->h = wc.height += c->bw * 2;
    wc.border_width = 0;
  }
  XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
  configure(c);
  XSync(dpy, False);
}

void
resizemouse(const Arg *arg)
{
  int ocx, ocy, nw, nh;
  Client *c;
  Monitor *m;
  XEvent ev;
  Time lasttime = 0;

  if (!(c = selmon->sel))
    return;
  if (c->isfullscreen) /* no support resizing fullscreen windows by mouse */
    return;
  restack(selmon);
  ocx = c->x;
  ocy = c->y;
  if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
    None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
    return;
  XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
  do {
    XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
    switch(ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[ev.type](&ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;

      nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
      nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
      if (c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww
      && c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh)
      {
        if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
        && (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
          togglefloating(NULL);
      }
      if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
        resize(c, c->x, c->y, nw, nh, 1);
      break;
    }
  } while (ev.type != ButtonRelease);
  XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
  XUngrabPointer(dpy, CurrentTime);
  while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
  if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
    sendmon(c, m);
    setselmon(m);
    focus(NULL);
  }
}

void
resizerequest(XEvent *e)
{
  XResizeRequestEvent *ev = &e->xresizerequest;
  Client *i;

  if ((i = wintosystrayicon(ev->window))) {
    updatesystrayicongeom(i, ev->width, ev->height);
    resizebarwin(selmon);
    updatesystray();
  }
}

void
restack(Monitor *m)
{
  Client *c;
  XEvent ev;
  XWindowChanges wc;

  drawbar(m);
  if (!m->sel)
    return;
  if (m->sel->isfloating || !m->lt[m->sellt]->arrange) // 当前client是浮动的，或布局是浮动的，将当前窗口置顶
    XRaiseWindow(dpy, m->sel->win);
  if (m->lt[m->sellt]->arrange) {
    wc.stack_mode = Below;
    wc.sibling = m->barwin;
    for (c = m->stack; c; c = c->snext)
      if (!c->isfloating && ISVISIBLE(c)) {
        // 窗口非浮动，则重新配置窗口
        // https://linux.die.net/man/3/xconfigurewindow
        XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
        wc.sibling = c->win;
      }
  }
  XSync(dpy, False);
  while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void
run(void)
{
  XEvent ev;
  /* main event loop */
  XSync(dpy, False);
  while (running && !XNextEvent(dpy, &ev))
    if (handler[ev.type])
      handler[ev.type](&ev); /* call handler */
}

void
runautosh(const char autoblocksh[], const char autosh[])
{
  char *pathpfx;
  char *path;
  char *xdgdatahome;
  char *home;
  struct stat sb;

  if ((home = getenv("HOME")) == NULL)
    /* this is almost impossible */
    return;

  /* if $XDG_DATA_HOME is set and not empty, use $XDG_DATA_HOME/dwm,
   * otherwise use ~/.local/share/dwm as autostart script directory
   */
  xdgdatahome = getenv("XDG_DATA_HOME");
  if (xdgdatahome != NULL && *xdgdatahome != '\0') {
    /* space for path segments, separators and nul */
    pathpfx = ecalloc(1, strlen(xdgdatahome) + strlen(dwmdir) + 2);

    if (sprintf(pathpfx, "%s/%s", xdgdatahome, dwmdir) <= 0) {
      free(pathpfx);
      return;
    }
  } else {
    /* space for path segments, separators and nul */
    pathpfx = ecalloc(1, strlen(home) + strlen(localshare)
                         + strlen(dwmdir) + 3);

    if (sprintf(pathpfx, "%s/%s/%s", home, localshare, dwmdir) < 0) {
      free(pathpfx);
      return;
    }
  }

  /* check if the autostart script directory exists */
  if (! (stat(pathpfx, &sb) == 0 && S_ISDIR(sb.st_mode))) {
    /* the XDG conformant path does not exist or is no directory
     * so we try ~/.dwm instead
     */
    char *pathpfx_new = realloc(pathpfx, strlen(home) + strlen(dwmdir) + 3);
    if(pathpfx_new == NULL) {
      free(pathpfx);
      return;
    }
    pathpfx = pathpfx_new;

    if (sprintf(pathpfx, "%s/.%s", home, dwmdir) <= 0) {
      free(pathpfx);
      return;
    }
  }

  /* try the blocking script first */
  path = ecalloc(1, strlen(pathpfx) + strlen(autoblocksh) + 2);
  if (sprintf(path, "%s/%s", pathpfx, autoblocksh) <= 0) {
    free(path);
    free(pathpfx);
  }

  if (access(path, X_OK) == 0)
    system(path);

  /* now the non-blocking script */
  if (sprintf(path, "%s/%s", pathpfx, autosh) <= 0) {
    free(path);
    free(pathpfx);
  }

  if (access(path, X_OK) == 0)
    system(strcat(path, " &"));

  free(pathpfx);
  free(path);
}

void
scan(void)
{
  unsigned int i, num;
  Window d1, d2, *wins = NULL;
  XWindowAttributes wa;

  if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
    for (i = 0; i < num; i++) {
      if (!XGetWindowAttributes(dpy, wins[i], &wa)
      || wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
        continue;
      if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
        manage(wins[i], &wa);
    }
    for (i = 0; i < num; i++) { /* now the transients */
      if (!XGetWindowAttributes(dpy, wins[i], &wa))
        continue;
      if (XGetTransientForHint(dpy, wins[i], &d1)
      && (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
        manage(wins[i], &wa);
    }
    if (wins)
      XFree(wins);
  }
}

void
sendmon(Client *c, Monitor *m)
{
  if (c->mon == m)
    return;
  unfocus(c, 1);
  detach(c);
  detachstack(c);
  c->mon = m;
  c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
  if (isappend(c)) {
    attachbottom(c);
  } else {
    attach(c);
  }
  attachstack(c);
  focus(NULL);
  arrange(NULL);
}

void
setclientstate(Client *c, long state)
{
  long data[] = { state, None };

  XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
    PropModeReplace, (unsigned char *)data, 2);
}

int
sendevent(Window w, Atom proto, int mask, long d0, long d1, long d2, long d3, long d4)
{
  int n;
  Atom *protocols, mt;
  int exists = 0;
  XEvent ev;

  if (proto == wmatom[WMTakeFocus] || proto == wmatom[WMDelete]) {
    mt = wmatom[WMProtocols];
    if (XGetWMProtocols(dpy, w, &protocols, &n)) {
      while (!exists && n--)
        exists = protocols[n] == proto;
      XFree(protocols);
    }
  }
  else {
    exists = True;
    mt = proto;
  }
  if (exists) {
    ev.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = mt;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = d0;
    ev.xclient.data.l[1] = d1;
    ev.xclient.data.l[2] = d2;
    ev.xclient.data.l[3] = d3;
    ev.xclient.data.l[4] = d4;
    XSendEvent(dpy, w, False, mask, &ev);
  }
  return exists;
}

void
setfocus(Client *c)
{
  if (!c->neverfocus) {
    XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
    XChangeProperty(dpy, root, netatom[NetActiveWindow],
      XA_WINDOW, 32, PropModeReplace,
      (unsigned char *) &(c->win), 1);
  }
  sendevent(c->win, wmatom[WMTakeFocus], NoEventMask, wmatom[WMTakeFocus], CurrentTime, 0, 0, 0);
}

void
setfullscreen(Client *c, int fullscreen)
{
  if (fullscreen && !c->isfullscreen) {
    XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
      PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
    c->isfullscreen = 1;
    c->oldstate = c->isfloating;
    c->oldbw = c->bw;
    c->bw = 0;
    c->isfloating = 1;
    resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
    XRaiseWindow(dpy, c->win);
  } else if (!fullscreen && c->isfullscreen){
    XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
      PropModeReplace, (unsigned char*)0, 0);
    c->isfullscreen = 0;
    c->isfloating = c->oldstate;
    c->bw = c->oldbw;
    c->x = c->oldx;
    c->y = c->oldy;
    c->w = c->oldw;
    c->h = c->oldh;
    resizeclient(c, c->x, c->y, c->w, c->h);
    arrange(c->mon);
  }
}

void
getgaps(Monitor *m, int *oh, int *ov, int *ih, int *iv, unsigned int *nc)
{
  unsigned int n, oe, ie;
  #if PERTAG_PATCH
  oe = ie = selmon->pertag->enablegaps[selmon->pertag->curtag];
  #else
  oe = ie = enablegaps;
  #endif // PERTAG_PATCH
  Client *c;

  for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
  if (smartgaps && n == 1) {
    oe = 0; // outer gaps disabled when only one client
  }

  *oh = m->gappoh*oe; // outer horizontal gap
  *ov = m->gappov*oe; // outer vertical gap
  *ih = m->gappih*ie; // inner horizontal gap
  *iv = m->gappiv*ie; // inner vertical gap
  *nc = n;            // number of clients
}

void
getfacts(Monitor *m, int msize, int ssize, float *mf, float *sf, int *mr, int *sr)
{
  unsigned int n;
  float mfacts, sfacts;
  int mtotal = 0, stotal = 0;
  Client *c;

  for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
  mfacts = MIN(n, m->nmaster);
  sfacts = n - m->nmaster;

  for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++)
    if (n < m->nmaster)
      mtotal += msize / mfacts;
    else
      stotal += ssize / sfacts;

  *mf = mfacts; // total factor of master area
  *sf = sfacts; // total factor of stack area
  *mr = msize - mtotal; // the remainder (rest) of pixels after an even master split
  *sr = ssize - stotal; // the remainder (rest) of pixels after an even stack split
}

void
setgaps(int oh, int ov, int ih, int iv)
{
  if (oh < 0) oh = 0;
  if (ov < 0) ov = 0;
  if (ih < 0) ih = 0;
  if (iv < 0) iv = 0;

  selmon->gappoh = oh;
  selmon->gappov = ov;
  selmon->gappih = ih;
  selmon->gappiv = iv;
  arrange(selmon);
}

void
togglesmartgaps(const Arg *arg) {
  smartgaps = !smartgaps;
  arrange(selmon);
}

void
togglegaps(const Arg *arg)
{
  enablegaps = !enablegaps;
  arrange(selmon);
}

void
defaultgaps(const Arg *arg)
{
  setgaps(gappoh, gappov, gappih, gappiv);
}

void
incrgaps(const Arg *arg)
{
  setgaps(
    selmon->gappoh + arg->i,
    selmon->gappov + arg->i,
    selmon->gappih + arg->i,
    selmon->gappiv + arg->i
  );
}

void
incrigaps(const Arg *arg)
{
  setgaps(
    selmon->gappoh,
    selmon->gappov,
    selmon->gappih + arg->i,
    selmon->gappiv + arg->i
  );
}

void
incrogaps(const Arg *arg)
{
  setgaps(
    selmon->gappoh + arg->i,
    selmon->gappov + arg->i,
    selmon->gappih,
    selmon->gappiv
  );
}

void
incrohgaps(const Arg *arg)
{
  setgaps(
    selmon->gappoh + arg->i,
    selmon->gappov,
    selmon->gappih,
    selmon->gappiv
  );
}

void
incrovgaps(const Arg *arg)
{
  setgaps(
    selmon->gappoh,
    selmon->gappov + arg->i,
    selmon->gappih,
    selmon->gappiv
  );
}

void
incrihgaps(const Arg *arg)
{
  setgaps(
    selmon->gappoh,
    selmon->gappov,
    selmon->gappih + arg->i,
    selmon->gappiv
  );
}

void
incrivgaps(const Arg *arg)
{
  setgaps(
    selmon->gappoh,
    selmon->gappov,
    selmon->gappih,
    selmon->gappiv + arg->i
  );
}

Layout *last_layout;
void
fullscreen(const Arg *arg)
{
  if (selmon->showbar) {
    for(last_layout = (Layout *)layouts; last_layout != selmon->lt[selmon->sellt]; last_layout++);
    setlayout(&((Arg) { .v = &layouts[1] }));
  } else {
    setlayout(&((Arg) { .v = last_layout }));
  }
  togglebar(arg);
}

void
setlayout(const Arg *arg)
{

  Arg *a = &((Arg) {0});
  if (LAYOUT_TOGGLE) {
    // 如果当前布局与默认布局不一致则切换布局
    if (arg && arg->v != selmon->lt[selmon->sellt]) {
      a->v = arg->v;
    }
  } else {
    a->v = arg->v;
  }

  unsigned int i;
  if (!a || !a->v || a->v != selmon->lt[selmon->sellt])
    selmon->sellt ^= 1;
  if (a && a->v)
    selmon->lt[selmon->sellt] = (Layout *)a->v;
  strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);

  for(i=0; i<LENGTH(tags); ++i)
    if(selmon->tagset[selmon->seltags] & 1<<i)
    {
      selmon->pertag->ltidxs[i+1][selmon->sellt] = selmon->lt[selmon->sellt]; 
      selmon->pertag->sellts[i+1] = selmon->sellt;
    }
  
  if(selmon->pertag->curtag == 0)
  {
    selmon->pertag->ltidxs[0][selmon->sellt] = selmon->lt[selmon->sellt]; 
    selmon->pertag->sellts[0] = selmon->sellt;
  }

  if (selmon->sel)
    arrange(selmon);
  else
    drawbar(selmon);
}

/* arg > 1.0 will set mfact absolutely */
void
setmfact(const Arg *arg)
{
  float f;
  unsigned int i;

  if (!arg || !selmon->lt[selmon->sellt]->arrange)
    return;
  f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
  if (arg->f == 0.0)
    f = mfact;
  if (f < 0.05 || f > 0.95)
    return;
  selmon->mfact = f;
  for(i=0; i<LENGTH(tags); ++i)
    if(selmon->tagset[selmon->seltags] & 1<<i)
      selmon->pertag->mfacts[i+1] = f;

  if(selmon->pertag->curtag == 0)
  {
    selmon->pertag->mfacts[0] = f;
  }
  arrange(selmon);
}

void
setup(void)
{
  int i;
  XSetWindowAttributes wa;
  Atom utf8string;

  /* clean up any zombies immediately */
  sigchld(0);

  /* init screen */
  screen = DefaultScreen(dpy);
  sw = DisplayWidth(dpy, screen);
  sh = DisplayHeight(dpy, screen);
  root = RootWindow(dpy, screen);
  xinitvisual();
  drw = drw_create(dpy, screen, root, sw, sh, visual, depth, cmap);
  if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
    die("no fonts could be loaded.");
  lrpad = drw->fonts->h;
  bh = drw->fonts->h + 2;
  updategeom();
  /* init atoms */
  utf8string = XInternAtom(dpy, "UTF8_STRING", False);
  wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
  wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
  wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
  netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
  netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
  netatom[NetSystemTray] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_S0", False);
  netatom[NetSystemTrayOP] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
  netatom[NetSystemTrayOrientation] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION", False);
  netatom[NetSystemTrayOrientationHorz] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION_HORZ", False);
  netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
  netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
  netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
  netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
  netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
  netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
  netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
  xatom[Manager] = XInternAtom(dpy, "MANAGER", False);
  xatom[Xembed] = XInternAtom(dpy, "_XEMBED", False);
  xatom[XembedInfo] = XInternAtom(dpy, "_XEMBED_INFO", False);
  /* init cursors */
  // https://tronche.com/gui/x/xlib/appendix/b/
  cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
  // cursor[CurResize] = drw_cur_create(drw, XC_sizing);
  cursor[CurResize] = drw_cur_create(drw, XC_bottom_right_corner);
  cursor[CurMove] = drw_cur_create(drw, XC_fleur);
  /* init appearance */
  scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
  for (i = 0; i < LENGTH(colors); i++)
    scheme[i] = drw_scm_create(drw, colors[i], alphas[i], 3);
  /* init system tray */
  updatesystray();
  /* init bars */
  updatebars();
  updatestatus();
  /* supporting window for NetWMCheck */
  wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
  XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
    PropModeReplace, (unsigned char *) &wmcheckwin, 1);
  XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
    PropModeReplace, (unsigned char *) "dwm", 3);
  XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
    PropModeReplace, (unsigned char *) &wmcheckwin, 1);
  /* EWMH support per view */
  XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
    PropModeReplace, (unsigned char *) netatom, NetLast);
  XDeleteProperty(dpy, root, netatom[NetClientList]);
  /* select events */
  wa.cursor = cursor[CurNormal]->cursor;
  wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
    |ButtonPressMask|PointerMotionMask|EnterWindowMask
    |LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
  XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
  XSelectInput(dpy, root, wa.event_mask);
  grabkeys();
  focus(NULL);
}

void
seturgent(Client *c, int urg)
{
  XWMHints *wmh;

  c->isurgent = urg;
  if (!(wmh = XGetWMHints(dpy, c->win)))
    return;
  wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
  XSetWMHints(dpy, c->win, wmh);
  XFree(wmh);
}

void
show(const Arg *arg)
{
	showwin(selmon->sel, 1);
}

void
showall(const Arg *arg)
{
	Client *c = NULL;
	for (c = selmon->clients; c; c = c->next) {
		if (ISVISIBLE(c))
			showwin(c, 1);
	}
	if (!selmon->sel) {
		for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next);
		if (c)
			focus(c);
	}
	restack(selmon);
}

void
showwin(Client *c, int clearflag)
{
  if (!c) {
    return;
  }
  if (clearflag) {
    c->hid = 0;
  }
	if (!HIDDEN(c))
		return;

	XMapWindow(dpy, c->win);
	setclientstate(c, NormalState);
	arrange(c->mon);

}

void
showhide(Client *c)
{
  if (!c)
    return;
  if (ISVISIBLE(c)) {
    /* show clients top down */
    XMoveWindow(dpy, c->win, c->x, c->y);
    if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
      resize(c, c->x, c->y, c->w, c->h, 0);
    showhide(c->snext);
  } else {
    /* hide clients bottom up */
    showhide(c->snext);
    XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
  }
}

void
sigchld(int unused)
{
  if (signal(SIGCHLD, sigchld) == SIG_ERR)
    die("can't install SIGCHLD handler:");
  while (0 < waitpid(-1, NULL, WNOHANG));
}

int
solitary(Client *c)
{
  return ((nexttiled(c->mon->clients) == c && !nexttiled(c->next)) // 只有一个平铺client
      || &monocle == c->mon->lt[c->mon->sellt]->arrange) // 或当前是monocle布局
      && !c->isfullscreen && !c->isfloating && !selmon->isoverview // 且client不处于这3种状态
      && NULL != c->mon->lt[c->mon->sellt]->arrange; // 且不是NULL，也就是浮动的布局
}

void
spawn(const Arg *arg)
{
  if (arg->v == dmenucmd)
    dmenumon[0] = '0' + selmon->num;
  if (fork() == 0) {
    if (dpy)
      close(ConnectionNumber(dpy));
    setsid();
    execvp(((char **)arg->v)[0], (char **)arg->v);
    die("dwm: execvp '%s' failed:", ((char **)arg->v)[0]);
  }
}

void
tag(const Arg *arg)
{
  if (selmon->sel && arg->ui & TAGMASK) {
    selmon->sel->tags = arg->ui & TAGMASK;
    // 跳到新的tag
    view(arg);
    // 聚焦到客户端
    focus(NULL);
    // 重新布局且刷新
    arrange(selmon);
  }
}

void
tagmon(const Arg *arg)
{
  Client *c = selmon->sel;
  if (!selmon->sel || !mons->next)
    return;
  sendmon(c, dirtomon(arg->i));
  switchclient(c);
}

/**
 * 网格布局
 */
void
grid(Monitor *m) {
  unsigned int i, n;
  unsigned int cx, cy, cw, ch;
  unsigned int dx;
  unsigned int cols, rows, overcols;
  Client *c;

  int oh, ov, ih, iv; // o-外侧 i-内侧 v-垂直 h-水平
  getgaps(m, &oh, &ov, &ih, &iv, &n);

  if (n == 0)
          return;
  if (n == 1) {
          c = nexttiled(m->clients);
          cw = (m->ww - 2 * ov) * 0.7;
          ch = (m->wh - 2 * oh) * 0.65;
          // cw = (m->ww - 2 * ov) * 0.95;
          // ch = (m->wh - 2 * oh) * 0.95;
          resize(c, m->mx + (m->mw - cw) / 2 + ov,
                 m->my + (m->mh - ch) / 2 + oh, cw - 2 * c->bw,
                 ch - 2 * c->bw, 0);
          return;
  }
  if (n == 2) {
          c = nexttiled(m->clients);
          cw = (m->ww - 2 * ov - iv) / 2;
          ch = (m->wh - 2 * oh) * 0.65;
          resize(c, m->mx + ov, m->my + (m->mh - ch) / 2 + oh,
                 cw - 2 * c->bw, ch - 2 * c->bw, 0);
          resize(nexttiled(c->next), m->mx + cw + ov + iv,
                 m->my + (m->mh - ch) / 2 + oh, cw - 2 * c->bw,
                 ch - 2 * c->bw, 0);
          return;
  }

  for (cols = 0; cols <= n / 2; cols++)
          if (cols * cols >= n)
                  break;
  rows = (cols && (cols - 1) * cols >= n) ? cols - 1 : cols;
  ch = (m->wh - 2 * oh - (rows - 1) * ih) / rows;
  cw = (m->ww - 2 * ov - (cols - 1) * iv) / cols;

  overcols = n % cols;
  if (overcols)
          dx = (m->ww - overcols * cw - (overcols - 1) * iv) / 2 - ov;
  for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
          cx = m->wx + (i % cols) * (cw + iv);
          cy = m->wy + (i / cols) * (ch + ih);
          if (overcols && i >= n - overcols) {
                  cx += dx;
          }
          resize(c, cx + ov, cy + oh, cw - 2 * c->bw, ch - 2 * c->bw, 0);
  }
}

/*
 * Default tile layout + gaps
 */
static void
tile(Monitor *m)
{
  unsigned int i, n;
  int oh, ov, ih, iv;
  int mx = 0, my = 0, mh = 0, mw = 0;
  int sx = 0, sy = 0, sh = 0, sw = 0;
  float mfacts, sfacts;
  int mrest, srest;
  Client *c;

  getgaps(m, &oh, &ov, &ih, &iv, &n);
  if (n == 0)
    return;

  sx = mx = m->wx + ov;
  sy = my = m->wy + oh;
  mh = m->wh - 2*oh - ih * (MIN(n, m->nmaster) - 1);
  sh = m->wh - 2*oh - ih * (n - m->nmaster - 1);
  sw = mw = m->ww - 2*ov;

  if (m->nmaster && n > m->nmaster) {
    sw = (mw - iv) * (1 - m->mfact);
    mw = mw - iv - sw;
    sx = mx + mw + iv;
  }

  getfacts(m, mh, sh, &mfacts, &sfacts, &mrest, &srest);

  for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
    if (i < m->nmaster) {
      resize(c, mx, my, mw - (2*c->bw), (mh / mfacts) + (i < mrest ? 1 : 0) - (2*c->bw), 0);
      my += HEIGHT(c) + ih;
    } else {
      resize(c, sx, sy, sw - (2*c->bw), (sh / sfacts) + ((i - m->nmaster) < srest ? 1 : 0) - (2*c->bw), 0);
      sy += HEIGHT(c) + ih;
    }
}

void
togglebar(const Arg *arg)
{
  unsigned int i;
  selmon->showbar = !selmon->showbar;
  for(i=0; i<LENGTH(tags); ++i)
    if(selmon->tagset[selmon->seltags] & 1<<i)
      selmon->pertag->showbars[i+1] = selmon->showbar;

  if(selmon->pertag->curtag == 0)
  {
    selmon->pertag->showbars[0] = selmon->showbar;
  }
  updatebarpos(selmon);
  resizebarwin(selmon);
  if (showsystray) {
    XWindowChanges wc;
    if (!selmon->showbar)
      wc.y = selmon->topbar ? -bh : (selmon->wh + bh);
    else if (selmon->showbar) {
      wc.y = 0;
      if (!selmon->topbar)
        wc.y = selmon->mh - bh;
    }
    XConfigureWindow(dpy, systray->win, CWY, &wc);
  }
  arrange(selmon);
}

void
togglefloating(const Arg *arg)
{
  if (!selmon->sel)
    return;
  if (selmon->sel->isfullscreen) /* no support for fullscreen windows */
    return;
  selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
  if (selmon->sel->isfloating)
    resize(selmon->sel, selmon->sel->x, selmon->sel->y,
      selmon->sel->w, selmon->sel->h, 0);
  arrange(selmon);
}

void
togglefloatingattach(const Arg *arg)
{
  togglefloating(arg);
  // if (!selmon->sel)
  //   return;
  // if (selmon->sel->isfullscreen) /* no support for fullscreen windows */
  //   return;
  // selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
  // if (selmon->sel->isfloating) {
  //   Client *c = selmon->sel;
  //   int w = selmon->ww * 3 / 5;
  //   int h = selmon->wh * 3 / 5;
  //   int x = selmon->wx + (selmon->ww - c->w) / 2;
  //   int y = selmon->wy + (selmon->wh - c->h) / 2;
  //   resize(c, x, y, w, h, 0);
  //   // resize(selmon->sel, selmon->sel->x, selmon->sel->y,
  //   //     selmon->sel->w, selmon->sel->h, 0);
  // }
  // arrange(selmon);
}

unsigned int
findscratch(Client **sc) {
  unsigned int found = 0;
  for (*sc = selmon->clients; *sc && !(found = (*sc)->tags & scratchtag); *sc = (*sc)->next) {}
  return found;
}

void
togglescratch(const Arg *arg)
{
  Client *c;
  if (findscratch(&c)) {
    unsigned int newtagset = selmon->tagset[selmon->seltags] ^ scratchtag; // 改变scratchtag位的状态
    if (newtagset) {
      if (!(newtagset & scratchtag)) {
        // 如果收起了scratchtag需要清理前一个tagset的相应位，并且从accstack中移出
        selmon->tagset[selmon->seltags ^ 1] &= ~scratchtag;
        removeaccstack(c);
      }
      selmon->tagset[selmon->seltags] = newtagset;
      focus(NULL);
      arrange(selmon);
    }
    if (ISVISIBLE(c)) {
      focus(c);
      restack(selmon);
    }
  } else {
    spawn(arg);
  }
}

void
toggletag(const Arg *arg)
{
  unsigned int newtags;

  if (!selmon->sel)
    return;
  newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
  if (newtags) {
    selmon->sel->tags = newtags;
    focus(NULL);
    arrange(selmon);
  }
}

void
toggleview(const Arg *arg)
{
  unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);
  int i;

  if (newtagset & TAGMASK) {
    selmon->tagset[selmon->seltags] = newtagset;

    // TODO 这段逻辑看起来不会进入，使用一段时间后如果没问题可以删除
    // if (newtagset == ~0) {
    //   selmon->pertag->prevtag = selmon->pertag->curtag;
    //   selmon->pertag->curtag = 0;
    // }

    /* test if the user did not select the same tag */
    if (!(newtagset & 1 << (selmon->pertag->curtag - 1))) {
      selmon->pertag->prevtag = selmon->pertag->curtag;
      for (i = 0; !(newtagset & 1 << i); i++) ;
      selmon->pertag->curtag = i + 1;
    }

    /* apply settings for this view */
    selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
    selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
    selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
    selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
    selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];

    if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
      togglebar(NULL);

    focus(NULL);
    arrange(selmon);
  }
}

// 显示所有tag 或 跳转到聚焦窗口的tag
void
toggleoverview(const Arg *arg)
{
    if (selmon->sel && selmon->sel->isfullscreen) /* no support for fullscreen windows */
        return;

    uint target = selmon->sel ? selmon->sel->tags : selmon->seltags;
    if (selmon->isoverview) {
      // 隐藏所有应该hide窗口
      Client *c = NULL;
      for (c = selmon->clients; c; c = c->next) {
        if (ISVISIBLE(c) && c->hid && c != selmon->sel)
          hidewin(c);
      }
    } else {
      // 打开所有hide窗口
      Client *c = NULL;
      for (c = selmon->clients; c; c = c->next) {
        if (ISVISIBLE(c))
          showwin(c, 0);
      }
    }
    selmon->isoverview ^= 1;
    if (ISTAG(target)) {
      arrange(selmon);
    }
    view(&(Arg){ .ui = target });
    focus(selmon->sel);
}

void
unfocus(Client *c, int setfocus)
{
  if (!c)
    return;

  if (c->hid && !HIDDEN(c)) {
    hidewin(c);
    arrange(c->mon);
  }

  grabbuttons(c, 0);
  XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
  if (setfocus) {
    XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
  }
}

void
togglewin(const Arg *arg)
{
	Client *c = (Client*)arg->v;
  if (!c) {
    c = selmon->sel;
  }
  if (!c) {
    return;
  }
	if (c == selmon->sel) { // 当前窗口就是选中窗口
    if (c->hid) {
			showwin(c, 1);
    } else {
      // 尝试找当前可见的不隐藏fc
      Client *fc;
      for (fc = selmon->clients; fc && (!ISVISIBLE(fc) || HIDDEN(fc) || fc == c); fc = fc->snext);
      if (fc) {
        // fc存在则隐藏当前窗口然后切到fc
        hidewin(c);
        focus(fc);
      } else {
        // fc不存在，说明找不到一个合适的窗口，那么还是显示当前窗口，仅设置hid标识
        c->hid = 1;
      }
    }
	} else { // 当前窗口不是选中窗口
		if (c->hid) {
			showwin(c, 1);
      focus(c);
    } else {
      hidewin(c);
    }
	}
  arrange(c->mon);
}

void
unmanage(Client *c, int destroyed)
{
  Monitor *m = c->mon;
  XWindowChanges wc;

  Client *sc;
  if (findscratch(&sc) && sc == c) {
    // 去除scratchtag标志位
    selmon->tagset[selmon->seltags] &= ~scratchtag;
    selmon->tagset[selmon->seltags ^ 1] &= ~scratchtag;
  }

  detach(c);
  detachstack(c);
  if (!destroyed) {
    wc.border_width = c->oldbw;
    XGrabServer(dpy); /* avoid race conditions */
    XSetErrorHandler(xerrordummy);
    XSelectInput(dpy, c->win, NoEventMask);
    XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
    XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
    setclientstate(c, WithdrawnState);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }
  free(c);
  focus(NULL);
  updateclientlist();
  arrange(m);
}

void
unmapnotify(XEvent *e)
{
  Client *c;
  XUnmapEvent *ev = &e->xunmap;

  if ((c = wintoclient(ev->window))) {
    if (ev->send_event)
      setclientstate(c, WithdrawnState);
    else
      unmanage(c, 0);
  }
  else if ((c = wintosystrayicon(ev->window))) {
    /* KLUDGE! sometimes icons occasionally unmap their windows, but do
     * _not_ destroy them. We map those windows back */
    XMapRaised(dpy, c->win);
    updatesystray();
  }
}

void
updatebars(void)
{
  unsigned int w;
  Monitor *m;
  XSetWindowAttributes wa = {
    .override_redirect = True,
    .background_pixel = 0,
    .border_pixel = 0,
    .colormap = cmap,
    .event_mask = ButtonPressMask|ExposureMask
  };
  XClassHint ch = {"dwm", "dwm"};
  for (m = mons; m; m = m->next) {
    if (m->barwin)
      continue;
    w = m->ww;
    if (showsystray && m == systraytomon(m))
      w -= getsystraywidth();
    m->barwin = XCreateWindow(dpy, root, m->wx, m->by, m->ww, bh, 0, depth,
                              InputOutput, visual,
                              CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWColormap|CWEventMask, &wa);
    XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
    if (showsystray && m == systraytomon(m))
      XMapRaised(dpy, systray->win);
    XMapRaised(dpy, m->barwin);
    XSetClassHint(dpy, m->barwin, &ch);
  }
}

void
updatebarpos(Monitor *m)
{
  m->wy = m->my;
  m->wh = m->mh;
  if (m->showbar) {
    m->wh -= bh;
    m->by = m->topbar ? m->wy : m->wy + m->wh;
    m->wy = m->topbar ? m->wy + bh : m->wy;
  } else 
    m->by = m->topbar ? -bh : (m->wh + bh);
}

void
updateclientlist()
{
  Client *c;
  Monitor *m;

  XDeleteProperty(dpy, root, netatom[NetClientList]);
  for (m = mons; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      XChangeProperty(dpy, root, netatom[NetClientList],
        XA_WINDOW, 32, PropModeAppend,
        (unsigned char *) &(c->win), 1);
}

int
updategeom(void)
{
  int dirty = 0;

#ifdef XINERAMA
  if (XineramaIsActive(dpy)) {
    int i, j, n, nn;
    Client *c;
    Monitor *m;
    XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
    XineramaScreenInfo *unique = NULL;

    for (n = 0, m = mons; m; m = m->next, n++);
    /* only consider unique geometries as separate screens */
    unique = ecalloc(nn, sizeof(XineramaScreenInfo));
    for (i = 0, j = 0; i < nn; i++)
      if (isuniquegeom(unique, j, &info[i]))
        memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
    XFree(info);
    nn = j;

    /* new monitors if nn > n */
    for (i = n; i < nn; i++) {
      for (m = mons; m && m->next; m = m->next);
      if (m)
        m->next = createmon();
      else
        mons = createmon();
    }
    for (i = 0, m = mons; i < nn && m; m = m->next, i++)
      if (i >= n
      || unique[i].x_org != m->mx || unique[i].y_org != m->my
      || unique[i].width != m->mw || unique[i].height != m->mh)
      {
        dirty = 1;
        m->num = i;
        m->mx = m->wx = unique[i].x_org;
        m->my = m->wy = unique[i].y_org;
        m->mw = m->ww = unique[i].width;
        m->mh = m->wh = unique[i].height;
        updatebarpos(m);
      }
    /* removed monitors if n > nn */
    for (i = nn; i < n; i++) {
      for (m = mons; m && m->next; m = m->next);
      while ((c = m->clients)) {
        dirty = 1;
        m->clients = c->next;
        detachstack(c);
        c->mon = mons;
        attach(c);
        attachstack(c);
      }
      if (m == selmon)
        setselmon(mons);
      cleanupmon(m);
    }
    free(unique);
  } else
#endif /* XINERAMA */
  { /* default monitor setup */
    if (!mons)
      mons = createmon();
    if (mons->mw != sw || mons->mh != sh) {
      dirty = 1;
      mons->mw = mons->ww = sw;
      mons->mh = mons->wh = sh;
      updatebarpos(mons);
    }
  }
  if (dirty) {
    setselmon(mons);
    setselmon(wintomon(root));
  }
  return dirty;
}

void
updatenumlockmask(void)
{
  unsigned int i, j;
  XModifierKeymap *modmap;

  numlockmask = 0;
  modmap = XGetModifierMapping(dpy);
  for (i = 0; i < 8; i++)
    for (j = 0; j < modmap->max_keypermod; j++)
      if (modmap->modifiermap[i * modmap->max_keypermod + j]
        == XKeysymToKeycode(dpy, XK_Num_Lock))
        numlockmask = (1 << i);
  XFreeModifiermap(modmap);
}

void
updatesizehints(Client *c)
{
  long msize;
  XSizeHints size;

  if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
    /* size is uninitialized, ensure that size.flags aren't used */
    size.flags = PSize;
  if (size.flags & PBaseSize) {
    c->basew = size.base_width;
    c->baseh = size.base_height;
  } else if (size.flags & PMinSize) {
    c->basew = size.min_width;
    c->baseh = size.min_height;
  } else
    c->basew = c->baseh = 0;
  if (size.flags & PResizeInc) {
    c->incw = size.width_inc;
    c->inch = size.height_inc;
  } else
    c->incw = c->inch = 0;
  if (size.flags & PMaxSize) {
    c->maxw = size.max_width;
    c->maxh = size.max_height;
  } else
    c->maxw = c->maxh = 0;
  if (size.flags & PMinSize) {
    c->minw = size.min_width;
    c->minh = size.min_height;
  } else if (size.flags & PBaseSize) {
    c->minw = size.base_width;
    c->minh = size.base_height;
  } else
    c->minw = c->minh = 0;
  if (size.flags & PAspect) {
    c->mina = (float)size.min_aspect.y / size.min_aspect.x;
    c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
  } else
    c->maxa = c->mina = 0.0;
  c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
  c->hintsvalid = 1;
}

void
updatestatus(void)
{
  if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
    strcpy(stext, "dwm-"VERSION);
  drawbar(selmon);
  updatesystray();
}

void
updatesystrayicongeom(Client *i, int w, int h)
{
  if (i) {
    i->h = bh;
    if (w == h)
      i->w = bh;
    else if (h == bh)
      i->w = w;
    else
      i->w = (int) ((float)bh * ((float)w / (float)h));
    applysizehints(i, &(i->x), &(i->y), &(i->w), &(i->h), False);
    /* force icons into the systray dimenons if they don't want to */
    if (i->h > bh) {
      if (i->w == i->h)
        i->w = bh;
      else
        i->w = (int) ((float)bh * ((float)i->w / (float)i->h));
      i->h = bh;
    }
  }
}

void
updatesystrayiconstate(Client *i, XPropertyEvent *ev)
{
  long flags;
  int code = 0;

  if (!showsystray || !i || ev->atom != xatom[XembedInfo] ||
      !(flags = getatomprop(i, xatom[XembedInfo])))
    return;

  if (flags & XEMBED_MAPPED && !i->tags) {
    i->tags = 1;
    code = XEMBED_WINDOW_ACTIVATE;
    XMapRaised(dpy, i->win);
    setclientstate(i, NormalState);
  }
  else if (!(flags & XEMBED_MAPPED) && i->tags) {
    i->tags = 0;
    code = XEMBED_WINDOW_DEACTIVATE;
    XUnmapWindow(dpy, i->win);
    setclientstate(i, WithdrawnState);
  }
  else
    return;
  sendevent(i->win, xatom[Xembed], StructureNotifyMask, CurrentTime, code, 0,
      systray->win, XEMBED_EMBEDDED_VERSION);
}

void
updatesystray(void)
{
  XSetWindowAttributes wa;
  XWindowChanges wc;
  Client *i;
  Monitor *m = systraytomon(NULL);
  unsigned int x = m->mx + m->mw;
  unsigned int w = 1;

  if (!showsystray)
    return;
  if (!systray) {
    /* init systray */
    if (!(systray = (Systray *)calloc(1, sizeof(Systray))))
      die("fatal: could not malloc() %u bytes\n", sizeof(Systray));
    systray->win = XCreateSimpleWindow(dpy, root, x, m->by, w, bh, 0, 0, scheme[SchemeSel][ColBg].pixel);
    wa.event_mask        = ButtonPressMask | ExposureMask;
    wa.override_redirect = True;
    wa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
    XSelectInput(dpy, systray->win, SubstructureNotifyMask);
    XChangeProperty(dpy, systray->win, netatom[NetSystemTrayOrientation], XA_CARDINAL, 32,
        PropModeReplace, (unsigned char *)&netatom[NetSystemTrayOrientationHorz], 1);
    XChangeWindowAttributes(dpy, systray->win, CWEventMask|CWOverrideRedirect|CWBackPixel, &wa);
    XMapRaised(dpy, systray->win);
    XSetSelectionOwner(dpy, netatom[NetSystemTray], systray->win, CurrentTime);
    if (XGetSelectionOwner(dpy, netatom[NetSystemTray]) == systray->win) {
      sendevent(root, xatom[Manager], StructureNotifyMask, CurrentTime, netatom[NetSystemTray], systray->win, 0, 0);
      XSync(dpy, False);
    }
    else {
      fprintf(stderr, "dwm: unable to obtain system tray.\n");
      free(systray);
      systray = NULL;
      return;
    }
  }
  for (w = 0, i = systray->icons; i; i = i->next) {
    /* make sure the background color stays the same */
    wa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
    XChangeWindowAttributes(dpy, i->win, CWBackPixel, &wa);
    XMapRaised(dpy, i->win);
    w += systrayspacing;
    i->x = w;
    XMoveResizeWindow(dpy, i->win, i->x, 0, i->w, i->h);
    w += i->w;
    if (i->mon != m)
      i->mon = m;
  }
  w = w ? w + systrayspacing : 1;
  x -= w;
  XMoveResizeWindow(dpy, systray->win, x, m->by, w, bh);
  wc.x = x; wc.y = m->by; wc.width = w; wc.height = bh;
  wc.stack_mode = Above; wc.sibling = m->barwin;
  XConfigureWindow(dpy, systray->win, CWX|CWY|CWWidth|CWHeight|CWSibling|CWStackMode, &wc);
  XMapWindow(dpy, systray->win);
  XMapSubwindows(dpy, systray->win);
  /* redraw background */
  XSetForeground(dpy, drw->gc, scheme[SchemeNorm][ColBg].pixel);
  XFillRectangle(dpy, systray->win, XCreateGC(dpy, root, 0 , NULL), 0, 0, w, bh);
  XSync(dpy, False);
}

void
updatetitle(Client *c)
{
  if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
    gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
  if (c->name[0] == '\0') /* hack to mark broken clients */
    strcpy(c->name, broken);
}

void
updatewindowtype(Client *c)
{
  Atom state = getatomprop(c, netatom[NetWMState]);
  Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

  if (state == netatom[NetWMFullscreen])
    setfullscreen(c, 1);
  if (wtype == netatom[NetWMWindowTypeDialog])
    c->isfloating = 1;
}

void
updatewmhints(Client *c)
{
  XWMHints *wmh;

  if ((wmh = XGetWMHints(dpy, c->win))) {
    if (c == selmon->sel && wmh->flags & XUrgencyHint) {
      wmh->flags &= ~XUrgencyHint;
      XSetWMHints(dpy, c->win, wmh);
    } else
      c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
    if (wmh->flags & InputHint)
      c->neverfocus = !wmh->input;
    else
      c->neverfocus = 0;
    XFree(wmh);
  }
}

void viewto(unsigned int movebit(unsigned int)) {
  Monitor *mon = selmon;
  // 不处理overview模式
  if (mon->isoverview) {
    return;
  }
  unsigned int seltags = mon->tagset[mon->seltags] & TAGMASK;
  // 如果当前不只有一个选中tag则不进行处理
  if (__builtin_popcount(seltags) != 1) {
    return;
  }

  unsigned int nextSeltags = movebit(seltags) & TAGMASK;
  while (nextSeltags) {
    int hasVisiable = 0;
    for (Client *c = mon->clients; c; c = c->next) {
      if (c->tags & nextSeltags) {
        hasVisiable = 1;
        break;
      }
    }
    if (hasVisiable) {
      view(&(Arg) { .ui = nextSeltags });
      break;
    }
    nextSeltags = movebit(nextSeltags) & TAGMASK;
  }
}

unsigned int tagmoveleft(unsigned int tag) {
  return tag >> 1;
}

void viewtoleft(const Arg *arg) {
  viewto(tagmoveleft);
}

unsigned int tagmoveright(unsigned int tag) {
  return tag << 1;
}

void viewtoright(const Arg *arg) {
  viewto(tagmoveright);
}

void
view(const Arg *arg)
{
  int i;
  unsigned int tmptag;

  if (ISTAG(arg->ui)) {
    return;
  }
  // int prevtag = selmon->tagset[selmon->seltags];
  selmon->seltags ^= 1; /* toggle sel tagset */
  if (arg->ui & TAGMASK) {
    // 保留TAGMASK之外的位以支持scratch
    // selmon->tagset[selmon->seltags] = (prevtag & scratchtag) | (arg->ui & TAGMASK);
    selmon->tagset[selmon->seltags] = arg->ui & TAGMASK; // 这里不抹除前一个tagset，这样可以使得switch回去后保留scratchtag
    selmon->pertag->prevtag = selmon->pertag->curtag;

    if (arg->ui == ~0)
      selmon->pertag->curtag = 0;
    else {
      for (i = 0; !(arg->ui & 1 << i); i++) ;
      selmon->pertag->curtag = i + 1;
    }
  } else {
    tmptag = selmon->pertag->prevtag;
    selmon->pertag->prevtag = selmon->pertag->curtag;
    selmon->pertag->curtag = tmptag;
  }

  selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
  selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
  selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
  selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
  selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];

  if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
    togglebar(NULL);

  focus(NULL);
  arrange(selmon);
}

void
listwindowpids(Window w, Window pids[], int len) {
  pids[0] = w;
  for (int i = 1; i < len; i++) {
    pids[i] = 0L;
  }
  unsigned int num;
  Window root, parent, *children = NULL;
  for (int i = 1; i < len; i++) {
    if (!XQueryTree(dpy, w, &root, &parent, &children, &num)) {
      break;
    }
    if (children) {
      XFree(children);
    }
    pids[i] = parent;
    if (w == root) {
      break;
    }
    w = parent;
  }
}

int
inwindowpids(Window w, Window pids[], int len) {
  if (w && pids) {
    for (int i = 0; i < len; i++) {
      if (w == pids[i]) {
        return 1;
      }
    }
  }
  return 0;
}

int
isdialog(Client *c) {
  return c && getatomprop(c, netatom[NetWMWindowType]) == netatom[NetWMWindowTypeDialog];
}

int
isprevclient(int switchmode, Client *src, Client *prev) {
  const Layout *lt;
  switch (switchmode) {
  case SWITCH_WIN:
    return 1;
  case SWITCH_SAME_TAG:
    return selmon && selmon == prev->mon && ISVISIBLE(prev);
  case SWITCH_DIFF_TAG:
    return (selmon && selmon != prev->mon) || !ISVISIBLE(prev);
  case SWITCH_SMART:
    if (!src->isfullscreen && (lt = src->mon->lt[src->mon->sellt]) && lt->arrange == monocle) {
      return isprevclient(SWITCH_WIN, src, prev);
    } else {
      return isprevclient(SWITCH_DIFF_TAG, src, prev);
    }
  default:
    return 0;
  }
}

void
setselmon(Monitor *newselmon) {
  selmon = newselmon;
  if (newselmon != selmon) {
    // 记录selmon状态
    char selmonnumstr[10];
    sprintf(selmonnumstr, "%d", newselmon->num);
    char *cmd[] = { "dwm-status-record", selmonnumstr, "selmon", NULL };
    Arg arg = { .v = cmd };
    spawn(&arg);
  }
}

void
setmonsel(Monitor *m, Client *c) {
  if (m) {
    m->sel = c;
    if (m == selmon && m->sel != c) {
      char selwinnum[10];
      sprintf(selwinnum, "%lu", c->win);
      char *cmd[] = { "dwm-status-record", selwinnum, "selwin", NULL };
      Arg arg = { .v = cmd };
      spawn(&arg);
    }
  }
}

void
switchprevclient(const Arg *arg) {

  if (!selmon || !selmon->accstack) {
    return;
  }

  unsigned int switchmode = arg->ui;

  // 如果当前窗口是dialog，找到非dialog的第2个窗口，如果没有第2个则取第1个
  // 如果当前窗口非dialog，找到第1个窗口
  Client *selc = selmon && selmon->sel ? selmon->sel : NULL;
  // if (isdialog(selc)) {
  //   ClientAccNode *f1 = selmon->accstack;
  //   while (f1 && (f1->c == selc || isdialog(f1->c))) {
  //     f1 = f1->next;
  //   }
  //   ClientAccNode *f2 = f1 ? f1->next : NULL;
  //   while (f2 && (f2->c == selc || !isprevclient(switchmode, selmon->accstack->c, f2->c))) {
  //     f2 = f2->next;
  //   }
  //   if (f2) {
  //     switchclient(f2->c);
  //   } else if (f1) {
  //     switchclient(f1->c);
  //   } else if (switchmode != SWITCH_WIN) {
  //     switchprevclient(&(Arg){.ui = SWITCH_WIN});
  //   }
  // } else {
    ClientAccNode *f = selmon->accstack;
    while (f && (f->c == selc || !isprevclient(switchmode, selmon->accstack->c, f->c))) {
      f = f->next;
    }
    if (f) {
      // 如果当前选中窗口是不可见的，那么还是回到当前选中窗口，否则会不符合预期，比如tag1打开窗口，tag2再打开窗口，来到tag3切换时应当希望回到tag2，而不是tag1
      Client *prevc = f->c;
      if (!ISVISIBLE(selc)) {
        prevc = selc;
      }
      switchclient(prevc);
    } else if (switchmode != SWITCH_WIN) {
      switchprevclient(&(Arg){.ui = SWITCH_WIN});
    }
  // }
}

// 切换到指定client
void
switchclient(Client *c) {
  if (!c) {
    return;
  }
  // 如果当前monitor并非client所在的monitor，跳到选择的监视器
  if (c->mon != selmon) {
    focusmonbyclient(c);
  }
  // 如果当前tag下不可见，跳转到相应的tag上
  if (!ISVISIBLE(c)) {
    view(&(Arg) { .ui = c->tags });
  }
  // 展示选中的隐藏窗口
  // TODO 移动到focus中先观察
  // if (HIDDEN(c)) {
  //   showwin(c, 0);
  // }
  // 聚焦并重置栈
  if (selmon->sel != c) { // TODO 这个判断是为了避免一些场景下focus自身会打断应用行为，另外也可以减少性能消耗，暂时先加上观察是否会有其它问题
    focus(c);
    restack(selmon); // TODO 理论上如果无需focus那么在上一次的focus已经将堆栈排好了也不用重新restack，也先观察
  }
  // 将选择的窗口置顶
  // XRaiseWindow(dpy, c->win);
  // 将选中窗口推到主工作区
  // pop(c);
  // 定位光标到相应client
  // if (selmon && selmon->sel && selmon->sel != c) {
  //   pointerfocuswin(c);
  // }
}

void
addaccstack(Client *c) {
  if (selmon && c) {
    removeaccstack(c);

    ClientAccNode *h = ecalloc(1, sizeof(ClientAccNode));
    h->c = c;
    h->next = selmon->accstack;
    selmon->accstack = h;
  }
}

void
removeaccstack(Client *c) {
  for (Monitor *m = mons; m; m = m->next) {
    ClientAccNode **cur = &m->accstack;
    while (*cur && (*cur)->c != c) {
      cur = &(*cur)->next;
    }

    if (*cur) {
      ClientAccNode *curfree = *cur;
      if ((*cur)->next) {
        *cur = (*cur)->next;
      } else {
        *cur = NULL;
      }
      free(curfree);
    }
  }
}

void
switchenternotify(const Arg *arg){
  if (arg->ui) {
    enableenternotify = 1;
  } else {
    enableenternotify = 0;
  }
}

Client *
wintoclient(Window w)
{
  Client *c;
  Monitor *m;

  for (m = mons; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      if (c->win == w)
        return c;
  return NULL;
}

Client *
wintosystrayicon(Window w) {
  Client *i = NULL;

  if (!showsystray || !w)
    return i;
  for (i = systray->icons; i && i->win != w; i = i->next) ;
  return i;
}

Monitor *
wintomon(Window w)
{
  int x, y;
  Client *c;
  Monitor *m;

  if (w == root && getrootptr(&x, &y))
    return recttomon(x, y, 1, 1);
  for (m = mons; m; m = m->next)
    if (w == m->barwin)
      return m;
  if ((c = wintoclient(w)))
    return c->mon;
  return selmon;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int
xerror(Display *dpy, XErrorEvent *ee)
{
  if (ee->error_code == BadWindow
  || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
  || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
  || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
  || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
  || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
  || (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
  || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
  || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
    return 0;
  fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
    ee->request_code, ee->error_code);
  return xerrorxlib(dpy, ee); /* may call exit */
}

int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
  return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *ee)
{
  die("dwm: another window manager is already running");
  return -1;
}

Monitor *
systraytomon(Monitor *m) {
  Monitor *t;
  int i, n;
  if(!systraypinning) {
    if(!m)
      return selmon;
    return m == selmon ? m : NULL;
  }
  for(n = 1, t = mons; t && t->next; n++, t = t->next) ;
  for(i = 1, t = mons; t && t->next && i < systraypinning; i++, t = t->next) ;
  if(systraypinningfailfirst && n < systraypinning)
    return mons;
  return t;
}

void
xinitvisual()
{
  XVisualInfo *infos;
  XRenderPictFormat *fmt;
  int nitems;
  int i;

  XVisualInfo tpl = {
    .screen = screen,
    .depth = 32,
    .class = TrueColor
  };
  long masks = VisualScreenMask | VisualDepthMask | VisualClassMask;

  infos = XGetVisualInfo(dpy, masks, &tpl, &nitems);
  visual = NULL;
  for(i = 0; i < nitems; i ++) {
    fmt = XRenderFindVisualFormat(dpy, infos[i].visual);
    if (fmt->type == PictTypeDirect && fmt->direct.alphaMask) {
      visual = infos[i].visual;
      depth = infos[i].depth;
      cmap = XCreateColormap(dpy, root, visual, AllocNone);
      useargb = 1;
      break;
    }
  }

  XFree(infos);

  if (! visual) {
    visual = DefaultVisual(dpy, screen);
    depth = DefaultDepth(dpy, screen);
    cmap = DefaultColormap(dpy, screen);
  }
}

void
zoom(const Arg *arg)
{
  Client *c = selmon->sel;

  // 选中监视器当前无布局，或者当前客户端是浮动的，则不执行zoom
  if (!selmon->lt[selmon->sellt]->arrange || !c || c->isfloating)
    return;
  // c是选中监视器的首个客户端，并且没有第二个客户端则返回
  if (c == nexttiled(selmon->clients) && !(c = nexttiled(c->next)))
    return;
  pop(c);
}

int
inarea(int x, int y, int rx, int ry, int rw, int rh) {
  return x > rx && x < rx + rw && y > ry && y < ry + rh;
}



void
movewin(const Arg *arg)
{
    Client *c;
    int x, y, nx, ny;
    int px, py;

    c = selmon->sel;
    if (!c || c->isfullscreen)
        return;
    if (!c->isfloating)
        togglefloating(NULL);
    x = nx = c->x; // x, next x
    y = ny = c->y; // y, next y
    int gap, tctx, tcty, tx, ty;
    switch (arg->ui) {
        case WIN_UP:
            ny -= c->mon->wh / movewinthresholdv;
            // 窗口吸附
            gap = fgappih;
            for (Client *tc = c->mon->clients; tc; tc = tc->next) {
              if (c->y > tc->y + HEIGHT(tc) + gap && tc->y + HEIGHT(tc) + gap > ny) {
                ny = tc->y + HEIGHT(tc) + gap;
              } else if (c->y + HEIGHT(c) > tc->y - gap && tc->y - gap > ny + HEIGHT(c)) {
                ny = tc->y - gap - HEIGHT(c);
              }
            }
            // 边缘吸附
            gap = fgappoh;
            if (c->y + HEIGHT(c) > c->mon->wy + c->mon->wh - gap && c->mon->wy + c->mon->wh - gap > ny + HEIGHT(c)) {
              ny = c->mon->wy + c->mon->wh - gap - HEIGHT(c);
            } else if (c->y > c->mon->wy + gap && c->mon->wy + gap > ny) {
              ny = c->mon->wy + gap;
            }
            // 限制出窗
            if (ny < c->mon->wy - HEIGHT(c))
              ny = MAX(ny, c->mon->wy - HEIGHT(c) + gap + borderpx);
            break;
        case WIN_DOWN:
            ny += c->mon->wh / movewinthresholdv;
            // 窗口吸附
            gap = fgappih;
            for (Client *tc = c->mon->clients; tc; tc = tc->next) {
              if (tc != c && ISVISIBLE(tc) && !HIDDEN(tc) && tc->isfloating && !tc->isfullscreen) {
                if (c->y + HEIGHT(c) < tc->y - gap && tc->y - gap < ny + HEIGHT(c)) {
                  ny = tc->y - gap - HEIGHT(c);
                } else if (c->y < tc->y + HEIGHT(tc) + gap && tc->y + HEIGHT(tc) + gap < ny) {
                  ny = tc->y + HEIGHT(tc) + gap;
                }
              }
            }
            // 边缘吸附
            gap = fgappoh;
            if (c->y < c->mon->wy + gap && c->mon->wy + gap < ny) {
             ny = c->mon->wy + gap;
            } else if (c->y + HEIGHT(c) < c->mon->wy + c->mon->wh - gap && c->mon->wy + c->mon->wh - gap < ny + HEIGHT(c)) {
             ny = c->mon->wy + c->mon->wh - gap - HEIGHT(c);
            }
            // 限制出窗
            if (ny > c->mon->wy + c->mon->wh - gap)
              ny = c->mon->wy + c->mon->wh - gap;
            break;
        case WIN_LEFT:
            nx -= c->mon->ww / movewinthresholdh;
            // 窗口吸附
            gap = fgappiv;
            for (Client *tc = c->mon->clients; tc; tc = tc->next) {
              if (tc != c && ISVISIBLE(tc) && !HIDDEN(tc) && tc->isfloating && !tc->isfullscreen) {
                if (c->x > tc->x + WIDTH(tc) + gap && tc->x + WIDTH(tc) + gap > nx) {
                  nx = tc->x + WIDTH(tc) + gap;
                } else if (c->x + WIDTH(c) > tc->x - gap && tc->x - gap > nx + WIDTH(c)) {
                  nx = tc->x - gap - WIDTH(c);
                }
              }
            }
            // 边缘吸附
            gap = fgappov;
            if (c->x + WIDTH(c) > c->mon->wx + c->mon->ww - gap && c->mon->wx + c->mon->ww - gap > nx + WIDTH(c)) {
              nx = c->mon->wx + c->mon->ww - gap - WIDTH(c);
            } else if (c->x > c->mon->wx + gap && c->mon->wx + gap > nx) {
              nx = c->mon->wx + gap;
            }
            // 限制出窗
            if (nx < c->mon->wx - WIDTH(c))
              nx = c->mon->wx - WIDTH(c) + gap + borderpx;
            break;
        case WIN_RIGHT:
            nx += c->mon->ww / movewinthresholdh;
            // 窗口吸附
            gap = fgappiv;
            for (Client *tc = c->mon->clients; tc; tc = tc->next) {
              if (tc != c && ISVISIBLE(tc) && !HIDDEN(tc) && tc->isfloating && !tc->isfullscreen) {
                if (c->x + WIDTH(c) < tc->x - gap && tc->x - gap < nx + WIDTH(c)) {
                  nx = tc->x - gap - WIDTH(c);
                } else if (c->x < tc->x + WIDTH(tc) + gap && tc->x + WIDTH(tc) + gap < nx) {
                  nx = tc->x + WIDTH(tc) + gap;
                }
              }
            }
            // 边缘吸附
            gap = fgappov;
            if (c->x < c->mon->wx + gap && c->mon->wx + gap < nx) {
              nx = c->mon->wx + gap;
            } else if (c->x + WIDTH(c) < c->mon->wx + c->mon->ww - gap && c->mon->wx + c->mon->ww - gap < nx + WIDTH(c)) {
              nx = c->mon->wx + c->mon->ww - gap - WIDTH(c);
            }
            // 限制出窗
            if (nx > c->mon->wx + c->mon->ww - gap)
              nx = c->mon->wx + c->mon->ww - gap;
            break;
    }

    resize(c, nx, ny, c->w, c->h, 1);
    getrootptr(&px, &py);
    if (inarea(px, py, x, y, c->w, c->h)) {
      XWarpPointer(dpy, None, root, 0, 0, 0, 0, nx - x + px, ny - y + py);
    }
}

void
resizewin(const Arg *arg)
{
    Client *c;
    int w, h, nw, nh;
    int px, py;

    c = selmon->sel;
    if (!c || c->isfullscreen)
        return;
    if (!c->isfloating)
        togglefloating(NULL);
    w = nw = c->w;
    h = nh = c->h;
    switch (arg->ui) {
        case H_EXPAND:
            nw += selmon->wh / resizewinthresholdv;
            break;
        case H_REDUCE:
            nw -= selmon->wh / resizewinthresholdv;
            break;
        case V_EXPAND:
            nh += selmon->ww / resizewinthresholdh;
            break;
        case V_REDUCE:
            nh -= selmon->ww / resizewinthresholdh;
            break;
    }
    nw = MAX(nw, selmon->ww / resizewinthresholdv);
    nh = MAX(nh, selmon->wh / resizewinthresholdh);
    if (c->x + nw + 2 * c->bw > selmon->wx + selmon->ww)
        nw = selmon->wx + selmon->ww - c->x - 2 * c->bw;
    if (c->y + nh + 2 * c->bw > selmon->wy + selmon->wh)
        nh = selmon->wy + selmon->wh - c->y - 2 * c->bw;
    resize(c, c->x, c->y, nw, nh, 1);
    getrootptr(&px, &py);
    if (inarea(px, py, c->x, c->y, w, h)) {
      px = MAX(px, c->x + 1);
      px = MIN(px, c->x + nw - 1);
      py = MAX(py, c->y + 1);
      py = MIN(py, c->y + nh - 1);
      XWarpPointer(dpy, None, root, 0, 0, 0, 0, px, py);
    }
}

void
mousefocus(const Arg *arg) {
  if (selmon && selmon->sel) {
    Client *c = selmon->sel;
    XWarpPointer(dpy, None, root, 0, 0, 0, 0, c->x + c->w / 2, c->y + c->h / 2);
  }
}

void
mousemove(const Arg *arg) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  long long curms = (long long) tv.tv_sec * 1000 + tv.tv_usec/ 1000;
  if (curms - prevmousemove < 100) {
    if (beginmousemove == 0) {
      beginmousemove = curms;
    }
  } else {
    beginmousemove = 0;
  }
  prevmousemove = curms;

  if (arg) {
    // v是鼠标移动的速度
    // k是基础速度
    // t是按压时间
    // e是自然对数的底数，约等于2.71828
    double base = 15;
    double t = beginmousemove == 0 ? 0 : (curms - beginmousemove);
    double delta = 400;
    double deltams = 1000 * 2;
    double v = base + delta * tanh(t / deltams);
    int step = ceil(v);

    int x, y;
    getrootptr(&x, &y);
    int dir = arg->ui % 4;
    if (dir == MOUSE_UP) {
      y -= step;
    } else if (dir == MOUSE_RIGHT) {
      x += step;
    } else if (dir == MOUSE_DOWM) {
      y += step;
    } else {
      x -= step;
    }
    XWarpPointer(dpy, None, root, 0, 0, 0, 0, x, y);
  }
}

int
main(int argc, char *argv[])
{
  if (argc == 2 && !strcmp("-v", argv[1]))
    die("dwm-"VERSION);
  else if (argc != 1)
    die("usage: dwm [-v]");
  if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
    fputs("warning: no locale support\n", stderr);
  if (!(dpy = XOpenDisplay(NULL)))
    die("dwm: cannot open display");
  checkotherwm();
  setup();
#ifdef __OpenBSD__
  if (pledge("stdio rpath proc exec", NULL) == -1)
    die("pledge");
#endif /* __OpenBSD__ */
  scan();
  runautosh(autostartblocksh, autostartsh);
  run();
  cleanup();
  XCloseDisplay(dpy);
  runautosh(autostopblocksh, autostopsh);
  return EXIT_SUCCESS;
}
