/*
 * Loop construct hidden behind cpp, loop statement spanning multiple lines.
 */

static void
render_run(struct regress_html *r, const struct run *run)
{
	struct html *html = r->html;
	const char *status = run_status_str(run->status);

	HTML_NODE_ATTR(html, "td", HTML_ATTR("class", status)) {
		HTML_NODE_ATTR(html, "a",
		    HTML_ATTR("class", "status"), HTML_ATTR("href", run->log))
			HTML_TEXT(html, status);
	}
}
