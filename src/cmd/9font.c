/*
 * 9font - choose a font from a list, each shown in its own font,
 * and print its name.
 * nemo
 */
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <keyboard.h>
#include <bio.h>

enum
{
	Margin = 4,
	Gap = 4,
	Maxopen = 64,	// fonts kept open
	Maxvis = 256,	// rows on the screen
};

typedef struct Item Item;
struct Item
{
	char	*name;	// font file name, printed when chosen
	char	*label;	// shown, in the font itself
	Font	*f;
	int	bad;	// can't open it: shown in the default font
};

static Item	*items;
static int	nitems;
static int	*shown;		// items matching the filter
static int	nshown;
static int	sel;		// index in shown
static int	top;		// first shown row on the screen
static int	vis[Maxvis];	// shown rows on the screen
static Rectangle visr[Maxvis];
static int	nvis;
static int	nopen;
static int	rowh;		// height of a row
static char	filter[128];
static int	tflag;
static Image	*back, *high, *bord, *text, *htext;

static void
usage(void)
{
	fprint(2, "usage: 9font [-t] [-s size] [-f font] [-l label] [-W winsize] [fontfile ...]\n");
	exits("usage");
}

static void
additem(char *name, char *label)
{
	if(nitems%64 == 0){
		items = realloc(items, (nitems+64)*sizeof items[0]);
		if(items == nil)
			sysfatal("realloc: %r");
	}
	memset(&items[nitems], 0, sizeof items[0]);
	items[nitems].name = name;
	items[nitems].label = label;
	nitems++;
}

// the fonts known to fontsrv, at the given size
static void
fontsrvitems(char *size)
{
	int p[2], pid;
	Biobuf bin;
	char *l, *n;

	if(pipe(p) < 0)
		sysfatal("pipe: %r");
	switch(pid = fork()){
	case -1:
		sysfatal("fork: %r");
	case 0:
		close(p[0]);
		dup(p[1], 1);
		close(p[1]);
		execlp("fontsrv", "fontsrv", "-p", ".", nil);
		_exits("exec");
	}
	close(p[1]);
	Binit(&bin, p[0], OREAD);
	while((l = Brdstr(&bin, '\n', 1)) != nil){
		n = strchr(l, '/');
		if(n != nil)
			*n = 0;
		if(l[0] == 0){
			free(l);
			continue;
		}
		additem(smprint("/mnt/font/%s/%sa/font", l, size), smprint("%s %s", l, size));
		free(l);
	}
	Bterm(&bin);
	close(p[0]);
	while(waitpid() != pid)
		;
}

static Font*
itemfont(Item *it)
{
	if(it->f != nil)
		return it->f;
	if(it->bad)
		return font;
	it->f = openfont(display, it->name);
	if(it->f == nil){
		it->bad = 1;
		return font;
	}
	nopen++;
	if(it->f->height+Gap > rowh)
		rowh = it->f->height+Gap;
	return it->f;
}

// keep few fonts open: close those not on the screen
static void
closefonts(void)
{
	int i, j;

	if(nopen <= Maxopen)
		return;
	for(i = 0; i < nitems; i++){
		if(items[i].f == nil)
			continue;
		for(j = 0; j < nvis; j++)
			if(vis[j] == i)
				break;
		if(j == nvis){
			freefont(items[i].f);
			items[i].f = nil;
			nopen--;
		}
	}
}

static int
lower(int c)
{
	if(c >= 'A' && c <= 'Z')
		return c-'A'+'a';
	return c;
}

static int
match(char *s)
{
	char *p, *q;

	if(filter[0] == 0)
		return 1;
	for(; *s != 0; s++){
		for(p = s, q = filter; *q != 0 && *p != 0; p++, q++)
			if(lower(*p) != lower(*q))
				break;
		if(*q == 0)
			return 1;
	}
	return 0;
}

static void
dofilter(void)
{
	int i;

	nshown = 0;
	for(i = 0; i < nitems; i++)
		if(match(items[i].label))
			shown[nshown++] = i;
	sel = 0;
	top = 0;
}

static Rectangle
listrect(void)
{
	Rectangle r;

	r = insetrect(screen->r, Margin);
	r.min.y += font->height+Gap+2;
	return r;
}

// place the rows from top on
static void
layout(void)
{
	Rectangle r, lr;
	int i;

	lr = listrect();
	nvis = 0;
	r = lr;
	for(i = top; i < nshown && nvis < Maxvis; i++){
		r.max.y = r.min.y+rowh;
		if(r.max.y > lr.max.y && nvis > 0)
			break;
		vis[nvis] = shown[i];
		visr[nvis] = r;
		nvis++;
		r.min.y = r.max.y;
	}
}

static void
drawrow(int j)
{
	Item *it;
	Image *fg;
	Rectangle r;

	it = &items[vis[j]];
	r = visr[j];
	if(top+j == sel){
		draw(screen, r, high, nil, ZP);
		fg = htext;
	}else{
		draw(screen, r, back, nil, ZP);
		fg = text;
	}
	// until its font is loaded, show the name in the default font
	replclipr(screen, 0, r);
	if(it->f == nil)
		string(screen, Pt(r.min.x+Margin, r.min.y+Gap/2), bord, ZP, font, it->label);
	else
		string(screen, Pt(r.min.x+Margin, r.min.y+Gap/2), fg, ZP, it->f, it->label);
	replclipr(screen, 0, screen->r);
}

static void
redraw(void)
{
	Rectangle r;
	Point p;
	char buf[32];
	int j;

	layout();
	draw(screen, screen->r, back, nil, ZP);
	border(screen, screen->r, 2, bord, ZP);
	r = insetrect(screen->r, Margin);
	p = string(screen, r.min, high, ZP, font, "font: ");
	p = string(screen, p, text, ZP, font, filter);
	draw(screen, Rect(p.x, p.y, p.x+2, p.y+font->height), text, nil, ZP);
	snprint(buf, sizeof buf, "(%d)", nshown);
	string(screen, Pt(p.x+2*stringwidth(font, " "), p.y), bord, ZP, font, buf);
	r.min.y += font->height+Gap/2;
	draw(screen, Rect(r.min.x, r.min.y, r.max.x, r.min.y+1), bord, nil, ZP);
	for(j = 0; j < nvis; j++)
		drawrow(j);
	flushimage(display, 1);
	closefonts();
}

// load the font of one row on the screen; 0 if none is missing
static int
loadone(void)
{
	int j, h;

	for(j = 0; j < nvis; j++)
		if(items[vis[j]].f == nil && !items[vis[j]].bad){
			h = rowh;
			itemfont(&items[vis[j]]);
			if(rowh != h)
				redraw();
			else{
				drawrow(j);
				flushimage(display, 1);
			}
			return 1;
		}
	return 0;
}

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		sysfatal("can't reattach to window: %r");
	redraw();
}

// select row i of shown, scrolling to show it
static void
setsel(int i)
{
	if(nshown == 0)
		return;
	if(i < 0)
		i = 0;
	if(i >= nshown)
		i = nshown-1;
	sel = i;
	if(sel < top)
		top = sel;
	for(;;){
		layout();
		if(sel < top+nvis || top >= sel)
			break;
		top++;
	}
	redraw();
}

static void
scroll(int n)
{
	top += n;
	if(top > nshown-1)
		top = nshown-1;
	if(top < 0)
		top = 0;
	// keep the selection on the screen
	layout();
	if(sel < top)
		sel = top;
	if(sel >= top+nvis)
		sel = top+nvis-1;
	redraw();
}

static int
rowat(Point p)
{
	int j;

	for(j = 0; j < nvis; j++)
		if(ptinrect(p, visr[j]))
			return top+j;
	return -1;
}

static void
choose(int i)
{
	Item *it;

	if(i < 0 || i >= nshown)
		return;
	it = &items[shown[i]];
	print("%s\n", tflag ? it->label : it->name);
	exits(nil);
}

void
main(int argc, char *argv[])
{
	char *label, *fontname, *size;
	Event e;
	Mouse m;
	int i, n, buttons;
	Rune r;

	label = "9font";
	fontname = nil;
	size = "14";
	ARGBEGIN{
	case 's':
		size = EARGF(usage());
		break;
	case 't':
		tflag = 1;
		break;
	case 'f':
		fontname = EARGF(usage());
		break;
	case 'l':
		label = EARGF(usage());
		break;
	case 'W':
		winsize = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	for(i = 0; i < argc; i++)
		additem(argv[i], argv[i]);
	if(argc == 0)
		fontsrvitems(size);
	if(nitems == 0)
		sysfatal("no fonts");
	shown = malloc(nitems*sizeof shown[0]);
	if(shown == nil)
		sysfatal("malloc: %r");
	dofilter();

	if(winsize == nil)
		winsize = "640x480";
	if(initdraw(nil, fontname, label) < 0)
		sysfatal("initdraw: %r");
	back = allocimagemix(display, DPalegreen, DWhite);
	high = allocimage(display, Rect(0,0,1,1), CMAP8, 1, DDarkgreen);
	bord = allocimage(display, Rect(0,0,1,1), CMAP8, 1, DMedgreen);
	text = display->black;
	htext = back;
	if(back == nil || high == nil || bord == nil)
		sysfatal("allocimage: %r");
	rowh = font->height+Gap;
	itemfont(&items[shown[0]]);	// a guess at the row height
	einit(Emouse|Ekeyboard);
	redraw();

	buttons = 0;
	for(;;){
		// fonts are loaded while nothing else happens,
		// so scrolling and typing never wait for them
		while(!ecanread(Emouse|Ekeyboard) && loadone())
			;
		switch(event(&e)){
		case Emouse:
			m = e.mouse;
			if(m.buttons & (8|16)){
				// take all the wheel events pending and
				// scroll once, so none are left when it stops.
				// devdraw on the mac sets other bits with 8 or 16
				// (how much to scroll): they are not buttons.
				n = 0;
				for(;;){
					if(m.buttons & 8)
						n--;
					else
						n++;
					if(!ecanmouse()){
						m.buttons = 0;
						break;
					}
					m = emouse();
					if((m.buttons & (8|16)) == 0)
						break;
				}
				scroll(n);
				if((m.buttons & 7) == 0)
					break;
			}
			if(m.buttons & 7){
				buttons = m.buttons;
				if((i = rowat(m.xy)) >= 0 && i != sel)
					setsel(i);
			}
			if(buttons && (m.buttons & 7) == 0){
				// released: choose what is under the mouse
				buttons = 0;
				choose(rowat(m.xy));
			}
			break;
		case Ekeyboard:
			r = e.kbdc;
			switch(r){
			case Kup:
			case 0x10:	// ^P
				setsel(sel-1);
				break;
			case Kdown:
			case 0x0e:	// ^N
				setsel(sel+1);
				break;
			case Kpgup:
				setsel(sel-(nvis > 1 ? nvis-1 : 1));
				break;
			case Kpgdown:
				setsel(sel+(nvis > 1 ? nvis-1 : 1));
				break;
			case '\n':
			case '\r':
				choose(sel);
				break;
			case Kesc:
			case Kdel:
				exits("cancel");
			case '\b':
				n = strlen(filter);
				if(n > 0){
					filter[n-1] = 0;
					dofilter();
					redraw();
				}
				break;
			case 0x15:	// ^U
				filter[0] = 0;
				dofilter();
				redraw();
				break;
			default:
				// type to filter by name
				n = strlen(filter);
				if(r > ' ' && r < 0x7F && n < sizeof filter-1){
					filter[n] = r;
					filter[n+1] = 0;
					dofilter();
					redraw();
				}
			}
			break;
		}
	}
}
