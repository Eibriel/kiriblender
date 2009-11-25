/**
 * A BVH for high poly meshes.
 * 
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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BLI_PBVH_H
#define BLI_PBVH_H

struct MFace;
struct MVert;
struct DMGridData;
struct PBVH;
struct PBVHNode;
struct ListBase;

typedef struct PBVH PBVH;
typedef struct PBVHNode PBVHNode;

/* Callbacks */

/* returns 1 if the search should continue from this node, 0 otherwise */
typedef int (*BLI_pbvh_SearchCallback)(PBVHNode *node, void *data);

typedef void (*BLI_pbvh_HitCallback)(PBVHNode *node, void *data);

/* Building */

PBVH *BLI_pbvh_new(void);
void BLI_pbvh_build_mesh(PBVH *bvh, struct MFace *faces, struct MVert *verts,
		    int totface, int totvert);
void BLI_pbvh_build_grids(PBVH *bvh, struct DMGridData **grids, int totgrid,
	int gridsize, void **gridfaces);
void BLI_pbvh_free(PBVH *bvh);

/* Hierarchical Search in the BVH, two methods:
   * for each hit calling a callback
   * gather nodes in an array (easy to multithread) */

void BLI_pbvh_search_callback(PBVH *bvh,
	BLI_pbvh_SearchCallback scb, void *search_data,
	BLI_pbvh_HitCallback hcb, void *hit_data);

void BLI_pbvh_search_gather(PBVH *bvh,
	BLI_pbvh_SearchCallback scb, void *search_data,
	PBVHNode ***array, int *tot);

/* Raycast
   the hit callback is called for all leaf nodes intersecting the ray;
   it's up to the callback to find the primitive within the leaves that is
   hit first */

void BLI_pbvh_raycast(PBVH *bvh, BLI_pbvh_HitCallback cb, void *data,
		      float ray_start[3], float ray_normal[3], int original);
int BLI_pbvh_node_raycast(PBVH *bvh, PBVHNode *node, float (*origco)[3],
	float ray_start[3], float ray_normal[3], float *dist);

/* Drawing */

void BLI_pbvh_node_draw(PBVHNode *node, void *data);
int BLI_pbvh_node_planes_contain_AABB(PBVHNode *node, void *data);
void BLI_pbvh_draw(PBVH *bvh, float (*planes)[4], float (*face_nors)[3]);

/* Node Access */

typedef enum {
	PBVH_Leaf = 1,

	PBVH_UpdateNormals = 2,
	PBVH_UpdateBB = 4,
	PBVH_UpdateOriginalBB = 4,
	PBVH_UpdateDrawBuffers = 8,
	PBVH_UpdateRedraw = 16
} PBVHNodeFlags;

void BLI_pbvh_node_mark_update(PBVHNode *node);

void BLI_pbvh_node_get_grids(PBVH *bvh, PBVHNode *node,
	int **grid_indices, int *totgrid, int *maxgrid, int *gridsize);
void BLI_pbvh_node_num_verts(PBVH *bvh, PBVHNode *node,
	int *uniquevert, int *totvert);

void BLI_pbvh_node_get_BB(PBVHNode *node, float bb_min[3], float bb_max[3]);
void BLI_pbvh_node_get_original_BB(PBVHNode *node, float bb_min[3], float bb_max[3]);

/* Update Normals/Bounding Box/Draw Buffers/Redraw and clear flags */

void BLI_pbvh_update(PBVH *bvh, int flags, float (*face_nors)[3]);
void BLI_pbvh_redraw_BB(PBVH *bvh, float bb_min[3], float bb_max[3]);
void BLI_pbvh_get_grid_updates(PBVH *bvh, void ***gridfaces, int *totface);

/* Vertex Iterator */

/* this iterator has quite a lot of code, but it's designed to:
   - allow the compiler to eliminate dead code and variables
   - spend most of the time in the relatively simple inner loop */

#define PBVH_ITER_ALL		0
#define PBVH_ITER_UNIQUE	1

typedef struct PBVHVertexIter {
	/* iteration */
	int g;
	int width;
	int height;
	int skip;
	int gx;
	int gy;
	int i;

	/* grid */
	struct DMGridData **grids;
	struct DMGridData *grid;
	int *grid_indices;
	int totgrid;
	int gridsize;

	/* mesh */
	struct MVert *mverts;
	int totvert;
	int *vert_indices;

	/* result: these are all computed in the macro, but we assume
	   that compiler optimizations will skip the ones we don't use */
	struct MVert *mvert;
	float *co;
	short *no;
	float *fno;
} PBVHVertexIter;

void BLI_pbvh_node_verts_iter_init(PBVH *bvh, PBVHNode *node, PBVHVertexIter *vi, int mode);

#define BLI_pbvh_vertex_iter_begin(bvh, node, vi, mode) \
	/* XXX breaks aliasing! */ \
	BLI_pbvh_node_verts_iter_init(bvh, node, &vi, mode); \
	\
	for(vi.i=0, vi.g=0; vi.g<vi.totgrid; vi.g++) { \
		if(vi.grids) { \
			vi.width= vi.gridsize; \
			vi.height= vi.gridsize; \
			vi.grid= vi.grids[vi.grid_indices[vi.g]]; \
			vi.skip= 0; \
		 	\
			/*if(mode == PVBH_ITER_UNIQUE) { \
				vi.grid += subm->grid.offset; \
				vi.skip= subm->grid.skip; \
				vi.grid -= skip; \
			}*/ \
		} \
		else { \
			vi.width= vi.totvert; \
			vi.height= 1; \
		} \
	 	\
		for(vi.gy=0; vi.gy<vi.height; vi.gy++) { \
			if(vi.grid) vi.grid += vi.skip; \
			\
			for(vi.gx=0; vi.gx<vi.width; vi.gx++, vi.i++) { \
				if(vi.grid) { \
					vi.co= vi.grid->co; \
					vi.fno= vi.grid->no; \
					vi.grid++; \
				} \
				else { \
					vi.mvert= &vi.mverts[vi.vert_indices[vi.gx]]; \
					vi.co= vi.mvert->co; \
					vi.no= vi.mvert->no; \
				} \

#define BLI_pbvh_vertex_iter_end \
			} \
		} \
	}


#endif /* BLI_PBVH_H */

