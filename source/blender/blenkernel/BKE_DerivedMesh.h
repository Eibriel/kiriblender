/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef BKE_DERIVEDMESH_H
#define BKE_DERIVEDMESH_H

/* TODO (Probably)
 *
 *  o Make drawMapped* functions take a predicate function that
 *    determines whether to draw the edge (this predicate can
 *    also set color, etc). This will be slightly more general 
 *    and allow some of the functions to be collapsed.
 *  o Once accessor functions are added then single element draw
 *    functions can be implemented using primitive accessors.
 *  o Add function to dispatch to renderer instead of using
 *    conversion to DLM.
 */

struct MVert;
struct Object;
struct TFace;
struct EditMesh;
struct DispListMesh;
struct ModifierData;

typedef struct DerivedMesh DerivedMesh;
struct DerivedMesh {
	/* Misc. Queries */

		/* Also called in Editmode */
	int (*getNumVerts)(DerivedMesh *dm);
		/* Also called in Editmode */
	int (*getNumFaces)(DerivedMesh *dm);

		/* Iterate over each mapped vertex in the derived mesh, calling the
		 * given function with the original vert and the mapped vert's new
		 * coordinate and normal. For historical reasons the normal can be
		 * passed as a float or short array, only one should be non-NULL.
		 */
	void (*foreachMappedVert)(DerivedMesh *dm, void (*func)(void *userData, int index, float *co, float *no_f, short *no_s), void *userData);

		/* Iterate over each mapped vertex in the derived mesh, calling the
		 * given function with the original vert and the mapped edge's new
		 * coordinates.
		 */
	void (*foreachMappedEdge)(DerivedMesh *dm, void (*func)(void *userData, int index, float *v0co, float *v1co), void *userData);

		/* Iterate over each mapped face in the derived mesh, calling the
		 * given function with the original face and the mapped face's (or
		 * faces') center and normal.
		 */
	void (*foreachMappedFaceCenter)(DerivedMesh *dm, void (*func)(void *userData, int index, float *cent, float *no), void *userData);

		/* Convert to new DispListMesh, should be free'd by caller.
		 *
		 * If allowShared is true then the caller is committing to not free'ng
		 * the DerivedMesh before free'ng the DispListMesh, which means that
		 * certain fields of the returned DispListMesh can safely be share with
		 * the DerivedMesh's internal data.
		 */
	struct DispListMesh* (*convertToDispListMesh)(DerivedMesh *dm, int allowShared);

		/* Iterate over all vertex points, calling DO_MINMAX with given args.
		 *
		 * Also called in Editmode
		 */
	void (*getMinMax)(DerivedMesh *dm, float min_r[3], float max_r[3]);

	/* Direct Access Operations */
	/*  o Can be undefined */
	/*  o Must be defined for modifiers that only deform however */
			
		/* Get vertex location, undefined if index is not valid */
	void (*getVertCo)(DerivedMesh *dm, int index, float co_r[3]);

		/* Fill the array (of length .getNumVerts()) with all vertex locations  */
	void (*getVertCos)(DerivedMesh *dm, float (*cos_r)[3]);

		/* Get vertex normal, undefined if index is not valid */
	void (*getVertNo)(DerivedMesh *dm, int index, float no_r[3]);

	/* Drawing Operations */

			/* Draw all vertices as bgl points (no options) */
	void (*drawVerts)(DerivedMesh *dm);

			/* Draw edges in the UV mesh (if exists) */
	void (*drawUVEdges)(DerivedMesh *dm);

			/* Draw all edges as lines (no options) 
			 *
			 * Also called for *final* editmode DerivedMeshes
			 */
	void (*drawEdges)(DerivedMesh *dm, int drawLooseEdges);
	
			/* Draw all loose edges (edges w/ no adjoining faces) */
	void (*drawLooseEdges)(DerivedMesh *dm);

			/* Draw all faces
			 *  o Set face normal or vertex normal based on inherited face flag
			 *  o Use inherited face material index to call setMaterial
			 *  o Only if setMaterial returns true
			 *
			 * Also called for *final* editmode DerivedMeshes
			 */
	void (*drawFacesSolid)(DerivedMesh *dm, int (*setMaterial)(int));

			/* Draw all faces
			 *  o If useTwoSided, draw front and back using col arrays
			 *  o col1,col2 are arrays of length numFace*4 of 4 component colors
			 *    in ABGR format, and should be passed as per-face vertex color.
			 */
	void (*drawFacesColored)(DerivedMesh *dm, int useTwoSided, unsigned char *col1, unsigned char *col2);

			/* Draw all faces uses TFace 
			 *  o Drawing options too complicated to enumerate, look at code.
			 */
	void (*drawFacesTex)(DerivedMesh *dm, int (*setDrawOptions)(struct TFace *tf, int matnr));

			/* Draw mapped faces (no color, or texture)
			 *  o Only if !setDrawOptions or setDrawOptions(userData, mapped-face-index, drawSmooth_r) returns true
			 *
			 * If drawSmooth is set to true then vertex normals should be set and glShadeModel
			 * called with GL_SMOOTH. Otherwise the face normal should be set and glShadeModel
			 * called with GL_FLAT.
			 *
			 * The setDrawOptions is allowed to not set drawSmooth (for example, when lighting
			 * is disabled), in which case the implementation should draw as smooth shaded.
			 */
	void (*drawMappedFaces)(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index, int *drawSmooth_r), void *userData, int useColors);

			/* Draw mapped edges as lines
			 *  o Only if !setDrawOptions or setDrawOptions(userData, mapped-edge) returns true
			 */
	void (*drawMappedEdges)(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index), void *userData);

			/* Draw mapped edges as lines with interpolation values
			 *  o Only if !setDrawOptions or setDrawOptions(userData, mapped-edge, mapped-v0, mapped-v1, t) returns true
			 *
			 * NOTE: This routine is optional!
			 */
	void (*drawMappedEdgesInterp)(DerivedMesh *dm, 
									int (*setDrawOptions)(void *userData, int index), 
									void (*setDrawInterpOptions)(void *userData, int index, float t),
									void *userData);

	void (*release)(DerivedMesh *dm);
};

    /* Simple function to get me->totvert amount of vertices/normals,
       correctly deformed and subsurfered. Needed especially when vertexgroups are involved.
       In use now by vertex/weigt paint and particles */
float *mesh_get_mapped_verts_nors(struct Object *ob);

	/* Internal function, just temporarily exposed */
DerivedMesh *derivedmesh_from_displistmesh(struct DispListMesh *dlm, float (*vertexCos)[3]);

DerivedMesh *mesh_get_derived_final(struct Object *ob, int *needsFree_r);
DerivedMesh *mesh_get_derived_deform(struct Object *ob, int *needsFree_r);

DerivedMesh *mesh_create_derived_for_modifier(struct Object *ob, struct ModifierData *md);

DerivedMesh *mesh_create_derived_render(struct Object *ob);
DerivedMesh *mesh_create_derived_no_deform(struct Object *ob, float (*vertCos)[3]);
DerivedMesh *mesh_create_derived_no_deform_render(struct Object *ob, float (*vertCos)[3]);

DerivedMesh *editmesh_get_derived_base(void);
DerivedMesh *editmesh_get_derived_cage(int *needsFree_r);
DerivedMesh *editmesh_get_derived_cage_and_final(DerivedMesh **final_r, int *cageNeedsFree_r, int *finalNeedsFree_r);

#endif

