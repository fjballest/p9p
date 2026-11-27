// mutool run $PLAN9/lib/apdflinks.js file page
// prints the links in page as "x0 y0 x1 y1 target" (points,
// from the top left); target is #n for page n of the document.
// Used by apdf(1) (see macme(1)).
var doc = Document.openDocument(scriptArgs[0]);
var page = doc.loadPage(parseInt(scriptArgs[1]) - 1);
var links = page.getLinks();
for (var i = 0; i < links.length; i++) {
	var l = links[i];
	var b = l.getBounds ? l.getBounds() : l.bounds;
	var u = l.getURI ? l.getURI() : l.uri;
	var t = u;
	if (!(l.isExternal ? l.isExternal() : /^[a-zA-Z][a-zA-Z0-9+.-]*:/.test(u))) {
		var d = doc.resolveLink(u);
		if (typeof d == "object" && d != null)
			d = d.page !== undefined ? d.page : d[1];
		if (typeof d != "number" || d < 0)
			continue;
		t = "#" + (d + 1);
	}
	if (t)
		print(Math.floor(b[0]) + " " + Math.floor(b[1]) + " " + Math.ceil(b[2]) + " " + Math.ceil(b[3]) + " " + t);
}
