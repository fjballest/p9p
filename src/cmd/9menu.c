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
	Margin = 4,	// space around the items
	Gap = 2,	// space between items
	Maxitems = 512,
	Maxshown = 30,	// items shown without scrolling
	Maxval = 256,	// runes in a field
};

static char	*items[Maxitems];
static int	nitems;
static int	sel;		// selected item, or -1
static int	top;		// first item shown
static int	tflag;
static int	iflag;		// a form: items are prompts for text fields
static Rune	*val[Maxitems];	// -i: text of each field
static char	*cmd[Maxitems];	// item!cmd: command run for the item
static int	nval[Maxitems];
static Image	*back, *high, *bord, *text, *htext;

static void	drawfield(int);

static void
usage(void)
{
	fprint(2, "usage: 9menu [-ti] [-f font] [-l label] [-W winsize] [item ...]\n");
	exits("usage");
}

static void
additem(char *s)
{
	char *p;
	int i;

	if(nitems >= Maxitems)
		sysfatal("too many items");
	if((p = strchr(s, '!')) != nil){
		*p = 0;
		cmd[nitems] = p+1;
		while(p > s && (p[-1] == ' ' || p[-1] == '\t'))
			*--p = 0;
	}
	if(iflag){
		val[nitems] = malloc(Maxval*sizeof(Rune));
		if(val[nitems] == nil)
			sysfatal("malloc: %r");
		if((p = strchr(s, '=')) != nil){
			*p++ = 0;
			for(i = 0; *p != 0 && i < Maxval; i++)
				p += chartorune(&val[nitems][i], p);
			nval[nitems] = i;
		}
	}
	items[nitems++] = s;
}

static void
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

static int
itemht(void)
{
	return font->height+Gap;
}

static int
nshown(void)
{
	int n;

	n = (Dy(screen->r)-2*Margin)/itemht();
	if(n < 1)
		n = 1;
	return n;
}

// space above the items, to center them when the window is taller
static int
yoff(void)
{
	int n;

	n = Dy(screen->r)-2*Margin - nitems*itemht();
	if(n <= 0)
		return 0;
	return n/2;
}

static Rectangle
itemrect(int i)
{
	Rectangle r;

	r = insetrect(screen->r, Margin);
	r.min.y += yoff() + (i-top)*itemht();
	r.max.y = r.min.y+itemht();
	return r;
}

static void
drawitem(int i)
{
	Rectangle r;
	Image *fg;
	Point p;

	if(i < top || i >= top+nshown() || i >= nitems)
		return;
	if(iflag){
		drawfield(i);
		return;
	}
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

static void
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

// make item i (-1 for none) the selected one, scrolling to show it
static void
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

static void
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

static int
itemat(Point p)
{
	Rectangle r;
	int i;

	r = insetrect(screen->r, Margin);
	if(!ptinrect(p, r))
		return -1;
	if(p.y < r.min.y+yoff())
		return -1;
	i = top+(p.y-r.min.y-yoff())/itemht();
	if(i >= nitems || i >= top+nshown())
		return -1;
	return i;
}

static void
choose(int i)
{
	if(i < 0 || i >= nitems)
		return;
	if(cmd[i] != nil){
		// the command prints what it likes, and its status is ours
		execlp("rc", "rc", "-c", cmd[i], nil);
		sysfatal("exec rc: %r");
	}
	if(tflag)
		print("%s\n", items[i]);
	else
		print("%d\n", i+1);
	exits(nil);
}

int	_fontpipe(char*);

// Guess the height of the font before the window exists,
// by reading the first line of the font file, so the window
// can be created at about the right size.
static int
fontheight(char *name)
{
	char buf[64], *p;
	int fd, n;

	if(name == nil)
		name = getenv("font");
	if(name == nil)
		return 0;
	if((p = strchr(name, '*')) != nil && p[1] == '/')	// scale*name
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

// set winsize to fit the items, guessing the font size
static void
guesswinsize(char *fontname)
{
	int i, n, h, cw, w;

	h = fontheight(fontname);
	cw = (h+1)/2;	// a guess at the average width
	if(h <= 0 || h > 200){
		// *default* font
		h = 15;
		cw = 9;
	}
	w = 0;
	for(i = 0; i < nitems; i++){
		n = utflen(items[i]);
		if(iflag)
			n += 1 + (nval[i] > 40 ? nval[i] : 40);
		if(n > w)
			w = n;
	}
	n = nitems;
	if(n > Maxshown)
		n = Maxshown;
	winsize = smprint("%dx%d", (w+4)*cw + 2*Margin, n*(h+Gap) + 2*Margin);
}

// fix the window size now that the font is known, if it is off
static void
fitwindow(void)
{
	int i, w, h, n, dx, dy;

	w = 0;
	for(i = 0; i < nitems; i++)
		if(stringwidth(font, items[i]) > w)
			w = stringwidth(font, items[i]);
	if(iflag){
		n = 40;
		for(i = 0; i < nitems; i++)
			if(nval[i] > n)
				n = nval[i];
		w += stringwidth(font, " ") + n*stringwidth(font, "0");
	}
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

// width of the widest prompt, where the fields start
static int
promptwidth(void)
{
	int i, w;

	w = 0;
	for(i = 0; i < nitems; i++)
		if(stringwidth(font, items[i]) > w)
			w = stringwidth(font, items[i]);
	return w;
}

// a field: its prompt, right aligned, and its text;
// the selected one has the cursor
static void
drawfield(int i)
{
	Rectangle r;
	Point p;
	int pw;

	r = itemrect(i);
	draw(screen, r, back, nil, ZP);
	pw = promptwidth();
	p = Pt(r.min.x+pw-stringwidth(font, items[i]), r.min.y+Gap/2);
	string(screen, p, i == sel ? high : bord, ZP, font, items[i]);
	if(cmd[i] != nil)	// underlined: click it to run the command
		draw(screen, Rect(p.x, p.y+font->ascent+1, r.min.x+pw, p.y+font->ascent+2),
			i == sel ? high : bord, nil, ZP);
	p.x = r.min.x+pw+stringwidth(font, " ");
	p = runestringn(screen, p, text, ZP, font, val[i], nval[i]);
	if(i == sel)
		draw(screen, Rect(p.x, p.y, p.x+2, p.y+font->height), text, nil, ZP);
}

// run the command of field i and set the field to the
// first line it prints, if any
static void
runcmd(int i)
{
	int p[2], pid, n, m;
	char buf[1024], *e;

	if(pipe(p) < 0)
		return;
	switch(pid = fork()){
	case -1:
		close(p[0]);
		close(p[1]);
		return;
	case 0:
		close(p[0]);
		dup(p[1], 1);
		close(p[1]);
		execlp("rc", "rc", "-c", cmd[i], nil);
		_exits("exec");
	}
	close(p[1]);
	n = 0;
	while(n < sizeof buf-1 && (m = read(p[0], buf+n, sizeof buf-1-n)) > 0)
		n += m;
	close(p[0]);
	while(waitpid() != pid)
		;
	buf[n] = 0;
	if((e = strchr(buf, '\n')) != nil)
		*e = 0;
	if(buf[0] == 0)
		return;
	e = buf;
	for(n = 0; *e != 0 && n < Maxval; n++)
		e += chartorune(&val[i][n], e);
	nval[i] = n;
}

// run the -i form: edit the fields, print them on Return
static void
doform(void)
{
	Event e;
	Mouse m;
	int i;

	redraw();
	for(;;){
		switch(event(&e)){
		case Emouse:
			m = e.mouse;
			if(m.buttons & 8)
				scroll(-1);
			else if(m.buttons & 16)
				scroll(1);
			else if((m.buttons & 7) && (i = itemat(m.xy)) >= 0){
				setsel(i);
				// a click on the prompt runs its command
				if(cmd[i] != nil && m.xy.x < itemrect(i).min.x+promptwidth()){
					while(emouse().buttons & 7)
						;
					runcmd(i);
					redraw();
				}
			}
			break;
		case Ekeyboard:
			switch(e.kbdc){
			case '\n':
			case '\r':
				// _ for an empty field, so rc's `{} keeps it
				for(i = 0; i < nitems; i++)
					if(nval[i] == 0)
						print("_\n");
					else
						print("%.*S\n", nval[i], val[i]);
				exits(nil);
			case Kesc:
			case Kdel:
				exits("cancel");
			case '\t':
			case Kdown:
			case 0x0e:	// ^N
				setsel(sel+1 < nitems ? sel+1 : 0);
				break;
			case Kup:
			case 0x10:	// ^P
				setsel(sel > 0 ? sel-1 : nitems-1);
				break;
			case '\b':	// backspace
				if(nval[sel] > 0)
					nval[sel]--;
				break;
			case 0x15:	// ^U
				nval[sel] = 0;
				break;
			case 0x17:	// ^W
				while(nval[sel] > 0 && isspacerune(val[sel][nval[sel]-1]))
					nval[sel]--;
				while(nval[sel] > 0 && !isspacerune(val[sel][nval[sel]-1]))
					nval[sel]--;
				break;
			default:
				if(e.kbdc >= ' ' && e.kbdc < KF && nval[sel] < Maxval)
					val[sel][nval[sel]++] = e.kbdc;
				break;
			}
			drawfield(sel);
			flushimage(display, 1);
			break;
		}
	}
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
	case 'i':
		iflag = 1;
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
	if(iflag)
		doform();
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
			if(itemat(m.xy) >= 0)
				setsel(itemat(m.xy));
			if(buttons && (m.buttons & 7) == 0){
				// released: choose what is under the mouse
				buttons = 0;
				choose(itemat(m.xy));
			}
			break;
		case Ekeyboard:
			switch(e.kbdc){
			case Kup:
			case 0x10:	// ^P
				if(sel < top || sel >= top+nshown())
					setsel(top+nshown()-1 < nitems ? top+nshown()-1 : nitems-1);
				else
					setsel(sel <= 0 ? 0 : sel-1);
				break;
			case Kdown:
			case 0x0e:	// ^N
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
