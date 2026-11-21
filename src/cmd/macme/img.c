/*
 * macme: windows whose body shows an image.
 * A client writes an image(6) to the window's image file;
 * when the file is closed, the body shows it.  Scrolling
 * works as usual, and clicks on the image are sent to the
 * event file as "MI x y buttons 0 " (x and y in the image).
 * The ctl message noimage goes back to text, and pagekeys
 * makes PgUp, PgDn, Home and End come as "KX" events for
 * pgup, pgdown, home and end instead of scrolling.
 * Reading the image file gives the size of the panel and of
 * the image, to fit the image to the panel.
 * nemo
 */
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>
#include <plumb.h>
#include <libsec.h>
#include "dat.h"
#include "fns.h"

static Rectangle
imgrect(Window *w)
{
	Rectangle r;

	r = w->body.all;
	r.min.x += Scrollwid+Scrollgap;
	return r;
}

static void
imgclamp(Window *w)
{
	Rectangle r;
	int mx, my;

	r = imgrect(w);
	mx = Dx(w->img->r) - Dx(r);
	my = Dy(w->img->r) - Dy(r);
	if(w->imgoff.x > mx)
		w->imgoff.x = mx;
	if(w->imgoff.y > my)
		w->imgoff.y = my;
	if(w->imgoff.x < 0)
		w->imgoff.x = 0;
	if(w->imgoff.y < 0)
		w->imgoff.y = 0;
}

void
imgscrdraw(Window *w)
{
	Rectangle r, t;
	int tot, vis;

	if(w->img == nil)
		return;
	r = w->body.scrollr;
	if(Dy(r) <= 0)
		return;
	tot = Dy(w->img->r);
	vis = Dy(imgrect(w));
	t = r;
	if(tot > 0){
		t.min.y = r.min.y + (vlong)Dy(r)*w->imgoff.y/tot;
		t.max.y = r.min.y + (vlong)Dy(r)*(w->imgoff.y+vis)/tot;
	}
	if(t.max.y > r.max.y)
		t.max.y = r.max.y;
	if(t.max.y < t.min.y+2)
		t.max.y = t.min.y+2;
	draw(screen, r, w->body.fr.cols[BORD], nil, ZP);
	draw(screen, t, w->body.fr.cols[BACK], nil, ZP);
	t.min.x = t.max.x-1;
	draw(screen, t, w->body.fr.cols[BORD], nil, ZP);
}

void
imgdraw(Window *w)
{
	Rectangle r;

	if(w->img == nil)
		return;
	r = imgrect(w);
	if(Dx(r) <= 0 || Dy(r) <= 0)
		return;
	imgclamp(w);
	draw(screen, r, w->body.fr.cols[BACK], nil, ZP);
	draw(screen, r, w->img, nil, addpt(w->img->r.min, w->imgoff));
	imgscrdraw(w);
	flushimage(display, 1);
}

void
imgscroll(Window *w, int dy)
{
	Point o;

	if(w->img == nil)
		return;
	o = w->imgoff;
	w->imgoff.y += dy;
	imgclamp(w);
	if(!eqpt(o, w->imgoff))
		imgdraw(w);
}

static void
waitup(void)
{
	while(mousectl->m.buttons & 7)
		readmouse(mousectl);
}

void
imgmouse(Window *w, Mouse *m)
{
	Rectangle r, s;
	Point p;
	int n, h;

	h = w->body.fr.font->height;
	if(m->buttons & (8|16)){
		// devdraw on the mac sends how much to scroll (see acme)
		n = m->buttons/4;
		if(n < 0)
			n = -n;
		if(n < 1)
			n = 1;
		if(n > 30)
			n = 30;
		imgscroll(w, (m->buttons&8) ? -n*h : n*h);
		return;
	}
	r = imgrect(w);
	s = w->body.scrollr;
	if(ptinrect(m->xy, s)){
		// as for text: 1 back, 2 absolute, 3 forward
		n = m->xy.y - s.min.y;
		if(m->buttons & 1)
			imgscroll(w, -(n > h ? n : h));
		else if(m->buttons & 2)
			imgscroll(w, (vlong)Dy(w->img->r)*n/Dy(s) - w->imgoff.y);
		else if(m->buttons & 4)
			imgscroll(w, n > h ? n : h);
		waitup();
		return;
	}
	if((m->buttons & 7) == 0 || !ptinrect(m->xy, r))
		return;
	activewin = w;
	p = addpt(addpt(subpt(m->xy, r.min), w->imgoff), w->img->r.min);
	winevent(w, "I%d %d %d 0 \n", p.x, p.y, m->buttons & 7);
	waitup();
}

// send a page key to the client as if name was executed
static int
imgkey(Window *w, char *name)
{
	w->owner = 'K';
	winevent(w, "X0 0 0 %d %s\n", utflen(name), name);
	return 1;
}

int
imgtype(Window *w, Rune r)
{
	int h, page;

	if(w->pagekeys)
		switch(r){
		case Kpgup:
			return imgkey(w, "pgup");
		case Kpgdown:
			return imgkey(w, "pgdown");
		case Khome:
			return imgkey(w, "home");
		case Kend:
			return imgkey(w, "end");
		}
	h = w->body.fr.font->height;
	page = Dy(imgrect(w)) - h;
	switch(r){
	case Kdown:
	case Kscrollonedown:
		imgscroll(w, 3*h);
		break;
	case Kup:
	case Kscrolloneup:
		imgscroll(w, -3*h);
		break;
	case Kpgdown:
	case ' ':
		imgscroll(w, page);
		break;
	case Kpgup:
		imgscroll(w, -page);
		break;
	case Khome:
		imgscroll(w, -w->imgoff.y);
		break;
	case Kend:
		imgscroll(w, Dy(w->img->r));
		break;
	case Kleft:
		w->imgoff.x -= 3*h;
		imgdraw(w);
		break;
	case Kright:
		w->imgoff.x += 3*h;
		imgdraw(w);
		break;
	default:
		return 0;
	}
	return 1;
}

// show the image written to the image file
char*
imgload(Window *w)
{
	static char err[ERRMAX];
	Image *i;
	int fd;

	fd = tempfile();
	if(fd < 0)
		return "can't create temp file";
	if(write(fd, w->imgbuf, w->nimgbuf) != w->nimgbuf){
		close(fd);
		return "can't write temp file";
	}
	free(w->imgbuf);
	w->imgbuf = nil;
	w->nimgbuf = 0;
	seek(fd, 0, 0);
	i = readimage(display, fd, 0);
	close(fd);
	if(i == nil){
		snprint(err, sizeof err, "bad image: %r");
		return err;
	}
	if(w->img != nil)
		freeimage(w->img);
	w->img = i;
	w->imgoff = ZP;
	winresize(w, w->r, FALSE, TRUE);
	flushimage(display, 1);
	return nil;
}

void
imgfree(Window *w)
{
	if(w->img != nil)
		freeimage(w->img);
	w->img = nil;
	free(w->imgbuf);
	w->imgbuf = nil;
	w->nimgbuf = 0;
	w->pagekeys = FALSE;
}

// panel and image sizes, for the image file
void
imgsize(Window *w, char *buf)
{
	Rectangle r;
	int dx, dy;

	r = imgrect(w);
	dx = dy = 0;
	if(w->img != nil){
		dx = Dx(w->img->r);
		dy = Dy(w->img->r);
	}
	sprint(buf, "%11d %11d %11d %11d ", Dx(r), Dy(r), dx, dy);
}
