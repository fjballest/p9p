/*
 * 9menu - show a menu in a window and print the choice.
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
	Margin = 4,	/* space around the items */
	Gap = 2,	/* space between items */
	Maxitems = 512,
	Maxshown = 30,	/* items shown without scrolling */
};

char	*items[Maxitems];
int	nitems;
int	sel = -1;	/* selected item, or -1 */
int	top;		/* first item shown */
int	tflag;
Image	*back, *high, *bord, *text, *htext;

void
usage(void)
{
	fprint(2, "usage: 9menu [-t] [-f font] [-l label] [-W winsize] [item ...]\n");
	exits("usage");
}

void
additem(char *s)
{
	if(nitems >= Maxitems)
		sysfatal("too many items");
	items[nitems++] = s;
}

void
readitems(void)
{
	Biobuf bin;
	char *l;

	Binit(&bin, 0, OREAD);
	while((l = Brdstr(&bin, '\n', 1)) != nil)
		if(l[0] != 0)
			additem(l);
	Bterm(&bin);
}

int
itemht(void)
{
	return font->height+Gap;
}

int
nshown(void)
{
	int n;

	n = (Dy(screen->r)-2*Margin)/itemht();
	if(n < 1)
		n = 1;
	return n;
}

Rectangle
itemrect(int i)
{
	Rectangle r;

	r = insetrect(screen->r, Margin);
	r.min.y += (i-top)*itemht();
	r.max.y = r.min.y+itemht();
	return r;
}

void
drawitem(int i)
{
	Rectangle r;
	Image *fg;
	Point p;

	if(i < top || i >= top+nshown() || i >= nitems)
		return;
	r = itemrect(i);
	if(i == sel){
		draw(screen, r, high, nil, ZP);
		fg = htext;
	}else{
		draw(screen, r, back, nil, ZP);
		fg = text;
	}
	p = Pt((r.min.x+r.max.x-stringwidth(font, items[i]))/2, r.min.y+Gap/2);
	if(p.x < r.min.x)
		p.x = r.min.x;
	string(screen, p, fg, ZP, font, items[i]);
}

void
redraw(void)
{
	int i;

	draw(screen, screen->r, back, nil, ZP);
	border(screen, screen->r, 2, bord, ZP);
	for(i = top; i < nitems && i < top+nshown(); i++)
		drawitem(i);
	flushimage(display, 1);
}

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		sysfatal("can't reattach to window: %r");
	if(top > nitems-nshown())
		top = nitems-nshown();
	if(top < 0)
		top = 0;
	redraw();
}

/* make item i (-1 for none) the selected one, scrolling to show it */
void
setsel(int i)
{
	int o;

	if(i < -1 || i >= nitems || i == sel)
		return;
	o = sel;
	sel = i;
	if(sel >= 0 && (sel < top || sel >= top+nshown())){
		if(sel < top)
			top = sel;
		else
			top = sel-nshown()+1;
		redraw();
		return;
	}
	drawitem(o);
	drawitem(sel);
	flushimage(display, 1);
}

void
scroll(int n)
{
	int t;

	t = top+n;
	if(t > nitems-nshown())
		t = nitems-nshown();
	if(t < 0)
		t = 0;
	if(t != top){
		top = t;
		redraw();
	}
}

int
itemat(Point p)
{
	Rectangle r;
	int i;

	r = insetrect(screen->r, Margin);
	if(!ptinrect(p, r))
		return -1;
	i = top+(p.y-r.min.y)/itemht();
	if(i >= nitems || i >= top+nshown())
		return -1;
	return i;
}

void
choose(int i)
{
	if(i < 0 || i >= nitems)
		return;
	if(tflag)
		print("%s\n", items[i]);
	else
		print("%d\n", i+1);
	exits(nil);
}

int _fontpipe(char*);

/*
 * Guess the height of the font before the window exists,
 * by reading the first line of the font file, so the window
 * can be created at about the right size.
 */
int
fontheight(char *name)
{
	char buf[64], *p;
	int fd, n;

	if(name == nil)
		name = getenv("font");
	if(name == nil)
		return 0;
	if((p = strchr(name, '*')) != nil && p[1] == '/')	/* scale*name */
		name = p+1;
	fd = open(name, OREAD);
	if(fd < 0 && strncmp(name, "/mnt/font/", 10) == 0)
		fd = _fontpipe(name+10);
	if(fd < 0)
		return 0;
	n = read(fd, buf, sizeof buf-1);
	close(fd);
	if(n <= 0)
		return 0;
	buf[n] = 0;
	return atoi(buf);
}

/* set winsize to fit the items, guessing the font size */
void
guesswinsize(char *fontname)
{
	int i, n, h, cw, w;

	h = fontheight(fontname);
	cw = (h+1)/2;	/* a guess at the average width */
	if(h <= 0 || h > 200){
		/* *default* font */
		h = 15;
		cw = 9;
	}
	w = 0;
	for(i = 0; i < nitems; i++)
		if(utflen(items[i]) > w)
			w = utflen(items[i]);
	n = nitems;
	if(n > Maxshown)
		n = Maxshown;
	winsize = smprint("%dx%d", (w+4)*cw + 2*Margin, n*(h+Gap) + 2*Margin);
}

/* fix the window size now that the font is known, if it is off */
void
fitwindow(void)
{
	int i, w, h, n, dx, dy;

	w = 0;
	for(i = 0; i < nitems; i++)
		if(stringwidth(font, items[i]) > w)
			w = stringwidth(font, items[i]);
	w += 2*Margin + 2*stringwidth(font, "0");
	n = nitems;
	if(n > Maxshown)
		n = Maxshown;
	h = 2*Margin + n*itemht();
	dx = Dx(screen->r);
	dy = Dy(screen->r);
	if(dx >= w && dx <= 2*w)
		w = dx;
	if(dy >= h && dy <= h+itemht())
		h = dy;
	if(w != dx || h != dy)
		drawresizewindow(Rect(0, 0, w, h));
}

void
main(int argc, char *argv[])
{
	char *label, *fontname;
	Event e;
	Mouse m;
	int i, buttons, sized;

	label = "9menu";
	fontname = nil;
	ARGBEGIN{
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
		additem(argv[i]);
	if(argc == 0)
		readitems();
	if(nitems == 0)
		sysfatal("no items");

	sized = winsize != nil;
	if(!sized)
		guesswinsize(fontname);
	if(initdraw(nil, fontname, label) < 0)
		sysfatal("initdraw: %r");
	back = allocimagemix(display, DPalegreen, DWhite);
	high = allocimage(display, Rect(0,0,1,1), CMAP8, 1, DDarkgreen);
	bord = allocimage(display, Rect(0,0,1,1), CMAP8, 1, DMedgreen);
	text = display->black;
	htext = back;
	if(back == nil || high == nil || bord == nil)
		sysfatal("allocimage: %r");
	if(!sized)
		fitwindow();
	einit(Emouse|Ekeyboard);
	redraw();

	buttons = 0;
	for(;;){
		switch(event(&e)){
		case Emouse:
			m = e.mouse;
			if(m.buttons & 8){
				scroll(-1);
				break;
			}
			if(m.buttons & 16){
				scroll(1);
				break;
			}
			if(m.buttons & 7)
				buttons = m.buttons;
			setsel(itemat(m.xy));
			if(buttons && (m.buttons & 7) == 0){
				/* released: choose what is under the mouse */
				buttons = 0;
				choose(itemat(m.xy));
			}
			break;
		case Ekeyboard:
			switch(e.kbdc){
			case Kup:
			case 0x10:	/* ^P */
				if(sel < top || sel >= top+nshown())
					setsel(top+nshown()-1 < nitems ? top+nshown()-1 : nitems-1);
				else
					setsel(sel <= 0 ? 0 : sel-1);
				break;
			case Kdown:
			case 0x0e:	/* ^N */
				if(sel < top || sel >= top+nshown())
					setsel(top);
				else
					setsel(sel+1 >= nitems ? nitems-1 : sel+1);
				break;
			case Kpgup:
				scroll(-nshown());
				break;
			case Kpgdown:
				scroll(nshown());
				break;
			case '\n':
			case '\r':
				choose(sel);
				break;
			case Kesc:
			case Kdel:
			case 'q':
				exits("cancel");
			default:
				if(e.kbdc >= '1' && e.kbdc <= '9' && e.kbdc-'1' < nitems)
					choose(e.kbdc-'1');
			}
			break;
		}
	}
}
