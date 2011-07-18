/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef DNA_MOVIECLIP_TYPES_H
#define DNA_MOVIECLIP_TYPES_H

/** \file DNA_movieclip_types.h
 *  \ingroup DNA
 *  \since may-2011
 *  \author Sergey Sharybin
 */

#include "DNA_ID.h"
#include "DNA_tracking_types.h"

struct anim;
struct ImBuf;

typedef struct MovieClipUser {
	int framenr;	/* current frame number */
	int pad;
} MovieClipUser;

typedef struct MovieClip {
	ID id;

	char name[240];		/* file path */

	int source;			/* sequence or movie */
	int lastframe;		/* last accessed frame number */
	int lastsize[2];	/* size of last accessed frame */

	struct anim *anim;	/* movie source data */
	void *ibuf_cache;	/* cache of ibufs, not in file */

	struct MovieTracking tracking;		/* data for SfM tracking */
	void *tracking_context;				/* context of tracking job
										   used to synchronize data like framenumber
										   in SpaceClip clip user */

	int sel_type;		/* last selected thing */
	int pad;
	void *last_sel;
} MovieClip;

typedef struct MovieClipScopes {
	int ok;							/* 1 means scopes are ok and re-calculaito is unneeded */
	int track_preview_height;		/* height of track preview widget */
	struct ImBuf *track_preview;	/* ImBuf displayed in track preview */
	float track_pos[2];				/* sub-pizel position of marker in track ImBuf */
	short track_disabled;			/* active track is disabled, special notifier should be drawn */
	char pad[6];
	float *marker_pos;				/* original marker position. used for sliding from preview */
	float slide_scale[2];			/* scale used for sliding from previewe area */
} MovieClipScopes;

/* MovieClip->source */
#define MCLIP_SRC_SEQUENCE	1
#define MCLIP_SRC_MOVIE		2

/* MovieClip->selection types */
#define MCLIP_SEL_NONE		0
#define MCLIP_SEL_TRACK		1

#endif
