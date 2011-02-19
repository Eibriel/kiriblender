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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_MARKERS_H
#define ED_MARKERS_H

struct wmKeyConfig;
struct bContext;
struct bAnimContext;
struct TimeMarker;

/* Drawing API ------------------------------ */

/* flags for drawing markers */
enum {
	DRAW_MARKERS_LINES	= (1<<0),
	DRAW_MARKERS_LOCAL	= (1<<1)
};

void draw_markers_time(const struct bContext *C, int flag);

/* Backend API ----------------------------- */

ListBase *ED_context_get_markers(const struct bContext *C);
ListBase *ED_animcontext_get_markers(const struct bAnimContext *ac);

struct TimeMarker *ED_markers_find_nearest_marker(ListBase *markers, float x);
int ED_markers_find_nearest_marker_time(ListBase *markers, float x);

void ED_markers_get_minmax(ListBase *markers, short sel, float *first, float *last);

void ED_markers_make_cfra_list(ListBase *markers, ListBase *lb, short sel);

struct TimeMarker *ED_markers_get_first_selected(ListBase *markers);

/* Operators ------------------------------ */

/* called in screen_ops.c:ED_operatortypes_screen() */
void ED_operatortypes_marker(void); 
/* called in screen_ops.c:ED_keymap_screen() */
void ED_marker_keymap(struct wmKeyConfig *keyconf);

/* debugging only */
void debug_markers_print_list(struct ListBase *markers);

#endif /* ED_MARKERS_H */

