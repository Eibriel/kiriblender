
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_sequencer/sequencer_ops.c
 *  \ingroup spseq
 */


#include <stdlib.h>
#include <math.h>


#include "DNA_space_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"



#include "WM_api.h"
#include "WM_types.h"

#include "ED_transform.h" /* transform keymap */

#include "sequencer_intern.h"


/* ************************** registration **********************************/


void sequencer_operatortypes(void)
{
	/* sequencer_edit.c */
	WM_operatortype_append(SEQUENCER_OT_cut);
	WM_operatortype_append(SEQUENCER_OT_mute);
	WM_operatortype_append(SEQUENCER_OT_unmute);
	WM_operatortype_append(SEQUENCER_OT_lock);
	WM_operatortype_append(SEQUENCER_OT_unlock);
	WM_operatortype_append(SEQUENCER_OT_reload);
	WM_operatortype_append(SEQUENCER_OT_refresh_all);
	WM_operatortype_append(SEQUENCER_OT_reassign_inputs);
	WM_operatortype_append(SEQUENCER_OT_swap_inputs);
	WM_operatortype_append(SEQUENCER_OT_duplicate);
	WM_operatortype_append(SEQUENCER_OT_delete);
	WM_operatortype_append(SEQUENCER_OT_images_separate);
	WM_operatortype_append(SEQUENCER_OT_meta_toggle);
	WM_operatortype_append(SEQUENCER_OT_meta_make);
	WM_operatortype_append(SEQUENCER_OT_meta_separate);
	WM_operatortype_append(SEQUENCER_OT_snap);
	WM_operatortype_append(SEQUENCER_OT_next_edit);
	WM_operatortype_append(SEQUENCER_OT_previous_edit);
	WM_operatortype_append(SEQUENCER_OT_swap);
	WM_operatortype_append(SEQUENCER_OT_swap_data);
	WM_operatortype_append(SEQUENCER_OT_rendersize);

	WM_operatortype_append(SEQUENCER_OT_view_all);
	WM_operatortype_append(SEQUENCER_OT_view_selected);
	WM_operatortype_append(SEQUENCER_OT_view_all_preview);
	WM_operatortype_append(SEQUENCER_OT_view_toggle);
	WM_operatortype_append(SEQUENCER_OT_view_zoom_ratio);
	WM_operatortype_append(SEQUENCER_OT_view_ghost_border);

	WM_operatortype_append(SEQUENCER_OT_rebuild_proxy);

	/* sequencer_select.c */
	WM_operatortype_append(SEQUENCER_OT_select_all_toggle);
	WM_operatortype_append(SEQUENCER_OT_select_inverse);
	WM_operatortype_append(SEQUENCER_OT_select);
	WM_operatortype_append(SEQUENCER_OT_select_more);
	WM_operatortype_append(SEQUENCER_OT_select_less);
	WM_operatortype_append(SEQUENCER_OT_select_linked_pick);
	WM_operatortype_append(SEQUENCER_OT_select_linked);
	WM_operatortype_append(SEQUENCER_OT_select_handles);
	WM_operatortype_append(SEQUENCER_OT_select_active_side);
	WM_operatortype_append(SEQUENCER_OT_select_border);
	
	/* sequencer_add.c */
	WM_operatortype_append(SEQUENCER_OT_scene_strip_add);
	WM_operatortype_append(SEQUENCER_OT_movie_strip_add);
	WM_operatortype_append(SEQUENCER_OT_sound_strip_add);
	WM_operatortype_append(SEQUENCER_OT_image_strip_add);
	WM_operatortype_append(SEQUENCER_OT_effect_strip_add);
	WM_operatortype_append(SEQUENCER_OT_properties);

	WM_operatortype_append(SEQUENCER_OT_copy);
	WM_operatortype_append(SEQUENCER_OT_paste);
}


void sequencer_keymap(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap= WM_keymap_find(keyconf, "SequencerCommon", SPACE_SEQ, 0);
	wmKeyMapItem *kmi;

	kmi= WM_keymap_add_item(keymap, "WM_OT_context_toggle", OKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(kmi->ptr, "data_path", "scene.sequence_editor.show_overlay");

	/* operators common to sequence and preview view */
	WM_keymap_add_item(keymap, "SEQUENCER_OT_view_toggle", TABKEY, KM_PRESS, KM_CTRL, 0);

	/* operators for sequence */
	keymap= WM_keymap_find(keyconf, "Sequencer", SPACE_SEQ, 0);
	
	WM_keymap_add_item(keymap, "SEQUENCER_OT_properties", NKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "SEQUENCER_OT_select_all_toggle", AKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "SEQUENCER_OT_select_inverse", IKEY, KM_PRESS, KM_CTRL, 0);
	
	RNA_enum_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_cut", KKEY, KM_PRESS, 0, 0)->ptr, "type", SEQ_CUT_SOFT);
	RNA_enum_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_cut", KKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "type", SEQ_CUT_HARD);
	
	WM_keymap_add_item(keymap, "SEQUENCER_OT_mute", HKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_mute", HKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "unselected", 1);
	
	WM_keymap_add_item(keymap, "SEQUENCER_OT_unmute", HKEY, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_unmute", HKEY, KM_PRESS, KM_ALT|KM_SHIFT, 0)->ptr, "unselected", 1);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_lock", LKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "SEQUENCER_OT_unlock", LKEY, KM_PRESS, KM_SHIFT|KM_ALT, 0);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_reassign_inputs", RKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "SEQUENCER_OT_reload", RKEY, KM_PRESS, KM_ALT, 0);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_duplicate", DKEY, KM_PRESS, KM_SHIFT, 0);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "SEQUENCER_OT_delete", DELKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "SEQUENCER_OT_copy", CKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "SEQUENCER_OT_paste", VKEY, KM_PRESS, KM_CTRL, 0);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_images_separate", YKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_meta_toggle", TABKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_meta_make", GKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "SEQUENCER_OT_meta_separate", GKEY, KM_PRESS, KM_ALT, 0);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_view_all", HOMEKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "SEQUENCER_OT_view_selected", PADPERIOD, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_next_edit", PAGEUPKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "SEQUENCER_OT_previous_edit", PAGEDOWNKEY, KM_PRESS, 0, 0);

	RNA_enum_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_swap", LEFTARROWKEY, KM_PRESS, KM_ALT, 0)->ptr, "side", SEQ_SIDE_LEFT);
	RNA_enum_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_swap", RIGHTARROWKEY, KM_PRESS, KM_ALT, 0)->ptr, "side", SEQ_SIDE_RIGHT);
	
	WM_keymap_add_item(keymap, "SEQUENCER_OT_snap", SKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "SEQUENCER_OT_swap_inputs", SKEY, KM_PRESS, KM_ALT, 0);

	/* multicam editing keyboard layout, switch to camera 1-10 using
	   regular number keys */
	{
		int keys[] = { ONEKEY, TWOKEY, THREEKEY, FOURKEY, FIVEKEY,
			       SIXKEY, SEVENKEY, EIGHTKEY, NINEKEY, ZEROKEY };
		int i;

		for (i = 1; i <= 10; i++) {
			RNA_int_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_cut_multicam", keys[i-1], KM_PRESS, 0, 0)->ptr, "camera", i);
		}
	}

	/* Mouse selection, a bit verbose :/ */
	WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "extend", 1);


	/* 2.4x method, now use Alt for handles and select the side based on which handle was selected */
	/*
	RNA_boolean_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, KM_CTRL, 0)->ptr, "linked_left", 1);
	RNA_boolean_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, KM_ALT, 0)->ptr, "linked_right", 1);
	
	kmi= WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, KM_CTRL|KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "linked_left", 1);
	RNA_boolean_set(kmi->ptr, "linked_right", 1);

	kmi= WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT|KM_CTRL|KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "extend", 1);
	RNA_boolean_set(kmi->ptr, "linked_left", 1);
	RNA_boolean_set(kmi->ptr, "linked_right", 1);

	kmi= WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT|KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "extend", 1);
	RNA_boolean_set(kmi->ptr, "linked_left", 1);

	kmi= WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT|KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "extend", 1);
	RNA_boolean_set(kmi->ptr, "linked_right", 1);
	 */

	/* 2.5 method, Alt and use selected handle */
	RNA_boolean_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, KM_ALT, 0)->ptr, "linked_handle", 1);

	kmi= WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT|KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "extend", 1);
	RNA_boolean_set(kmi->ptr, "linked_handle", 1);

	/* match action editor */
	kmi= WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "left_right", 1); /* grr, these conflict - only use left_right if not over an active seq */
	RNA_boolean_set(kmi->ptr, "linked_time", 1);
	/* adjusted since 2.4 */

	kmi= WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT|KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "extend", 1);
	RNA_boolean_set(kmi->ptr, "linked_time", 1);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_select_more", PADPLUSKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "SEQUENCER_OT_select_less", PADMINUS, KM_PRESS, KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "SEQUENCER_OT_select_linked_pick", LKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_select_linked_pick", LKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "extend", 1);
	
	WM_keymap_add_item(keymap, "SEQUENCER_OT_select_linked", LKEY, KM_PRESS, KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "SEQUENCER_OT_select_border", BKEY, KM_PRESS, 0, 0);

	WM_keymap_add_menu(keymap, "SEQUENCER_MT_add", AKEY, KM_PRESS, KM_SHIFT, 0);
	
	kmi= WM_keymap_add_item(keymap, "WM_OT_context_set_int", OKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "scene.sequence_editor.overlay_frame");
	RNA_int_set(kmi->ptr, "value", 0);

	transform_keymap_for_space(keyconf, keymap, SPACE_SEQ);

	keymap= WM_keymap_find(keyconf, "SequencerPreview", SPACE_SEQ, 0);
	WM_keymap_add_item(keymap, "SEQUENCER_OT_view_all_preview", HOMEKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "SEQUENCER_OT_properties", NKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_view_ghost_border", OKEY, KM_PRESS, 0, 0);

	/* would prefer to use numpad keys for job */
	RNA_float_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_view_zoom_ratio", PAD1, KM_PRESS, 0, 0)->ptr, "ratio", 1.0f);
	
	/* Setting zoom levels is not that useful, except for back to zoom level 1, removing keymap because of conflicts for now */
#if 0
	RNA_float_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_view_zoom_ratio", PAD8, KM_PRESS, KM_SHIFT, 0)->ptr, "ratio", 8.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_view_zoom_ratio", PAD4, KM_PRESS, KM_SHIFT, 0)->ptr, "ratio", 4.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_view_zoom_ratio", PAD2, KM_PRESS, KM_SHIFT, 0)->ptr, "ratio", 2.0f);
	
	RNA_float_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_view_zoom_ratio", PAD2, KM_PRESS, 0, 0)->ptr, "ratio", 0.5f);
	RNA_float_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_view_zoom_ratio", PAD4, KM_PRESS, 0, 0)->ptr, "ratio", 0.25f);
	RNA_float_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_view_zoom_ratio", PAD8, KM_PRESS, 0, 0)->ptr, "ratio", 0.125f);
#endif

}

