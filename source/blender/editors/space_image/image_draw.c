/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2002-2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_image_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "PIL_time.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BKE_colortools.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_screen.h"

#include "UI_resources.h"
#include "UI_text.h"
#include "UI_view2d.h"

#include "WM_api.h"

#include "image_intern.h"

#define HEADER_HEIGHT 18

#if 0
static int image_preview_active(SpaceImage *sima, Scene *scene, float *xim, float *yim)
{
	/* only when compositor shows, and image handler set */
	if(sima->image && sima->image->type==IMA_TYPE_COMPOSITE) {
		/* XXX panels .. */
#if 0
		short a;
	
		for(a=0; a<SPACE_MAXHANDLER; a+=2) {
			if(sima->blockhandler[a] == IMAGE_HANDLER_PREVIEW) {
				if(xim) *xim= (scene->r.size*scene->r.xsch)/100;
				if(yim) *yim= (scene->r.size*scene->r.ysch)/100;
				return 1;
			}
		}
#endif
	}
	return 0;
}
#endif

/* are there curves? curves visible? and curves do something? */
static int image_curves_active(SpaceImage *sima)
{
	if(sima->cumap) {
		if(curvemapping_RGBA_does_something(sima->cumap)) {
			/* XXX panels .. */
#if 0
			short a;
			for(a=0; a<SPACE_MAXHANDLER; a+=2) {
				if(sima->blockhandler[a] == IMAGE_HANDLER_CURVES)
					return 1;
			}
#endif
		}
	}

	return 0;
}

static void image_verify_buffer_float(SpaceImage *sima, ImBuf *ibuf)
{
	/* detect if we need to redo the curve map.
	   ibuf->rect is zero for compositor and render results after change 
	   convert to 32 bits always... drawing float rects isnt supported well (atis)
	
	   NOTE: if float buffer changes, we have to manually remove the rect
	*/

	if(ibuf->rect_float) {
		if(ibuf->rect==NULL) {
			if(image_curves_active(sima))
				curvemapping_do_ibuf(sima->cumap, ibuf);
			else 
				IMB_rect_from_float(ibuf);
		}
		else if(sima->pad) {
			sima->pad= 0; // XXX temp for render updates!
			IMB_rect_from_float(ibuf);
		}
	}
}

static void sima_draw_render_info(SpaceImage *sima, ARegion *ar)
{
	rcti rect;
	float colf[3];
	int showspare= 0; // XXX BIF_show_render_spare();
	char *str= "render text"; // XXX BIF_render_text();
	
	if(str==NULL)
		return;
	
	rect= ar->winrct;
	rect.ymin= rect.ymax - HEADER_HEIGHT;
	
	glaDefine2DArea(&rect);
	
	/* clear header rect */
	UI_GetThemeColor3fv(TH_BACK, colf);
	glClearColor(colf[0]+0.1f, colf[1]+0.1f, colf[2]+0.1f, 1.0); 
	glClear(GL_COLOR_BUFFER_BIT);
	
	UI_ThemeColor(TH_TEXT_HI);
	glRasterPos2i(12, 5);
	UI_RasterPos(12, 5);

	if(showspare) {
		UI_DrawString(G.fonts, "(Previous)", 0);
		glRasterPos2i(72, 5);
		UI_RasterPos(72, 5);
	}

	UI_DrawString(G.fonts, str, 0);
}

/*static void sima_draw_image_info(ARegion *ar, int channels, int x, int y, char *cp, float *fp, int *zp, float *zpf)
{
	char str[256];
	int ofs;
	
	ofs= sprintf(str, "X: %d Y: %d ", x, y);
	if(cp)
		ofs+= sprintf(str+ofs, "| R: %d G: %d B: %d A: %d ", cp[0], cp[1], cp[2], cp[3]);

	if(fp) {
		if(channels==4)
			ofs+= sprintf(str+ofs, "| R: %.3f G: %.3f B: %.3f A: %.3f ", fp[0], fp[1], fp[2], fp[3]);
		else if(channels==1)
			ofs+= sprintf(str+ofs, "| Val: %.3f ", fp[0]);
		else if(channels==3)
			ofs+= sprintf(str+ofs, "| R: %.3f G: %.3f B: %.3f ", fp[0], fp[1], fp[2]);
	}

	if(zp)
		ofs+= sprintf(str+ofs, "| Z: %.4f ", 0.5+0.5*(((float)*zp)/(float)0x7fffffff));
	if(zpf)
		ofs+= sprintf(str+ofs, "| Z: %.3f ", *zpf);
	
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	
	glColor4f(.0,.0,.0,.25);
	glRectf(0.0, 0.0, ar->winrct.xmax - ar->winrct.xmin + 1, 30.0);
	glDisable(GL_BLEND);
	
	glColor3ub(255, 255, 255);
	glRasterPos2i(10, 10);
	UI_RasterPos(10, 10);
	
	UI_DrawString(G.fonts, str, 0);
}*/

/* image drawing */

static void draw_image_grid(ARegion *ar, float zoomx, float zoomy)
{
	float gridsize, gridstep= 1.0f/32.0f;
	float fac, blendfac;
	int x1, y1, x2, y2;
	
	/* the image is located inside (0,0),(1, 1) as set by view2d */
	UI_ThemeColorShade(TH_BACK, 20);

	UI_view2d_to_region_no_clip(&ar->v2d, 0.0f, 0.0f, &x1, &y1);
	UI_view2d_to_region_no_clip(&ar->v2d, 1.0f, 1.0f, &x2, &y2);
	glRectf(x1, y1, x2, y2);

	/* gridsize adapted to zoom level */
	gridsize= 0.5f*(zoomx+zoomy);
	if(gridsize<=0.0f) return;
	
	if(gridsize<1.0f) {
		while(gridsize<1.0f) {
			gridsize*= 4.0;
			gridstep*= 4.0;
		}
	}
	else {
		while(gridsize>=4.0f) {
			gridsize/= 4.0;
			gridstep/= 4.0;
		}
	}
	
	/* the fine resolution level */
	blendfac= 0.25*gridsize - floor(0.25*gridsize);
	CLAMP(blendfac, 0.0, 1.0);
	UI_ThemeColorShade(TH_BACK, (int)(20.0*(1.0-blendfac)));
	
	fac= 0.0f;
	glBegin(GL_LINES);
	while(fac<1.0f) {
		glVertex2f(x1, y1*(1.0f-fac) + y2*fac);
		glVertex2f(x2, y1*(1.0f-fac) + y2*fac);
		glVertex2f(x1*(1.0f-fac) + x2*fac, y1);
		glVertex2f(x1*(1.0f-fac) + x2*fac, y2);
		fac+= gridstep;
	}
	
	/* the large resolution level */
	UI_ThemeColor(TH_BACK);
	
	fac= 0.0f;
	while(fac<1.0f) {
		glVertex2f(x1, y1*(1.0f-fac) + y2*fac);
		glVertex2f(x2, y1*(1.0f-fac) + y2*fac);
		glVertex2f(x1*(1.0f-fac) + x2*fac, y1);
		glVertex2f(x1*(1.0f-fac) + x2*fac, y2);
		fac+= 4.0f*gridstep;
	}
	glEnd();
}

static void sima_draw_alpha_backdrop(float x1, float y1, float xsize, float ysize, float zoomx, float zoomy)
{
	GLubyte checker_stipple[32*32/8] =
	{
		255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0, \
		255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0, \
		255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0, \
		255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0, \
		0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255, \
		0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255, \
		0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255, \
		0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255, \
	};
	
	glColor3ub(100, 100, 100);
	glRectf(x1, y1, x1 + zoomx*xsize, y1 + zoomy*ysize);
	glColor3ub(160, 160, 160);

	glEnable(GL_POLYGON_STIPPLE);
	glPolygonStipple(checker_stipple);
	glRectf(x1, y1, x1 + zoomx*xsize, y1 + zoomy*ysize);
	glDisable(GL_POLYGON_STIPPLE);
}

static void sima_draw_alpha_pixels(float x1, float y1, int rectx, int recty, unsigned int *recti)
{
	
	/* swap bytes, so alpha is most significant one, then just draw it as luminance int */
	if(ENDIAN_ORDER == B_ENDIAN)
		glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);

	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_LUMINANCE, GL_UNSIGNED_INT, recti);
	glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
}

static void sima_draw_alpha_pixelsf(float x1, float y1, int rectx, int recty, float *rectf)
{
	float *trectf= MEM_mallocN(rectx*recty*4, "temp");
	int a, b;
	
	for(a= rectx*recty -1, b= 4*a+3; a>=0; a--, b-=4)
		trectf[a]= rectf[b];
	
	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_LUMINANCE, GL_FLOAT, trectf);
	MEM_freeN(trectf);
	/* ogl trick below is slower... (on ATI 9600) */
//	glColorMask(1, 0, 0, 0);
//	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_RGBA, GL_FLOAT, rectf+3);
//	glColorMask(0, 1, 0, 0);
//	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_RGBA, GL_FLOAT, rectf+2);
//	glColorMask(0, 0, 1, 0);
//	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_RGBA, GL_FLOAT, rectf+1);
//	glColorMask(1, 1, 1, 1);
}

static void sima_draw_zbuf_pixels(float x1, float y1, int rectx, int recty, int *recti)
{
	/* zbuffer values are signed, so we need to shift color range */
	glPixelTransferf(GL_RED_SCALE, 0.5f);
	glPixelTransferf(GL_GREEN_SCALE, 0.5f);
	glPixelTransferf(GL_BLUE_SCALE, 0.5f);
	glPixelTransferf(GL_RED_BIAS, 0.5f);
	glPixelTransferf(GL_GREEN_BIAS, 0.5f);
	glPixelTransferf(GL_BLUE_BIAS, 0.5f);
	
	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_LUMINANCE, GL_INT, recti);
	
	glPixelTransferf(GL_RED_SCALE, 1.0f);
	glPixelTransferf(GL_GREEN_SCALE, 1.0f);
	glPixelTransferf(GL_BLUE_SCALE, 1.0f);
	glPixelTransferf(GL_RED_BIAS, 0.0f);
	glPixelTransferf(GL_GREEN_BIAS, 0.0f);
	glPixelTransferf(GL_BLUE_BIAS, 0.0f);
}

static void sima_draw_zbuffloat_pixels(Scene *scene, float x1, float y1, int rectx, int recty, float *rect_float)
{
	float bias, scale, *rectf, clipend;
	int a;
	
	if(scene->camera && scene->camera->type==OB_CAMERA) {
		bias= ((Camera *)scene->camera->data)->clipsta;
		clipend= ((Camera *)scene->camera->data)->clipend;
		scale= 1.0f/(clipend-bias);
	}
	else {
		bias= 0.1f;
		scale= 0.01f;
		clipend= 100.0f;
	}
	
	rectf= MEM_mallocN(rectx*recty*4, "temp");
	for(a= rectx*recty -1; a>=0; a--) {
		if(rect_float[a]>clipend)
			rectf[a]= 0.0f;
		else if(rect_float[a]<bias)
			rectf[a]= 1.0f;
		else {
			rectf[a]= 1.0f - (rect_float[a]-bias)*scale;
			rectf[a]*= rectf[a];
		}
	}
	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_LUMINANCE, GL_FLOAT, rectf);
	
	MEM_freeN(rectf);
}

static void draw_image_buffer(SpaceImage *sima, ARegion *ar, Scene *scene, ImBuf *ibuf, float fx, float fy, float zoomx, float zoomy)
{
	int x, y;

	/* set zoom */
	glPixelZoom(zoomx, zoomy);

	/* find window pixel coordinates of origin */
	UI_view2d_to_region_no_clip(&ar->v2d, fx, fy, &x, &y);

	/* this part is generic image display */
	if(sima->flag & SI_SHOW_ALPHA) {
		if(ibuf->rect)
			sima_draw_alpha_pixels(x, y, ibuf->x, ibuf->y, ibuf->rect);
		else if(ibuf->rect_float && ibuf->channels==4)
			sima_draw_alpha_pixelsf(x, y, ibuf->x, ibuf->y, ibuf->rect_float);
	}
	else if(sima->flag & SI_SHOW_ZBUF && (ibuf->zbuf || ibuf->zbuf_float || (ibuf->channels==1))) {
		if(ibuf->zbuf)
			sima_draw_zbuf_pixels(x, y, ibuf->x, ibuf->y, ibuf->zbuf);
		else if(ibuf->zbuf_float)
			sima_draw_zbuffloat_pixels(scene, x, y, ibuf->x, ibuf->y, ibuf->zbuf_float);
		else if(ibuf->channels==1)
			sima_draw_zbuffloat_pixels(scene, x, y, ibuf->x, ibuf->y, ibuf->rect_float);
	}
	else {
		if(sima->flag & SI_USE_ALPHA) {
			sima_draw_alpha_backdrop(x, y, ibuf->x, ibuf->y, zoomx, zoomy);

			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		}

		/* we don't draw floats buffers directly but
		 * convert them, and optionally apply curves */
		image_verify_buffer_float(sima, ibuf);

		if(ibuf->rect)
			glaDrawPixelsSafe(x, y, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
		/*else
			glaDrawPixelsSafe(x, y, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_FLOAT, ibuf->rect_float);*/
		
		if(sima->flag & SI_USE_ALPHA)
			glDisable(GL_BLEND);
	}

	/* reset zoom */
	glPixelZoom(1.0f, 1.0f);
}

static unsigned int *get_part_from_ibuf(ImBuf *ibuf, short startx, short starty, short endx, short endy)
{
	unsigned int *rt, *rp, *rectmain;
	short y, heigth, len;

	/* the right offset in rectot */

	rt= ibuf->rect+ (starty*ibuf->x+ startx);

	len= (endx-startx);
	heigth= (endy-starty);

	rp=rectmain= MEM_mallocN(heigth*len*sizeof(int), "rect");
	
	for(y=0; y<heigth; y++) {
		memcpy(rp, rt, len*4);
		rt+= ibuf->x;
		rp+= len;
	}
	return rectmain;
}

static void draw_image_buffer_tiled(SpaceImage *sima, ARegion *ar, Image *ima, ImBuf *ibuf, float zoomx, float zoomy)
{
	unsigned int *rect;
	int dx, dy, sx, sy, x, y;

	/* verify valid values, just leave this a while */
	if(ima->xrep<1) return;
	if(ima->yrep<1) return;
	
	glPixelZoom(zoomx, zoomy);

	if(sima->curtile >= ima->xrep*ima->yrep) 
		sima->curtile = ima->xrep*ima->yrep - 1; 
	
	/* create char buffer from float if needed */
	image_verify_buffer_float(sima, ibuf);

	/* retrieve part of image buffer */
	dx= ibuf->x/ima->xrep;
	dy= ibuf->y/ima->yrep;
	sx= (sima->curtile % ima->xrep)*dx;
	sy= (sima->curtile / ima->xrep)*dy;
	rect= get_part_from_ibuf(ibuf, sx, sy, sx+dx, sy+dy);
	
	/* draw repeated */
	for(sy=0; sy+dy<=ibuf->y; sy+= dy) {
		for(sx=0; sx+dx<=ibuf->x; sx+= dx) {
			UI_view2d_view_to_region(&ar->v2d, (float)sx/(float)ibuf->x, (float)sy/(float)ibuf->y, &x, &y);

			glaDrawPixelsSafe(x, y, dx, dy, dx, GL_RGBA, GL_UNSIGNED_BYTE, rect);
		}
	}

	glPixelZoom(1.0f, 1.0f);

	MEM_freeN(rect);
}

static void draw_image_buffer_repeated(SpaceImage *sima, ARegion *ar, Scene *scene, ImBuf *ibuf, float zoomx, float zoomy)
{
	float x, y;
	double time_current;
	
	time_current = PIL_check_seconds_timer();

	for(x=ar->v2d.cur.xmin; x<ar->v2d.cur.xmax; x += zoomx) { 
		for(y=ar->v2d.cur.ymin; y<ar->v2d.cur.ymax; y += zoomy) { 
			draw_image_buffer(sima, ar, scene, ibuf, x, y, zoomx, zoomy);

			/* only draw until running out of time */
			if((PIL_check_seconds_timer() - time_current) > 0.25)
				return;
		}
	}
}

/* draw uv edit */

/* XXX this becomes draw extra? */
#if 0
		glPixelZoom(zoomx, zoomy);

		if(sima->flag & SI_EDITTILE) {
			/* create char buffer from float if needed */
			image_verify_buffer_float(sima, ibuf);

			glaDrawPixelsSafe(x1, y1, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
			
			glPixelZoom(1.0, 1.0);
			
			dx= ibuf->x/sima->image->xrep;
			dy= ibuf->y/sima->image->yrep;
			sy= (sima->curtile / sima->image->xrep);
			sx= sima->curtile - sy*sima->image->xrep;
	
			sx*= dx;
			sy*= dy;
			
			calc_image_view(sima, 'p');	/* pixel */
			myortho2(G.v2d->cur.xmin, G.v2d->cur.xmax, G.v2d->cur.ymin, G.v2d->cur.ymax);
			
			cpack(0x0);
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); glRects(sx,  sy,  sx+dx-1,  sy+dy-1); glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			cpack(0xFFFFFF);
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); glRects(sx+1,  sy+1,  sx+dx,  sy+dy); glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}
#endif

/* draw grease pencil */

static void draw_image_grease_pencil(SpaceImage *sima, ImBuf *ibuf)
{
	/* XXX bring back */
	/* draw grease-pencil ('image' strokes) */
	if (sima->flag & SI_DISPGP)
		; // XXX draw_gpencil_2dimage(sa, ibuf);

#if 0
	mywinset(sa->win);	/* restore scissor after gla call... */
	wmOrtho2(-0.375, sa->winx-0.375, -0.375, sa->winy-0.375);
#endif
	
	/* draw grease-pencil (screen strokes) */
	if (sima->flag & SI_DISPGP)
		; // XXX draw_gpencil_2dview(sa, NULL);
}

/* XXX becomes WM paint cursor */
#if 0
static void draw_image_view_tool(Scene *scene)
{
	ToolSettings *settings= scene->toolsettings;
	Brush *brush= settings->imapaint.brush;
	short mval[2];
	float radius;
	int draw= 0;

	if(brush) {
		if(settings->imapaint.flag & IMAGEPAINT_DRAWING) {
			if(settings->imapaint.flag & IMAGEPAINT_DRAW_TOOL_DRAWING)
				draw= 1;
		}
		else if(settings->imapaint.flag & IMAGEPAINT_DRAW_TOOL)
			draw= 1;
		
		if(draw) {
			getmouseco_areawin(mval);

			radius= brush->size*G.sima->zoom/2;
			fdrawXORcirc(mval[0], mval[1], radius);

			if (brush->innerradius != 1.0) {
				radius *= brush->innerradius;
				fdrawXORcirc(mval[0], mval[1], radius);
			}
		}
	}
}
#endif

static unsigned char *get_alpha_clone_image(Scene *scene, int *width, int *height)
{
	Brush *brush = scene->toolsettings->imapaint.brush;
	ImBuf *ibuf;
	unsigned int size, alpha;
	unsigned char *rect, *cp;

	if(!brush || !brush->clone.image)
		return NULL;
	
	ibuf= BKE_image_get_ibuf(brush->clone.image, NULL);

	if(!ibuf || !ibuf->rect)
		return NULL;

	rect= MEM_dupallocN(ibuf->rect);
	if(!rect)
		return NULL;

	*width= ibuf->x;
	*height= ibuf->y;

	size= (*width)*(*height);
	alpha= (unsigned char)255*brush->clone.alpha;
	cp= rect;

	while(size-- > 0) {
		cp[3]= alpha;
		cp += 4;
	}

	return rect;
}

static void draw_image_paint_helpers(SpaceImage *sima, ARegion *ar, Scene *scene, float zoomx, float zoomy)
{
	Brush *brush;
	int x, y, w, h;
	unsigned char *clonerect;

	brush= scene->toolsettings->imapaint.brush;

	if(brush && (scene->toolsettings->imapaint.tool == PAINT_TOOL_CLONE)) {
		/* this is not very efficient, but glDrawPixels doesn't allow
		   drawing with alpha */
		clonerect= get_alpha_clone_image(scene, &w, &h);

		if(clonerect) {
			UI_view2d_to_region_no_clip(&ar->v2d, brush->clone.offset[0], brush->clone.offset[1], &x, &y);

			glPixelZoom(zoomx, zoomy);

			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glaDrawPixelsSafe(x, y, w, h, w, GL_RGBA, GL_UNSIGNED_BYTE, clonerect);
			glDisable(GL_BLEND);

			glPixelZoom(1.0, 1.0);

			MEM_freeN(clonerect);
		}
	}
}

/* draw main image area */

void draw_image_main(SpaceImage *sima, ARegion *ar, Scene *scene)
{
	Image *ima;
	ImBuf *ibuf;
	float zoomx, zoomy;
	int show_viewer, show_render;

	/* XXX can we do this in refresh? */
#if 0
	what_image(sima);
	
	if(sima->image) {
		image_pixel_aspect(sima->image, &xuser_asp, &yuser_asp);
		
		/* UGLY hack? until now iusers worked fine... but for flipbook viewer we need this */
		if(sima->image->type==IMA_TYPE_COMPOSITE) {
			ImageUser *iuser= ntree_get_active_iuser(scene->nodetree);
			if(iuser) {
				BKE_image_user_calc_imanr(iuser, scene->r.cfra, 0);
				sima->iuser= *iuser;
			}
		}
		/* and we check for spare */
		ibuf= get_space_image_buffer(sima);
	}
#endif

	/* put scene context variable in iuser */
	sima->iuser.scene= scene;
	/* retrieve the image and information about it */
	ima= get_space_image(sima);
	ibuf= get_space_image_buffer(sima);
	get_space_image_zoom(sima, ar, &zoomx, &zoomy);

	show_viewer= (ima && ima->source == IMA_SRC_VIEWER);
	show_render= (show_viewer && ima->type == IMA_TYPE_R_RESULT);

	/* draw the image or grid */
	if(ibuf==NULL)
		draw_image_grid(ar, zoomx, zoomy);
	else if(sima->flag & SI_DRAW_TILE)
		draw_image_buffer_repeated(sima, ar, scene, ibuf, zoomx, zoomy);
	else if(ima && (ima->tpageflag & IMA_TILES))
		draw_image_buffer_tiled(sima, ar, ima, ibuf, zoomx, zoomy);
	else
		draw_image_buffer(sima, ar, scene, ibuf, 0.0f, 0.0f, zoomx, zoomy);

	/* grease pencil */
	draw_image_grease_pencil(sima, ibuf);

	/* paint helpers */
	draw_image_paint_helpers(sima, ar, scene, zoomx, zoomy);

	/* render info */
	if(ibuf && show_render)
		sima_draw_render_info(sima, ar);

	/* XXX integrate this code */
#if 0
	if(ibuf) {
		float xoffs=0.0f, yoffs= 0.0f;
		
		if(image_preview_active(sa, &xim, &yim)) {
			xoffs= scene->r.disprect.xmin;
			yoffs= scene->r.disprect.ymin;
			glColor3ub(0,0,0);
			calc_image_view(sima, 'f');	
			myortho2(G.v2d->cur.xmin, G.v2d->cur.xmax, G.v2d->cur.ymin, G.v2d->cur.ymax);
			glRectf(0.0f, 0.0f, 1.0f, 1.0f);
			glLoadIdentity();
		}
	}
#endif

#if 0
	/* it is important to end a view in a transform compatible with buttons */
	bwin_scalematrix(sa->win, sima->blockscale, sima->blockscale, sima->blockscale);
	if(!(G.rendering && show_render))
		image_blockhandlers(sa);
#endif
}

