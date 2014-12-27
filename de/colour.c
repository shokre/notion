/*
 * ion/de/colour.h
 *
 * Copyright (c) Tuomo Valkonen 1999-2009.
 *
 * See the included file LICENSE for details.
 */

#include <ioncore/common.h>
#include "colour.h"


bool de_alloc_colour(WRootWin *rootwin, DEColour *ret, const char *name)
{
 	XftColor c;
	if(name==NULL)
		return FALSE;
	if (XftColorAllocName(
			ioncore_g.dpy,
			DefaultVisual(ioncore_g.dpy, 0),
			rootwin->default_cmap,
			name,
			&c
	))
		{
		*ret = c;
		return TRUE;
	}

	return FALSE;
}


bool de_duplicate_colour(WRootWin *rootwin, DEColour in, DEColour *out)
{
	return XftColorAllocValue(
			ioncore_g.dpy,
			DefaultVisual(ioncore_g.dpy, 0),
			rootwin->default_cmap,
			&(in.color),
			out
	);
}


void de_free_colour_group(WRootWin *rootwin, DEColourGroup *cg)
{
	de_free_colour(rootwin, cg->bg);
	de_free_colour(rootwin, cg->fg);
	de_free_colour(rootwin, cg->hl);
	de_free_colour(rootwin, cg->sh);
	de_free_colour(rootwin, cg->pad);
	gr_stylespec_unalloc(&cg->spec);
}


void de_free_colour(WRootWin *rootwin, DEColour col)
{
	XftColorFree(ioncore_g.dpy, DefaultVisual(ioncore_g.dpy, 0), rootwin->default_cmap, &col);
}

