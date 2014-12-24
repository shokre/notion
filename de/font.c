/*
 * ion/de/font.c
 *
 * Copyright (c) Tuomo Valkonen 1999-2009.
 *
 * See the included file LICENSE for details.
 */

#include <string.h>

#include <libtu/objp.h>
#include <ioncore/common.h>
#include "font.h"
#include "brush.h"
#include "precompose.h"


/*{{{ UTF-8 processing */


#define UTF_DATA 0x3F
#define UTF_2_DATA 0x1F
#define UTF_3_DATA 0x0F
#define UTF_1 0x80
#define UTF_2 0xC0
#define UTF_3 0xE0
#define UTF_4 0xF0
#define UTF_5 0xF8
#define UTF_6 0xFC

static void toucs(const char *str_, int len, XChar2b **str16, int *len16)
{
    int i=0;
    const uchar *str=(const uchar*)str_;
    wchar_t prev=0;
    
    *str16=ALLOC_N(XChar2b, len);
    *len16=0;
    
    while(i<len){
        wchar_t ch=0;
        
        if((str[i] & UTF_3) == UTF_3){
             if(i+2>=len)
                 break;
             ch=((str[i] & UTF_3_DATA) << 12)
                 | ((str[i+1] & UTF_DATA) << 6)
                 | (str[i+2] & UTF_DATA);
            i+=3;
        }else if((str[i] & UTF_2) == UTF_2){
            if(i+1>=len)
                break;
            ch = ((str[i] & UTF_2_DATA) << 6) | (str[i+1] & UTF_DATA);
            i+=2;
        }else if(str[i] < UTF_1){
            ch = str[i];
            i++;
        }else{
            ch='?';
            i++;
        }
        
        if(*len16>0){
            wchar_t precomp=do_precomposition(prev, ch);
            if(precomp!=-1){
                (*len16)--;
                ch=precomp;
            }
        }
            
        (*str16)[*len16].byte2=ch&0xff;
        (*str16)[*len16].byte1=(ch>>8)&0xff;
        (*len16)++;
        prev=ch;
    }
}


/*}}}*/


/*{{{ Load/free */


static DEFont *fonts=NULL;


static bool iso10646_font(const char *fontname)
{
    const char *iso;
    
    if(strchr(fontname, ',')!=NULL)
        return FALSE; /* fontset */
        
    iso=strstr(fontname, "iso10646-1");
    return (iso!=NULL && iso[10]=='\0');
}


DEFont *de_load_font(const char *fontname)
{
	bool dbg_load = FALSE;
    DEFont *fnt;
	XftFont *font;

	assert(fontname!=NULL);

	if (dbg_load) printf("Loading3 font: %s\n",  fontname);

	/* There shouldn't be that many fonts... */
	for(fnt=fonts; fnt!=NULL; fnt=fnt->next){
		if(strcmp(fnt->pattern, fontname)==0){
			if (dbg_load) printf("- found in cache\n");
			fnt->refcount++;
			return fnt;
		}
	}
	if(strncmp(fontname, "xft:", 4)==0){
		font=XftFontOpenName(ioncore_g.dpy, DefaultScreen(ioncore_g.dpy),
				fontname+4);
	}else{
		font=XftFontOpenXlfd(ioncore_g.dpy, DefaultScreen(ioncore_g.dpy), fontname);
	}

	if(font==NULL){
		if(strcmp(fontname, CF_FALLBACK_FONT_NAME)!=0){
			warn(TR("Could not load font \"%s\", trying \"%s\""),
					fontname, CF_FALLBACK_FONT_NAME);
			return de_load_font(CF_FALLBACK_FONT_NAME);
		}
		return NULL;
	}

	fnt=ALLOC(DEFont);

	if(fnt==NULL)
		return NULL;

	fnt->font=font;
	fnt->pattern=scopy(fontname);
	fnt->next=NULL;
	fnt->prev=NULL;
	fnt->refcount=1;

	LINK_ITEM(fonts, fnt, next, prev);

	return fnt;
}


bool de_set_font_for_style(DEStyle *style, DEFont *font)
{
    if(style->font!=NULL)
        de_free_font(style->font);
    
    style->font=font;
    font->refcount++;
    
    return TRUE;
}


bool de_load_font_for_style(DEStyle *style, const char *fontname)
{
    if(style->font!=NULL)
        de_free_font(style->font);
    
    style->font=de_load_font(fontname);

    if(style->font==NULL)
        return FALSE;
    
    return TRUE;
}


void de_free_font(DEFont *font)
{
	if(--font->refcount!=0)
		return;

	if(font->font!=NULL)
		XftFontClose(ioncore_g.dpy, font->font);
	if(font->pattern!=NULL)
		free(font->pattern);

	UNLINK_ITEM(fonts, font, next, prev);
	free(font);
}


/*}}}*/


/*{{{ Lengths */
void int_measure_str(XftFont *font, const char *str, int len, XGlyphInfo *extents)
{
	if(ioncore_g.enc_utf8)
		XftTextExtentsUtf8(ioncore_g.dpy, font, (XftChar8 *)str, len, extents);
	else
		XftTextExtents8(ioncore_g.dpy, font, (XftChar8 *)str, len, extents);

}

void debrush_get_font_extents(DEBrush *brush, GrFontExtents *fnte)
{
    if(brush->d->font==NULL){
        DE_RESET_FONT_EXTENTS(fnte);
        return;
    }
    
    defont_get_font_extents(brush->d->font, fnte);
}


void defont_get_font_extents(DEFont *font, GrFontExtents *fnte)
{

	if(font->font!=NULL){
		fnte->max_height=font->font->ascent+font->font->descent;
		fnte->max_width=font->font->max_advance_width;
		fnte->baseline=font->font->ascent;
		return;
	}
	DE_RESET_FONT_EXTENTS(fnte);
}


uint debrush_get_text_width(DEBrush *brush, const char *text, uint len)
{
    if(brush->d->font==NULL || text==NULL || len==0)
        return 0;
    
    return defont_get_text_width(brush->d->font, text, len);
}


uint defont_get_text_width(DEFont *font, const char *text, uint len)
{
	if(font->font!=NULL){
		XGlyphInfo extents;
		int_measure_str(font->font, text, len, &extents);
		return extents.xOff;

	}else{
		return 0;
	}
}


/*}}}*/


/*{{{ String drawing */
void dbg_draw_rect(XftDraw *draw, XftColor *col, int x, int y, int w, int h) {
	XftDrawRect(draw, col, x, y, w, 1);
	XftDrawRect(draw, col, x, y+h, w, 1);
	XftDrawRect(draw, col, x, y, 1, h);
	XftDrawRect(draw, col, x+w, y, 1, h);
}

void dbg_draw_cross(XftDraw *draw, XftColor *col, int x, int y, int s) {
	XftDrawRect(draw, col, x - (s/2), y, s, 1);
	XftDrawRect(draw, col, x, y - (s/2), 1, s);
}

void debrush_do_draw_string_default(DEBrush *brush, int x, int y,
                                    const char *str, int len, bool needfill,
                                    DEColourGroup *colours)
{
	bool debug_draw = ioncore_g.debug_font_draw;
    GC gc=brush->d->normal_gc;
	XftFont *font=brush->d->font->font;
	XftDraw *draw;
	XGlyphInfo extents;

    if(brush->d->font==NULL)
        return;

	draw=debrush_get_draw(brush);

	if(needfill || debug_draw)
		int_measure_str(font, str, len + 1, &extents);

	if (needfill) {
		int mh = font->ascent + extents.height - extents.y;
		if (font->height > mh)
			mh = font->height;
		XftDrawRect(draw, &(colours->bg.pixel), x-extents.x, y-font->ascent,
			extents.width, mh);
	}

	if(ioncore_g.enc_utf8)
		XftDrawStringUtf8(draw, &(colours->fg.pixel), font, x, y, (XftChar8 *)str,
				len);
	else
		XftDrawString8(draw, &(colours->fg.pixel), font, x, y, (XftChar8 *)str, len);


	if (debug_draw) {
		DEColour debug_col, debug_col_anc, debug_col_proj, debug_col_box;

		// printf("RN: [%s] %d\n", str, extents.width);
		de_alloc_colour(brush->d->rootwin, &debug_col, "yellow");
		de_alloc_colour(brush->d->rootwin, &debug_col_anc, "red");
		de_alloc_colour(brush->d->rootwin, &debug_col_proj, "blue");
		de_alloc_colour(brush->d->rootwin, &debug_col_box, "green");

		// border around text in yellow
		dbg_draw_rect(draw, &(debug_col.pixel), x-extents.x, y-extents.y, extents.width, extents.height);
		// fill box in blue
		dbg_draw_rect(draw, &(debug_col_proj.pixel), x, y-font->ascent, extents.width, font->height);
		// anchor point from ion in red
		dbg_draw_cross(draw, &(debug_col_anc.pixel), x, y, 5);
		// fill start
		dbg_draw_cross(draw, &(debug_col_box.pixel), x, y-font->ascent, 5);
	}
}


void debrush_do_draw_string(DEBrush *brush, int x, int y,
                            const char *str, int len, bool needfill, 
                            DEColourGroup *colours)
{
    CALL_DYN(debrush_do_draw_string, brush, (brush, x, y, str, len,
                                             needfill, colours));
}


void debrush_draw_string(DEBrush *brush, int x, int y,
                         const char *str, int len, bool needfill)
{
    DEColourGroup *cg=debrush_get_current_colour_group(brush);
    if(cg!=NULL)
        debrush_do_draw_string(brush, x, y, str, len, needfill, cg);
}


/*}}}*/

