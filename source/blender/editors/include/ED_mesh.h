/**
 * $Id:
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_MESH_H
#define ED_MESH_H

struct ID;
struct View3D;
struct ARegion;
struct EditMesh;
struct EditVert;
struct EditEdge;
struct EditFace;
struct bContext;
struct wmWindowManager;
struct EditSelection;
struct ViewContext;
struct bDeformGroup;
struct MDeformWeight;
struct MDeformVert;
struct Scene;
struct Mesh;
struct MCol;
struct UvVertMap;
struct UvMapVert;
struct CustomData;
struct BMEditSelection;
struct BMesh;
struct BMVert;
struct BMEdge;
struct BMFace;

// edge and face flag both
#define EM_FGON		2
// face flag
#define EM_FGON_DRAW	1

/* editbutflag */
#define B_CLOCKWISE		1
#define B_KEEPORIG		2
#define B_BEAUTY		4
#define B_SMOOTH		8
#define B_BEAUTY_SHORT  	16
#define B_AUTOFGON		32
#define B_KNIFE			0x80
#define B_PERCENTSUBD		0x40
#define B_MESH_X_MIRROR		0x100
#define B_JOINTRIA_UV		0x200
#define B_JOINTRIA_VCOL		0X400
#define B_JOINTRIA_SHARP	0X800
#define B_JOINTRIA_MAT		0X1000

/* bmeshutils.c */

/*this function is currently defunct, dead*/
void EDBM_Tesselate(struct EditMesh *em);
void EDBM_RecalcNormals(struct BMEditMesh *em);
void EDBM_MakeEditBMesh(struct Scene *scene, struct Object *ob);
void EDBM_FreeEditBMesh(struct BMEditMesh *tm);
void EDBM_LoadEditBMesh(struct Scene *scene, struct Object *ob);
void EDBM_init_index_arrays(struct BMEditMesh *tm, int forvert, int foredge, int forface);
void EDBM_free_index_arrays(struct BMEditMesh *tm);
struct BMVert *EDBM_get_vert_for_index(struct BMEditMesh *tm, int index);
struct BMEdge *EDBM_get_edge_for_index(struct BMEditMesh *tm, int index);
struct BMFace *EDBM_get_face_for_index(struct BMEditMesh *tm, int index);
struct BMFace *EDBM_get_actFace(struct BMEditMesh *em, int sloppy);
void EDBM_selectmode_flush(struct BMEditMesh *em);
int EDBM_get_actSelection(struct BMEditMesh *em, struct BMEditSelection *ese);
void EDBM_editselection_center(struct BMEditMesh *em, float *center, struct BMEditSelection *ese);
void EDBM_editselection_plane(struct BMEditMesh *em, float *plane, struct BMEditSelection *ese);
void EDBM_selectmode_set(struct BMEditMesh *em);
void EDBM_convertsel(struct BMEditMesh *em, short oldmode, short selectmode);

int			EDBM_check_backbuf(unsigned int index);
int			EDBM_mask_init_backbuf_border(struct ViewContext *vc, short mcords[][2], short tot, short xmin, short ymin, short xmax, short ymax);
void		EDBM_free_backbuf(void);
int			EDBM_init_backbuf_border(struct ViewContext *vc, short xmin, short ymin, short xmax, short ymax);
int			EDBM_init_backbuf_circle(struct ViewContext *vc, short xs, short ys, short rads);

/* meshtools.c */

intptr_t	mesh_octree_table(struct Object *ob, struct BMEditMesh *em, float *co, char mode);
struct BMVert   *editmesh_get_x_mirror_vert(struct Object *ob, struct BMEditMesh *em, float *co);
int			mesh_get_x_mirror_vert(struct Object *ob, int index);
int			*mesh_get_x_mirror_faces(struct Object *ob, struct BMEditMesh *em);

/* mesh_ops.c */
void		ED_operatortypes_mesh(void);
void		ED_keymap_mesh(struct wmWindowManager *wm);


/* editmesh.c */

void		ED_spacetypes_init(void);
void		ED_keymap_mesh(struct wmWindowManager *wm);

struct EditMesh *make_editMesh(struct Scene *scene, Object *ob);
void		load_editMesh(struct Scene *scene, Object *ob, struct EditMesh *em);
void		remake_editMesh(struct Scene *scene, Object *ob);
void		free_editMesh(struct EditMesh *em);

void		recalc_editnormals(struct EditMesh *em);

void		EM_init_index_arrays(struct EditMesh *em, int forVert, int forEdge, int forFace);
void		EM_free_index_arrays(void);
struct EditVert	*EM_get_vert_for_index(int index);
struct EditEdge	*EM_get_edge_for_index(int index);
struct EditFace	*EM_get_face_for_index(int index);
int			EM_texFaceCheck(struct EditMesh *em);
int			EM_vertColorCheck(struct EditMesh *em);

void		undo_push_mesh(struct bContext *C, char *name);


/* editmesh_lib.c */

struct EditFace	*EM_get_actFace(struct EditMesh *em, int sloppy);
void             EM_set_actFace(struct EditMesh *em, struct EditFace *efa);
float            EM_face_area(struct EditFace *efa);
void             EM_add_data_layer(struct EditMesh *em, struct CustomData *data, int type);

void		EM_select_edge(struct EditEdge *eed, int sel);
void		EM_select_face(struct EditFace *efa, int sel);
void		EM_select_face_fgon(struct EditMesh *em, struct EditFace *efa, int val);
void		EM_select_swap(struct EditMesh *em);
void		EM_toggle_select_all(struct EditMesh *em);
void		EM_selectmode_flush(struct EditMesh *em);
void		EM_deselect_flush(struct EditMesh *em);
void		EM_selectmode_set(struct EditMesh *em);
void		EM_select_flush(struct EditMesh *em);
void		EM_convertsel(struct EditMesh *em, short oldmode, short selectmode);
void		EM_validate_selections(struct EditMesh *em);

			/* exported to transform */
int			EM_get_actSelection(struct EditMesh *em, struct EditSelection *ese);
void		EM_editselection_normal(float *normal, struct EditSelection *ese);
void		EM_editselection_plane(float *plane, struct EditSelection *ese);
void		EM_editselection_center(float *center, struct EditSelection *ese);			

struct UvVertMap *EM_make_uv_vert_map(struct EditMesh *em, int selected, int do_face_idx_array, float *limit);
struct UvMapVert *EM_get_uv_map_vert(struct UvVertMap *vmap, unsigned int v);
void              EM_free_uv_vert_map(struct UvVertMap *vmap);

/* editmesh_mods.c */
extern unsigned int bm_vertoffs, bm_solidoffs, bm_wireoffs;

void		mouse_mesh(struct bContext *C, short mval[2], short extend);
int			EM_check_backbuf(unsigned int index);
int			EM_mask_init_backbuf_border(struct ViewContext *vc, short mcords[][2], short tot, short xmin, short ymin, short xmax, short ymax);
void		EM_free_backbuf(void);
int			EM_init_backbuf_border(struct ViewContext *vc, short xmin, short ymin, short xmax, short ymax);
int			EM_init_backbuf_circle(struct ViewContext *vc, short xs, short ys, short rads);

void		EM_hide_mesh(struct EditMesh *em, int swap);
void		EM_reveal_mesh(struct EditMesh *em);

/* editface.c */
struct MTFace	*EM_get_active_mtface(struct EditMesh *em, struct EditFace **act_efa, struct MCol **mcol, int sloppy);

/* editdeform.c XXX rename functions? */

#define WEIGHT_REPLACE 1
#define WEIGHT_ADD 2
#define WEIGHT_SUBTRACT 3

void		add_defgroup (Object *ob);
void		create_dverts(struct ID *id);
float		get_vert_defgroup (Object *ob, struct bDeformGroup *dg, int vertnum);
void		remove_vert_defgroup (Object *ob, struct bDeformGroup *dg, int vertnum);
void		remove_verts_defgroup (Object *obedit, int allverts);
void		vertexgroup_select_by_name(Object *ob, char *name);
void		add_vert_to_defgroup (Object *ob, struct bDeformGroup *dg, int vertnum, 
                           float weight, int assignmode);

struct bDeformGroup		*add_defgroup_name (Object *ob, char *name);
struct MDeformWeight	*verify_defweight (struct MDeformVert *dv, int defgroup);
struct MDeformWeight	*get_defweight (struct MDeformVert *dv, int defgroup);


#endif /* ED_MESH_H */

