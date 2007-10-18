/*  softbody.c      
 * 
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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/*
******
variables on the UI for now

	float mediafrict;  friction to env 
	float nodemass;	  softbody mass of *vertex* 
	float grav;        softbody amount of gravitaion to apply 
	
	float goalspring;  softbody goal springs 
	float goalfrict;   softbody goal springs friction 
	float mingoal;     quick limits for goal 
	float maxgoal;

	float inspring;	  softbody inner springs 
	float infrict;     softbody inner springs friction 

*****
*/


#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

/* types */
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"	/* here is the softbody struct */
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_lattice_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_ghash.h"
#include "BKE_curve.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_object.h"
#include "BKE_softbody.h"
#include "BKE_utildefines.h"
#include "BKE_DerivedMesh.h"

#include  "BIF_editdeform.h"
#include  "BIF_graphics.h"
#include  "PIL_time.h"

/* callbacks for errors and interrupts and some goo */
static int (*SB_localInterruptCallBack)(void) = NULL;


/* ********** soft body engine ******* */



typedef struct BodyPoint {
	float origS[3], origE[3], origT[3], pos[3], vec[3], force[3];
	float goal;
	float prevpos[3], prevvec[3], prevdx[3], prevdv[3]; /* used for Heun integration */
    int nofsprings; int *springs;
	float choke;
	float colball;
	short flag;
	char octantflag;
} BodyPoint;

typedef struct BodySpring {
	int v1, v2;
	float len, strength, cf;
	float ext_force[3]; /* edges colliding and sailing */
	short order;
	short flag;
} BodySpring;

typedef struct BodyFace {
	int v1, v2, v3 ,v4;
	float ext_force[3]; /* edges colliding and sailing */
	short flag;
} BodyFace;


/*private scratch pad for caching and other data only needed when alive*/
typedef struct SBScratch {
	GHash *colliderhash;
	short needstobuildcollider;
	short flag;
	BodyFace *bodyface;
	int totface;
	float aabbmin[3],aabbmax[3];
}SBScratch;

#define SOFTGOALSNAP  0.999f 
/* if bp-> goal is above make it a *forced follow original* and skip all ODE stuff for this bp
   removes *unnecessary* stiffnes from ODE system
*/
#define HEUNWARNLIMIT 1 /* 500 would be fine i think for detecting severe *stiff* stuff */


#define BSF_INTERSECT   1 /* edge intersects collider face */
#define SBF_DOFUZZY     1 /* edge intersects collider face */
#define BFF_INTERSECT   1 /* edge intersects collider face */


float SoftHeunTol = 1.0f; /* humm .. this should be calculated from sb parameters and sizes */

/* local prototypes */
static void free_softbody_intern(SoftBody *sb);
/* aye this belongs to arith.c */
static void Vec3PlusStVec(float *v, float s, float *v1);

/*+++ frame based timing +++*/

/*physical unit of force is [kg * m / sec^2]*/

static float sb_grav_force_scale(Object *ob)
/* since unit of g is [m/sec^2] and F = mass * g we rescale unit mass of node to 1 gramm
  put it to a function here, so we can add user options later without touching simulation code
*/
{
	return (0.001f);
}

static float sb_fric_force_scale(Object *ob)
/* rescaling unit of drag [1 / sec] to somehow reasonable
  put it to a function here, so we can add user options later without touching simulation code
*/
{
	return (0.01f);
}

static float sb_time_scale(Object *ob)
/* defining the frames to *real* time relation */
{
	SoftBody *sb= ob->soft;	/* is supposed to be there */
	if (sb){
		return(sb->physics_speed); 
		/*hrms .. this could be IPO as well :) 
		 estimated range [0.001 sluggish slug - 100.0 very fast (i hope ODE solver can handle that)]
		 1 approx = a unit 1 pendulum at g = 9.8 [earth conditions]  has period 65 frames
         theory would give a 50 frames period .. so there must be something inaccurate .. looking for that (BM) 
		 */
	}
	return (1.0f);
	/* 
	this would be frames/sec independant timing assuming 25 fps is default
	but does not work very well with NLA
		return (25.0f/G.scene->r.frs_sec)
	*/
}
/*--- frame based timing ---*/

/*+++ collider caching and dicing +++*/

/********************
for each target object/face the axis aligned bounding box (AABB) is stored
faces paralell to global axes 
so only simple "value" in [min,max] ckecks are used
float operations still
*/

/* just an ID here to reduce the prob for killing objects
** ob->sumohandle points to we should not kill :)
*/ 
const int CCD_SAVETY = 190561; 

typedef struct ccdf_minmax{
float minx,miny,minz,maxx,maxy,maxz;
}ccdf_minmax;



typedef struct ccd_Mesh {
	int totvert, totface;
	MVert *mvert;
	MVert *mprevvert;
	MFace *mface;
	int savety;
	ccdf_minmax *mima;
	/* Axis Aligned Bounding Box AABB */
	float bbmin[3];
	float bbmax[3];
}ccd_Mesh;




ccd_Mesh *ccd_mesh_make(Object *ob, DerivedMesh *dm)
{
    ccd_Mesh *pccd_M = NULL;
	ccdf_minmax *mima =NULL;
	MFace *mface=NULL;
	float v[3],hull;
	int i;
	
	/* first some paranoia checks */
	if (!dm) return NULL;
	if (!dm->getNumVerts(dm) || !dm->getNumFaces(dm)) return NULL;
	
	pccd_M = MEM_mallocN(sizeof(ccd_Mesh),"ccd_Mesh");
	pccd_M->totvert = dm->getNumVerts(dm);
	pccd_M->totface = dm->getNumFaces(dm);
	pccd_M->savety  = CCD_SAVETY;
	pccd_M->bbmin[0]=pccd_M->bbmin[1]=pccd_M->bbmin[2]=1e30f;
	pccd_M->bbmax[0]=pccd_M->bbmax[1]=pccd_M->bbmax[2]=-1e30f;
	pccd_M->mprevvert=NULL;
	
	
    /* blow it up with forcefield ranges */
	hull = MAX2(ob->pd->pdef_sbift,ob->pd->pdef_sboft);
	
	/* alloc and copy verts*/
	pccd_M->mvert = dm->dupVertArray(dm);
    /* ah yeah, put the verices to global coords once */ 	
	/* and determine the ortho BB on the fly */ 
	for(i=0; i < pccd_M->totvert; i++){
		Mat4MulVecfl(ob->obmat, pccd_M->mvert[i].co);
		
        /* evaluate limits */
		VECCOPY(v,pccd_M->mvert[i].co);
		pccd_M->bbmin[0] = MIN2(pccd_M->bbmin[0],v[0]-hull);
		pccd_M->bbmin[1] = MIN2(pccd_M->bbmin[1],v[1]-hull);
		pccd_M->bbmin[2] = MIN2(pccd_M->bbmin[2],v[2]-hull);
		
		pccd_M->bbmax[0] = MAX2(pccd_M->bbmax[0],v[0]+hull);
		pccd_M->bbmax[1] = MAX2(pccd_M->bbmax[1],v[1]+hull);
		pccd_M->bbmax[2] = MAX2(pccd_M->bbmax[2],v[2]+hull);
		
	}
	/* alloc and copy faces*/
    pccd_M->mface = dm->dupFaceArray(dm);
	
	/* OBBs for idea1 */
    pccd_M->mima = MEM_mallocN(sizeof(ccdf_minmax)*pccd_M->totface,"ccd_Mesh_Faces_mima");
	mima  = pccd_M->mima;
	mface = pccd_M->mface;


	/* anyhoo we need to walk the list of faces and find OBB they live in */
	for(i=0; i < pccd_M->totface; i++){
		mima->minx=mima->miny=mima->minz=1e30f;
		mima->maxx=mima->maxy=mima->maxz=-1e30f;
		
        VECCOPY(v,pccd_M->mvert[mface->v1].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
		
        VECCOPY(v,pccd_M->mvert[mface->v2].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
		
		VECCOPY(v,pccd_M->mvert[mface->v3].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
        
		if(mface->v4){
			VECCOPY(v,pccd_M->mvert[mface->v4].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
		}

		
	mima++;
	mface++;
		
	}
	return pccd_M;
}
void ccd_mesh_update(Object *ob,ccd_Mesh *pccd_M, DerivedMesh *dm)
{
 	ccdf_minmax *mima =NULL;
	MFace *mface=NULL;
	float v[3],hull;
	int i;
	
	/* first some paranoia checks */
	if (!dm) return ;
	if (!dm->getNumVerts(dm) || !dm->getNumFaces(dm)) return ;

	if ((pccd_M->totvert != dm->getNumVerts(dm)) ||
		(pccd_M->totface != dm->getNumFaces(dm))) return;

	pccd_M->bbmin[0]=pccd_M->bbmin[1]=pccd_M->bbmin[2]=1e30f;
	pccd_M->bbmax[0]=pccd_M->bbmax[1]=pccd_M->bbmax[2]=-1e30f;
	
	
    /* blow it up with forcefield ranges */
	hull = MAX2(ob->pd->pdef_sbift,ob->pd->pdef_sboft);
	
	/* rotate current to previous */
	if(pccd_M->mprevvert) MEM_freeN(pccd_M->mprevvert);
    pccd_M->mprevvert = pccd_M->mvert;
	/* alloc and copy verts*/
    pccd_M->mvert = dm->dupVertArray(dm);
    /* ah yeah, put the verices to global coords once */ 	
	/* and determine the ortho BB on the fly */ 
	for(i=0; i < pccd_M->totvert; i++){
		Mat4MulVecfl(ob->obmat, pccd_M->mvert[i].co);
		
        /* evaluate limits */
		VECCOPY(v,pccd_M->mvert[i].co);
		pccd_M->bbmin[0] = MIN2(pccd_M->bbmin[0],v[0]-hull);
		pccd_M->bbmin[1] = MIN2(pccd_M->bbmin[1],v[1]-hull);
		pccd_M->bbmin[2] = MIN2(pccd_M->bbmin[2],v[2]-hull);
		
		pccd_M->bbmax[0] = MAX2(pccd_M->bbmax[0],v[0]+hull);
		pccd_M->bbmax[1] = MAX2(pccd_M->bbmax[1],v[1]+hull);
		pccd_M->bbmax[2] = MAX2(pccd_M->bbmax[2],v[2]+hull);

        /* evaluate limits */
		VECCOPY(v,pccd_M->mprevvert[i].co);
		pccd_M->bbmin[0] = MIN2(pccd_M->bbmin[0],v[0]-hull);
		pccd_M->bbmin[1] = MIN2(pccd_M->bbmin[1],v[1]-hull);
		pccd_M->bbmin[2] = MIN2(pccd_M->bbmin[2],v[2]-hull);
		
		pccd_M->bbmax[0] = MAX2(pccd_M->bbmax[0],v[0]+hull);
		pccd_M->bbmax[1] = MAX2(pccd_M->bbmax[1],v[1]+hull);
		pccd_M->bbmax[2] = MAX2(pccd_M->bbmax[2],v[2]+hull);
		
	}
	
	mima  = pccd_M->mima;
	mface = pccd_M->mface;


	/* anyhoo we need to walk the list of faces and find OBB they live in */
	for(i=0; i < pccd_M->totface; i++){
		mima->minx=mima->miny=mima->minz=1e30f;
		mima->maxx=mima->maxy=mima->maxz=-1e30f;
		
        VECCOPY(v,pccd_M->mvert[mface->v1].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
		
        VECCOPY(v,pccd_M->mvert[mface->v2].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
		
		VECCOPY(v,pccd_M->mvert[mface->v3].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
        
		if(mface->v4){
			VECCOPY(v,pccd_M->mvert[mface->v4].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
		}


        VECCOPY(v,pccd_M->mprevvert[mface->v1].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
		
        VECCOPY(v,pccd_M->mprevvert[mface->v2].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
		
		VECCOPY(v,pccd_M->mprevvert[mface->v3].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
        
		if(mface->v4){
			VECCOPY(v,pccd_M->mprevvert[mface->v4].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
		}

		
	mima++;
	mface++;
		
	}
	return ;
}

void ccd_mesh_free(ccd_Mesh *ccdm)
{
	if(ccdm && (ccdm->savety == CCD_SAVETY )){ /*make sure we're not nuking objects we don't know*/
		MEM_freeN(ccdm->mface);
		MEM_freeN(ccdm->mvert);
		if (ccdm->mprevvert) MEM_freeN(ccdm->mprevvert);
		MEM_freeN(ccdm->mima);
		MEM_freeN(ccdm);
		ccdm = NULL;
	}
}

void ccd_build_deflector_hache(Object *vertexowner,GHash *hash)
{
	Base *base;
	Object *ob;
	base= G.scene->base.first;
	base= G.scene->base.first;
	if (!hash) return;
	while (base) {
		/*Only proceed for mesh object in same layer */
		if(base->object->type==OB_MESH && (base->lay & vertexowner->lay)) {
			ob= base->object;
			if((vertexowner) && (ob == vertexowner)){ 
				/* if vertexowner is given  we don't want to check collision with owner object */ 
				base = base->next;
				continue;				
			}

			/*+++ only with deflecting set */
			if(ob->pd && ob->pd->deflect && BLI_ghash_lookup(hash, ob) == 0) {
				DerivedMesh *dm= NULL;
				
				if(ob->softflag & OB_SB_COLLFINAL) { /* so maybe someone wants overkill to collide with subsurfed */
					dm = mesh_get_derived_final(ob, CD_MASK_BAREMESH);
				} else {
					dm = mesh_get_derived_deform(ob, CD_MASK_BAREMESH);
				}

				if(dm){
					ccd_Mesh *ccdmesh = ccd_mesh_make(ob, dm);
					BLI_ghash_insert(hash, ob, ccdmesh);

					/* we did copy & modify all we need so give 'em away again */
					dm->release(dm);
					
				}
			}/*--- only with deflecting set */

		}/* mesh && layer*/		
	   base = base->next;
	} /* while (base) */
}

void ccd_update_deflector_hache(Object *vertexowner,GHash *hash)
{
	Base *base;
	Object *ob;
	base= G.scene->base.first;
	base= G.scene->base.first;
	if ((!hash) || (!vertexowner)) return;
	while (base) {
		/*Only proceed for mesh object in same layer */
		if(base->object->type==OB_MESH && (base->lay & vertexowner->lay)) {
			ob= base->object;
			if(ob == vertexowner){ 
				/* if vertexowner is given  we don't want to check collision with owner object */ 
				base = base->next;
				continue;				
			}

			/*+++ only with deflecting set */
			if(ob->pd && ob->pd->deflect) {
				DerivedMesh *dm= NULL;
				
				if(ob->softflag & OB_SB_COLLFINAL) { /* so maybe someone wants overkill to collide with subsurfed */
					dm = mesh_get_derived_final(ob, CD_MASK_BAREMESH);
				} else {
					dm = mesh_get_derived_deform(ob, CD_MASK_BAREMESH);
				}
				if(dm){
					ccd_Mesh *ccdmesh = BLI_ghash_lookup(hash,ob);
					if (ccdmesh)
						ccd_mesh_update(ob,ccdmesh,dm);

					/* we did copy & modify all we need so give 'em away again */
					dm->release(dm);
				}
			}/*--- only with deflecting set */

		}/* mesh && layer*/		
	   base = base->next;
	} /* while (base) */
}




/*--- collider caching and dicing ---*/


static int count_mesh_quads(Mesh *me)
{
	int a,result = 0;
	MFace *mface= me->mface;
	
	if(mface) {
		for(a=me->totface; a>0; a--, mface++) {
			if(mface->v4) result++;
		}
	}	
	return result;
}

static void add_mesh_quad_diag_springs(Object *ob)
{
	Mesh *me= ob->data;
	MFace *mface= me->mface;
	BodyPoint *bp;
	BodySpring *bs, *bs_new;
	int a ;
	
	if (ob->soft){
		int nofquads;
		
		nofquads = count_mesh_quads(me);
		if (nofquads) {
			/* resize spring-array to hold additional quad springs */
			bs_new= MEM_callocN( (ob->soft->totspring + nofquads *2 )*sizeof(BodySpring), "bodyspring");
			memcpy(bs_new,ob->soft->bspring,(ob->soft->totspring )*sizeof(BodySpring));
			
			if(ob->soft->bspring)
				MEM_freeN(ob->soft->bspring); /* do this before reassigning the pointer  or have a 1st class memory leak */
			ob->soft->bspring = bs_new; 
			
			/* fill the tail */
			a = 0;
			bs = bs_new+ob->soft->totspring;
			bp= ob->soft->bpoint;
			if(mface ) {
				for(a=me->totface; a>0; a--, mface++) {
					if(mface->v4) {
						bs->v1= mface->v1;
						bs->v2= mface->v3;
						bs->strength= 1.0;
						bs->order   =2;
						bs++;
						bs->v1= mface->v2;
						bs->v2= mface->v4;
						bs->strength= 1.0;
						bs->order   =2;
						bs++;
						
					}
				}	
			}
			
            /* now we can announce new springs */
			ob->soft->totspring += nofquads *2;
		}
	}
}

static void add_2nd_order_roller(Object *ob,float stiffness,int *counter, int addsprings)
{
	/*assume we have a softbody*/
	SoftBody *sb= ob->soft;	/* is supposed to be there */
	BodyPoint *bp,*bpo;	
	BodySpring *bs,*bs2,*bs3= NULL;
	int a,b,c,notthis= 0,v0;
	if (!sb->bspring){return;} /* we are 2nd order here so 1rst should have been build :) */
	/* first run counting  second run adding */
	*counter = 0;
	if (addsprings) bs3 = ob->soft->bspring+ob->soft->totspring;
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		/*scan for neighborhood*/
		bpo = NULL;
		v0  = (sb->totpoint-a);
		for(b=bp->nofsprings;b>0;b--){
			bs = sb->bspring + bp->springs[b-1];
			/*nasty thing here that springs have two ends
			so here we have to make sure we examine the other */
			if (( v0 == bs->v1) ){ 
				bpo =sb->bpoint+bs->v2;
				notthis = bs->v2;
			}
			else {
			if (( v0 == bs->v2) ){
				bpo =sb->bpoint+bs->v1;
				notthis = bs->v1;
			} 
			else {printf("oops we should not get here -  add_2nd_order_springs");}
			}
            if (bpo){/* so now we have a 2nd order humpdidump */
				for(c=bpo->nofsprings;c>0;c--){
					bs2 = sb->bspring + bpo->springs[c-1];
					if ((bs2->v1 != notthis)  && (bs2->v1 > v0)){
						(*counter)++;/*hit */
						if (addsprings){
							bs3->v1= v0;
							bs3->v2= bs2->v1;
							bs3->strength= stiffness;
							bs3->order=2;
							bs3++;
						}
					}
					if ((bs2->v2 !=notthis)&&(bs2->v2 > v0)){
					(*counter)++;/*hit */
						if (addsprings){
							bs3->v1= v0;
							bs3->v2= bs2->v2;
							bs3->strength= stiffness;
							bs3->order=2;
							bs3++;
						}

					}
				}
				
			}
			
		}
		/*scan for neighborhood done*/
	}
}


static void add_2nd_order_springs(Object *ob,float stiffness)
{
	int counter = 0;
	BodySpring *bs_new;
	
	add_2nd_order_roller(ob,stiffness,&counter,0); /* counting */
	if (counter) {
		/* resize spring-array to hold additional springs */
		bs_new= MEM_callocN( (ob->soft->totspring + counter )*sizeof(BodySpring), "bodyspring");
		memcpy(bs_new,ob->soft->bspring,(ob->soft->totspring )*sizeof(BodySpring));
		
		if(ob->soft->bspring)
			MEM_freeN(ob->soft->bspring); 
		ob->soft->bspring = bs_new; 
		
		add_2nd_order_roller(ob,stiffness,&counter,1); /* adding */
		ob->soft->totspring +=counter ;
	}
}

static void add_bp_springlist(BodyPoint *bp,int springID)
{
	int *newlist;
	
	if (bp->springs == NULL) {
		bp->springs = MEM_callocN( sizeof(int), "bpsprings");
		bp->springs[0] = springID;
		bp->nofsprings = 1;
	}
	else {
		bp->nofsprings++;
		newlist = MEM_callocN(bp->nofsprings * sizeof(int), "bpsprings");
		memcpy(newlist,bp->springs,(bp->nofsprings-1)* sizeof(int));
		MEM_freeN(bp->springs);
		bp->springs = newlist;
		bp->springs[bp->nofsprings-1] = springID;
	}
}

/* do this once when sb is build
it is O(N^2) so scanning for springs every iteration is too expensive
*/
static void build_bps_springlist(Object *ob)
{
	SoftBody *sb= ob->soft;	/* is supposed to be there */
	BodyPoint *bp;	
	BodySpring *bs;	
	int a,b;
	
	if (sb==NULL) return; /* paranoya check */
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		/* throw away old list */
		if (bp->springs) {
			MEM_freeN(bp->springs);
			bp->springs=NULL;
		}
		/* scan for attached inner springs */	
		for(b=sb->totspring, bs= sb->bspring; b>0; b--, bs++) {
			if (( (sb->totpoint-a) == bs->v1) ){ 
				add_bp_springlist(bp,sb->totspring -b);
			}
			if (( (sb->totpoint-a) == bs->v2) ){ 
				add_bp_springlist(bp,sb->totspring -b);
			}
		}/*for springs*/
	}/*for bp*/		
}

static void calculate_collision_balls(Object *ob)
{
	SoftBody *sb= ob->soft;	/* is supposed to be there */
	BodyPoint *bp;	
	BodySpring *bs;	
	int a,b,akku_count;
	float min,max,akku;

	if (sb==NULL) return; /* paranoya check */

	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		bp->colball=0;
		akku =0.0f;
		akku_count=0;
		min = 1e22f;
		max = -1e22f;
		/* first estimation based on attached */
		for(b=bp->nofsprings;b>0;b--){
			bs = sb->bspring + bp->springs[b-1];
			if (bs->order == 1){
			akku += bs->len;
			akku_count++,
			min = MIN2(bs->len,min);
			max = MAX2(bs->len,max);
			}
		}

		if (akku_count > 0) {
			if (sb->sbc_mode == 0){
				bp->colball=sb->colball;
			}		
			if (sb->sbc_mode == 1){
			 bp->colball = akku/(float)akku_count*sb->colball;
			}
			if (sb->colball == 2){
				bp->colball=min*sb->colball;
			}		
			if (sb->colball == 3){
				bp->colball=max*sb->colball;
			}		
			if (sb->colball == 4){
			bp->colball = (min + max)/2.0f*sb->colball;
			}		
		}
		else bp->colball=0;
	}/*for bp*/		
}


char set_octant_flags(float *ce, float *pos, float ball)
{ 
	float x,y,z;
	char res = 0;
	int a;
	
	for (a=0;a<7;a++){
		switch(a){
		case 0: x=pos[0];      y=pos[1];      z=pos[2]; break;
		case 1: x=pos[0]+ball; y=pos[1];      z=pos[2]; break;
		case 2: x=pos[0]-ball; y=pos[1];      z=pos[2]; break;
		case 3: x=pos[0];      y=pos[1]+ball; z=pos[2]; break;
		case 4: x=pos[0];      y=pos[1]-ball; z=pos[2]; break;
		case 5: x=pos[0];      y=pos[1];      z=pos[2]+ball; break;
		case 6: x=pos[0];      y=pos[1];      z=pos[2]-ball; break;
		}

		x=pos[0]; y=pos[1]; z=pos[2];
		
		if (x > ce[0]){
			if (y > ce[1]){
				if (z > ce[2])  res|= 1;
				else res|= 2;
			}
			else{
				if (z > ce[2])  res|= 4;
				else res|= 8;
			}
		}
		
		else{
			if (y > ce[1]){
				if (z > ce[2])  res|= 16;
				else res|= 32;
			}
			else{
				if (z > ce[2])  res|= 64;
				else res|= 128;
			}
		}
	}
	return res;
}

/* creates new softbody if didn't exist yet, makes new points and springs arrays */
static void renew_softbody(Object *ob, int totpoint, int totspring)  
{
	SoftBody *sb;
	int i;
	
	if(ob->soft==NULL) ob->soft= sbNew();
	else free_softbody_intern(ob->soft);
	sb= ob->soft;
	   
	if(totpoint) {
		sb->totpoint= totpoint;
		sb->totspring= totspring;
		
		sb->bpoint= MEM_mallocN( totpoint*sizeof(BodyPoint), "bodypoint");
		if(totspring) 
			sb->bspring= MEM_mallocN( totspring*sizeof(BodySpring), "bodyspring");

			/* initialise BodyPoint array */
		for (i=0; i<totpoint; i++) {
			BodyPoint *bp = &sb->bpoint[i];

			if(ob->softflag & OB_SB_GOAL) {
				bp->goal= ob->soft->defgoal;
			}
			else { 
				bp->goal= 0.0f; 
				/* so this will definily be below SOFTGOALSNAP */
			}
			
			bp->nofsprings= 0;
			bp->springs= NULL;
			bp->choke = 0.0f;
			bp->colball = 0.0f;
			bp->flag = 0;

		}
	}
}

static void free_softbody_baked(SoftBody *sb)
{
	SBVertex *key;
	int k;

	for(k=0; k<sb->totkey; k++) {
		key= *(sb->keys + k);
		if(key) MEM_freeN(key);
	}
	if(sb->keys) MEM_freeN(sb->keys);
	
	sb->keys= NULL;
	sb->totkey= 0;
	
}
static void free_scratch(SoftBody *sb)
{
	if(sb->scratch){
		/* todo make sure everything is cleaned up nicly */
		if (sb->scratch->colliderhash){
			BLI_ghash_free(sb->scratch->colliderhash, NULL,
					(GHashValFreeFP) ccd_mesh_free); /*this hoepfully will free all caches*/
			sb->scratch->colliderhash = NULL;
		}
		if (sb->scratch->bodyface){
			MEM_freeN(sb->scratch->bodyface);
		}
		MEM_freeN(sb->scratch);
		sb->scratch = NULL;
	}
	
}

/* only frees internal data */
static void free_softbody_intern(SoftBody *sb)
{
	if(sb) {
		int a;
		BodyPoint *bp;
		
		if(sb->bpoint){
			for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
				/* free spring list */ 
				if (bp->springs != NULL) {
					MEM_freeN(bp->springs);
				}
			}
			MEM_freeN(sb->bpoint);
		}
		
		if(sb->bspring) MEM_freeN(sb->bspring);
		
		sb->totpoint= sb->totspring= 0;
		sb->bpoint= NULL;
		sb->bspring= NULL;

		free_scratch(sb);
		free_softbody_baked(sb);
	}
}


/* ************ dynamics ********** */

/* the most general (micro physics correct) way to do collision 
** (only needs the current particle position)  
**
** it actually checks if the particle intrudes a short range force field generated 
** by the faces of the target object and returns a force to drive the particel out
** the strenght of the field grows exponetially if the particle is on the 'wrong' side of the face
** 'wrong' side : projection to the face normal is negative (all referred to a vertex in the face)
**
** flaw of this: 'fast' particles as well as 'fast' colliding faces 
** give a 'tunnel' effect such that the particle passes through the force field 
** without ever 'seeing' it 
** this is fully compliant to heisenberg: h >= fuzzy(location) * fuzzy(time)
** besides our h is way larger than in QM because forces propagate way slower here
** we have to deal with fuzzy(time) in the range of 1/25 seconds (typical frame rate)
** yup collision targets are not known here any better 
** and 1/25 second is looong compared to real collision events
** Q: why not use 'simple' collision here like bouncing back a particle 
**   --> reverting is velocity on the face normal
** A: because our particles are not alone here 
**    and need to tell their neighbours exactly what happens via spring forces 
** unless sbObjectStep( .. ) is called on sub frame timing level
** BTW that also questions the use of a 'implicit' solvers on softbodies  
** since that would only valid for 'slow' moving collision targets and dito particles
*/

/* aye this belongs to arith.c */
static void Vec3PlusStVec(float *v, float s, float *v1)
{
	v[0] += s*v1[0];
	v[1] += s*v1[1];
	v[2] += s*v1[2];
}

/* +++ dependancy information functions*/
static int are_there_deflectors(unsigned int layer)
{
	Base *base;
	
	for(base = G.scene->base.first; base; base= base->next) {
		if( (base->lay & layer) && base->object->pd) {
			if(base->object->pd->deflect) 
				return 1;
		}
	}
	return 0;
}

static int query_external_colliders(Object *me)
{
	return(are_there_deflectors(me->lay));
}

#if 0
static int query_external_forces(Object *me)
{
/* silly but true: we need to create effector cache to see if anything is in it */
	ListBase *ec = pdInitEffectors(me,NULL);
	int result = 0;
	if (ec){
		result = 1;
		pdEndEffectors(ec); /* sorry ec, yes i'm an idiot, but i needed to know if you were there */
	}
	return result;
}

/* 
any of that external objects may have an IPO or something alike ..
so unless we can ask them if they are moving we have to assume they do
*/
static int query_external_time(Object *me)
{
	if (query_external_colliders(me)) return 1;
	if (query_external_forces(me)) return 1;
	return 0;
}
static int query_internal_time(Object *me)
{
	if (me->softflag & OB_SB_GOAL) return 1;
	return 0;
}
#endif
/* --- dependancy information functions*/

/* +++ the aabb "force" section*/
int sb_detect_aabb_collisionCached(	float force[3], unsigned int par_layer,struct Object *vertexowner,float time)
{
	Object *ob;
	SoftBody *sb=vertexowner->soft;
	GHash *hash;
	GHashIterator *ihash;
	float  aabbmin[3],aabbmax[3];
	int a, deflected=0;

	if ((sb == NULL) || (sb->scratch ==NULL)) return 0;
	VECCOPY(aabbmin,sb->scratch->aabbmin);
	VECCOPY(aabbmax,sb->scratch->aabbmax);

	hash  = vertexowner->soft->scratch->colliderhash;
	ihash =	BLI_ghashIterator_new(hash);
    while (!BLI_ghashIterator_isDone(ihash) ) {

		ccd_Mesh *ccdm = BLI_ghashIterator_getValue	(ihash);
		ob             = BLI_ghashIterator_getKey	(ihash);
			/* only with deflecting set */
			if(ob->pd && ob->pd->deflect) {
				MFace *mface= NULL;
				MVert *mvert= NULL;
				MVert *mprevvert= NULL;
				ccdf_minmax *mima= NULL;
				if(ccdm){
					mface= ccdm->mface;
					mvert= ccdm->mvert;
					mprevvert= ccdm->mprevvert;
					mima= ccdm->mima;
					a = ccdm->totface;
					
					if ((aabbmax[0] < ccdm->bbmin[0]) || 
						(aabbmax[1] < ccdm->bbmin[1]) ||
						(aabbmax[2] < ccdm->bbmin[2]) ||
						(aabbmin[0] > ccdm->bbmax[0]) || 
						(aabbmin[1] > ccdm->bbmax[1]) || 
						(aabbmin[2] > ccdm->bbmax[2]) ) {
						/* boxes dont intersect */ 
						BLI_ghashIterator_step(ihash);
						continue;				
					}					

					/* so now we have the 2 boxes overlapping */
                    /* forces actually not used */
					deflected = 2;

				}
				else{
					/*aye that should be cached*/
					printf("missing cache error \n");
					BLI_ghashIterator_step(ihash);
					continue;				
				}
			} /* if(ob->pd && ob->pd->deflect) */
			BLI_ghashIterator_step(ihash);
	} /* while () */
	BLI_ghashIterator_free(ihash);
	return deflected;	
}
/* --- the aabb section*/


/* +++ the face external section*/

int sb_detect_face_collisionCached(float face_v1[3],float face_v2[3],float face_v3[3],float *damp,						
								   float force[3], unsigned int par_layer,struct Object *vertexowner,float time)
{
	Object *ob;
	GHash *hash;
	GHashIterator *ihash;
	float nv1[3], nv2[3], nv3[3], nv4[3], edge1[3], edge2[3], d_nvect[3], aabbmin[3],aabbmax[3];
	float t;
	int a, deflected=0;

	aabbmin[0] = MIN3(face_v1[0],face_v2[0],face_v3[0]);
	aabbmin[1] = MIN3(face_v1[1],face_v2[1],face_v3[1]);
	aabbmin[2] = MIN3(face_v1[2],face_v2[2],face_v3[2]);
	aabbmax[0] = MAX3(face_v1[0],face_v2[0],face_v3[0]);
	aabbmax[1] = MAX3(face_v1[1],face_v2[1],face_v3[1]);
	aabbmax[2] = MAX3(face_v1[2],face_v2[2],face_v3[2]);

	hash  = vertexowner->soft->scratch->colliderhash;
	ihash =	BLI_ghashIterator_new(hash);
    while (!BLI_ghashIterator_isDone(ihash) ) {

		ccd_Mesh *ccdm = BLI_ghashIterator_getValue	(ihash);
		ob             = BLI_ghashIterator_getKey	(ihash);
			/* only with deflecting set */
			if(ob->pd && ob->pd->deflect) {
				MFace *mface= NULL;
				MVert *mvert= NULL;
				MVert *mprevvert= NULL;
				ccdf_minmax *mima= NULL;
				if(ccdm){
					mface= ccdm->mface;
					mvert= ccdm->mvert;
					mprevvert= ccdm->mprevvert;
					mima= ccdm->mima;
					a = ccdm->totface;
					
					if ((aabbmax[0] < ccdm->bbmin[0]) || 
						(aabbmax[1] < ccdm->bbmin[1]) ||
						(aabbmax[2] < ccdm->bbmin[2]) ||
						(aabbmin[0] > ccdm->bbmax[0]) || 
						(aabbmin[1] > ccdm->bbmax[1]) || 
						(aabbmin[2] > ccdm->bbmax[2]) ) {
						/* boxes dont intersect */ 
						BLI_ghashIterator_step(ihash);
						continue;				
					}					

				}
				else{
					/*aye that should be cached*/
					printf("missing cache error \n");
					BLI_ghashIterator_step(ihash);
					continue;				
				}


				/* use mesh*/
				while (a--) {
					if (
						(aabbmax[0] < mima->minx) || 
						(aabbmin[0] > mima->maxx) || 
						(aabbmax[1] < mima->miny) ||
						(aabbmin[1] > mima->maxy) || 
						(aabbmax[2] < mima->minz) ||
						(aabbmin[2] > mima->maxz) 
						) {
						mface++;
						mima++;
						continue;
					}


					if (mvert){

						VECCOPY(nv1,mvert[mface->v1].co);						
						VECCOPY(nv2,mvert[mface->v2].co);
						VECCOPY(nv3,mvert[mface->v3].co);
						if (mface->v4){
							VECCOPY(nv4,mvert[mface->v4].co);
						}
						if (mprevvert){
							VecMulf(nv1,time);
							Vec3PlusStVec(nv1,(1.0f-time),mprevvert[mface->v1].co);
							
							VecMulf(nv2,time);
							Vec3PlusStVec(nv2,(1.0f-time),mprevvert[mface->v2].co);
							
							VecMulf(nv3,time);
							Vec3PlusStVec(nv3,(1.0f-time),mprevvert[mface->v3].co);
							
							if (mface->v4){
								VecMulf(nv4,time);
								Vec3PlusStVec(nv4,(1.0f-time),mprevvert[mface->v4].co);
							}
						}	
					}

					/* switch origin to be nv2*/
					VECSUB(edge1, nv1, nv2);
					VECSUB(edge2, nv3, nv2);
					Crossf(d_nvect, edge2, edge1);
					Normalize(d_nvect);
					if ( 
						LineIntersectsTriangle(nv1, nv2, face_v1, face_v2, face_v3, &t) ||
						LineIntersectsTriangle(nv2, nv3, face_v1, face_v2, face_v3, &t) ||
						LineIntersectsTriangle(nv3, nv1, face_v1, face_v2, face_v3, &t) ){
						Vec3PlusStVec(force,-1.0f,d_nvect);
						*damp=ob->pd->pdef_sbdamp;
						deflected = 2;
					}
					if (mface->v4){ /* quad */
						/* switch origin to be nv4 */
						VECSUB(edge1, nv3, nv4);
						VECSUB(edge2, nv1, nv4);					
						Crossf(d_nvect, edge2, edge1);
						Normalize(d_nvect);	
						if ( 
							LineIntersectsTriangle(nv1, nv3, face_v1, face_v2, face_v3, &t) ||
							LineIntersectsTriangle(nv3, nv4, face_v1, face_v2, face_v3, &t) ||
							LineIntersectsTriangle(nv4, nv1, face_v1, face_v2, face_v3, &t) ){
							Vec3PlusStVec(force,-1.0f,d_nvect);
							*damp=ob->pd->pdef_sbdamp;
							deflected = 2;
						}
					}
					mface++;
					mima++;					
				}/* while a */		
			} /* if(ob->pd && ob->pd->deflect) */
			BLI_ghashIterator_step(ihash);
	} /* while () */
	BLI_ghashIterator_free(ihash);
	return deflected;	
}



void scan_for_ext_face_forces(Object *ob,float timenow)
{
	SoftBody *sb = ob->soft;
	BodyFace *bf;
	int a;
	float damp; 
	float tune = -10.0f;
	float feedback[3];
	
	if (sb && sb->scratch->totface){
		
		
		bf = sb->scratch->bodyface;
		for(a=0; a<sb->scratch->totface; a++, bf++) {
			bf->ext_force[0]=bf->ext_force[1]=bf->ext_force[2]=0.0f; 
			feedback[0]=feedback[1]=feedback[2]=0.0f;
			bf->flag &= ~BFF_INTERSECT;
			
			if (sb_detect_face_collisionCached(sb->bpoint[bf->v1].pos,sb->bpoint[bf->v2].pos, sb->bpoint[bf->v3].pos, 
				&damp,	feedback, ob->lay ,ob , timenow)){
				Vec3PlusStVec(bf->ext_force,tune,feedback);
				bf->flag |= BFF_INTERSECT;
			}

			feedback[0]=feedback[1]=feedback[2]=0.0f;
			if ((bf->v4) && (sb_detect_face_collisionCached(sb->bpoint[bf->v1].pos,sb->bpoint[bf->v3].pos, sb->bpoint[bf->v4].pos, 
				&damp,	feedback, ob->lay ,ob , timenow))){
				Vec3PlusStVec(bf->ext_force,tune,feedback);
				bf->flag |= BFF_INTERSECT;	
			}
		}

		
		
		bf = sb->scratch->bodyface;
		for(a=0; a<sb->scratch->totface; a++, bf++) {
			if ( bf->flag & BFF_INTERSECT)
			{
				VECADD(sb->bpoint[bf->v1].force,sb->bpoint[bf->v1].force,bf->ext_force); 
				VECADD(sb->bpoint[bf->v2].force,sb->bpoint[bf->v2].force,bf->ext_force);
				VECADD(sb->bpoint[bf->v3].force,sb->bpoint[bf->v3].force,bf->ext_force); 
				if (bf->v4){
				VECADD(sb->bpoint[bf->v4].force,sb->bpoint[bf->v4].force,bf->ext_force);
				}

			}
			
		}




	}
}

/*  --- the face external section*/


/* +++ the spring external section*/

int sb_detect_edge_collisionCached(float edge_v1[3],float edge_v2[3],float *damp,						
								   float force[3], unsigned int par_layer,struct Object *vertexowner,float time)
{
	Object *ob;
	GHash *hash;
	GHashIterator *ihash;
	float nv1[3], nv2[3], nv3[3], nv4[3], edge1[3], edge2[3], d_nvect[3], aabbmin[3],aabbmax[3];
	float t,el;
	int a, deflected=0;

	aabbmin[0] = MIN2(edge_v1[0],edge_v2[0]);
	aabbmin[1] = MIN2(edge_v1[1],edge_v2[1]);
	aabbmin[2] = MIN2(edge_v1[2],edge_v2[2]);
	aabbmax[0] = MAX2(edge_v1[0],edge_v2[0]);
	aabbmax[1] = MAX2(edge_v1[1],edge_v2[1]);
	aabbmax[2] = MAX2(edge_v1[2],edge_v2[2]);

	el = VecLenf(edge_v1,edge_v2);

	hash  = vertexowner->soft->scratch->colliderhash;
	ihash =	BLI_ghashIterator_new(hash);
    while (!BLI_ghashIterator_isDone(ihash) ) {

		ccd_Mesh *ccdm = BLI_ghashIterator_getValue	(ihash);
		ob             = BLI_ghashIterator_getKey	(ihash);
			/* only with deflecting set */
			if(ob->pd && ob->pd->deflect) {
				MFace *mface= NULL;
				MVert *mvert= NULL;
				MVert *mprevvert= NULL;
				ccdf_minmax *mima= NULL;
				if(ccdm){
					mface= ccdm->mface;
					mvert= ccdm->mvert;
					mprevvert= ccdm->mprevvert;
					mima= ccdm->mima;
					a = ccdm->totface;
					
					if ((aabbmax[0] < ccdm->bbmin[0]) || 
						(aabbmax[1] < ccdm->bbmin[1]) ||
						(aabbmax[2] < ccdm->bbmin[2]) ||
						(aabbmin[0] > ccdm->bbmax[0]) || 
						(aabbmin[1] > ccdm->bbmax[1]) || 
						(aabbmin[2] > ccdm->bbmax[2]) ) {
						/* boxes dont intersect */ 
						BLI_ghashIterator_step(ihash);
						continue;				
					}					

				}
				else{
					/*aye that should be cached*/
					printf("missing cache error \n");
					BLI_ghashIterator_step(ihash);
					continue;				
				}


				/* use mesh*/
				while (a--) {
					if (
						(aabbmax[0] < mima->minx) || 
						(aabbmin[0] > mima->maxx) || 
						(aabbmax[1] < mima->miny) ||
						(aabbmin[1] > mima->maxy) || 
						(aabbmax[2] < mima->minz) ||
						(aabbmin[2] > mima->maxz) 
						) {
						mface++;
						mima++;
						continue;
					}


					if (mvert){

						VECCOPY(nv1,mvert[mface->v1].co);						
						VECCOPY(nv2,mvert[mface->v2].co);
						VECCOPY(nv3,mvert[mface->v3].co);
						if (mface->v4){
							VECCOPY(nv4,mvert[mface->v4].co);
						}
						if (mprevvert){
							VecMulf(nv1,time);
							Vec3PlusStVec(nv1,(1.0f-time),mprevvert[mface->v1].co);
							
							VecMulf(nv2,time);
							Vec3PlusStVec(nv2,(1.0f-time),mprevvert[mface->v2].co);
							
							VecMulf(nv3,time);
							Vec3PlusStVec(nv3,(1.0f-time),mprevvert[mface->v3].co);
							
							if (mface->v4){
								VecMulf(nv4,time);
								Vec3PlusStVec(nv4,(1.0f-time),mprevvert[mface->v4].co);
							}
						}	
					}

					/* switch origin to be nv2*/
					VECSUB(edge1, nv1, nv2);
					VECSUB(edge2, nv3, nv2);

					Crossf(d_nvect, edge2, edge1);
					Normalize(d_nvect);
					if ( LineIntersectsTriangle(edge_v1, edge_v2, nv1, nv2, nv3, &t)){
						float v1[3],v2[3];
						float intrusiondepth,i1,i2;
						VECSUB(v1, edge_v1, nv2);
						VECSUB(v2, edge_v2, nv2);
						i1 = Inpf(v1,d_nvect);
						i2 = Inpf(v2,d_nvect);
						intrusiondepth = -MIN2(i1,i2)/el;
						Vec3PlusStVec(force,intrusiondepth,d_nvect);
						*damp=ob->pd->pdef_sbdamp;
						deflected = 2;
					}
					if (mface->v4){ /* quad */
						/* switch origin to be nv4 */
						VECSUB(edge1, nv3, nv4);
						VECSUB(edge2, nv1, nv4);

						Crossf(d_nvect, edge2, edge1);
						Normalize(d_nvect);						
						if (LineIntersectsTriangle( edge_v1, edge_v2,nv1, nv3, nv4, &t)){
							float v1[3],v2[3];
							float intrusiondepth,i1,i2;
							VECSUB(v1, edge_v1, nv4);
							VECSUB(v2, edge_v2, nv4);
						i1 = Inpf(v1,d_nvect);
						i2 = Inpf(v2,d_nvect);
						intrusiondepth = -MIN2(i1,i2)/el;


							Vec3PlusStVec(force,intrusiondepth,d_nvect);
							*damp=ob->pd->pdef_sbdamp;
							deflected = 2;
						}
					}
					mface++;
					mima++;					
				}/* while a */		
			} /* if(ob->pd && ob->pd->deflect) */
			BLI_ghashIterator_step(ihash);
	} /* while () */
	BLI_ghashIterator_free(ihash);
	return deflected;	
}


void scan_for_ext_spring_forces(Object *ob,float timenow)
{
	SoftBody *sb = ob->soft;
	ListBase *do_effector;
	int a;
	float damp; 
	float feedback[3];
	do_effector= pdInitEffectors(ob,NULL);

	if (sb && sb->totspring){
		for(a=0; a<sb->totspring; a++) {
			BodySpring *bs = &sb->bspring[a];
			bs->ext_force[0]=bs->ext_force[1]=bs->ext_force[2]=0.0f; 
			feedback[0]=feedback[1]=feedback[2]=0.0f;
			bs->flag &= ~BSF_INTERSECT;

			if (bs->order ==1){
				/* +++ springs colliding */
				if (ob->softflag & OB_SB_EDGECOLL){
					if ( sb_detect_edge_collisionCached (sb->bpoint[bs->v1].pos , sb->bpoint[bs->v2].pos,
						&damp,feedback,ob->lay,ob,timenow)){
							VecAddf(bs->ext_force,bs->ext_force,feedback);
							bs->flag |= BSF_INTERSECT;
							//bs->cf=damp;
                            bs->cf=sb->choke*0.01f;

					}
				}
				/* ---- springs colliding */

				/* +++ springs seeing wind ... n stuff depending on their orientation*/
				/* note we don't use sb->mediafrict but use sb->aeroedge for magnitude of effect*/ 
				if(sb->aeroedge){
					float vel[3],sp[3],pr[3],force[3];
					float f,windfactor  = 250.0f;   
					/*see if we have wind*/
					if(do_effector) {
						float speed[3]={0.0f,0.0f,0.0f};
						float pos[3];
						VecMidf(pos, sb->bpoint[bs->v1].pos , sb->bpoint[bs->v2].pos);
						VecMidf(vel, sb->bpoint[bs->v1].vec , sb->bpoint[bs->v2].vec);
						pdDoEffectors(do_effector, pos, force, speed, (float)G.scene->r.cfra, 0.0f, PE_WIND_AS_SPEED);
						VecMulf(speed,windfactor); 
						VecAddf(vel,vel,speed);
					}
					/* media in rest */
					else{
						VECADD(vel, sb->bpoint[bs->v1].vec , sb->bpoint[bs->v2].vec);
					}
					f = Normalize(vel);
					f = -0.0001f*f*f*sb->aeroedge;
					/* todo add a nice angle dependant function */
					/* look up one at bergman scheafer */

					VECSUB(sp, sb->bpoint[bs->v1].pos , sb->bpoint[bs->v2].pos);
					Projf(pr,vel,sp);
					VECSUB(vel,vel,pr);
					Normalize(vel);
					Vec3PlusStVec(bs->ext_force,f,vel);
				}
				/* --- springs seeing wind */
			}
		}
	}
	if(do_effector)
		pdEndEffectors(do_effector);
}
/* --- the spring external section*/

int choose_winner(float*w, float* pos,float*a,float*b,float*c,float*ca,float*cb,float*cc)
{
	float mindist,cp;
	int winner =1;
	mindist = ABS(Inpf(pos,a));

    cp = ABS(Inpf(pos,b));
	if ( mindist < cp ){
		mindist = cp;
		winner =2;
	}

	cp = ABS(Inpf(pos,c));
	if (mindist < cp ){
		mindist = cp;
		winner =3;
	}
	switch (winner){ 
		case 1: VECCOPY(w,ca); break; 
		case 2: VECCOPY(w,cb); break; 
		case 3: VECCOPY(w,cc); 
	}
	return(winner);
}



int sb_detect_vertex_collisionCached(float opco[3], float facenormal[3], float *damp,
									 float force[3], unsigned int par_layer,struct Object *vertexowner,
									 float time,float vel[3], float *intrusion)
{
	Object *ob;
	GHash *hash;
	GHashIterator *ihash;
	float nv1[3], nv2[3], nv3[3], nv4[3], edge1[3], edge2[3],d_nvect[3], dv1[3],ve[3],avel[3],
    vv1[3], vv2[3], vv3[3], vv4[3], coledge[3], mindistedge = 1000.0f, 
	outerforceaccu[3],innerforceaccu[3],
		facedist,n_mag,force_mag_norm,minx,miny,minz,maxx,maxy,maxz,
		innerfacethickness = -0.5f, outerfacethickness = 0.2f,
		ee = 5.0f, ff = 0.1f, fa;
	int a, deflected=0, cavel=0,ci=0;
/* init */
	*intrusion = 0.0f;
	hash  = vertexowner->soft->scratch->colliderhash;
	ihash =	BLI_ghashIterator_new(hash);
	outerforceaccu[0]=outerforceaccu[1]=outerforceaccu[2]=0.0f;
	innerforceaccu[0]=innerforceaccu[1]=innerforceaccu[2]=0.0f;
/* go */
    while (!BLI_ghashIterator_isDone(ihash) ) {

		ccd_Mesh *ccdm = BLI_ghashIterator_getValue	(ihash);
		ob             = BLI_ghashIterator_getKey	(ihash);
			/* only with deflecting set */
			if(ob->pd && ob->pd->deflect) {
				MFace *mface= NULL;
				MVert *mvert= NULL;
				MVert *mprevvert= NULL;
				ccdf_minmax *mima= NULL;

				if(ccdm){
					mface= ccdm->mface;
					mvert= ccdm->mvert;
					mprevvert= ccdm->mprevvert;
					mima= ccdm->mima;
					a = ccdm->totface;

					minx =ccdm->bbmin[0]; 
					miny =ccdm->bbmin[1]; 
					minz =ccdm->bbmin[2];

					maxx =ccdm->bbmax[0]; 
					maxy =ccdm->bbmax[1]; 
					maxz =ccdm->bbmax[2]; 

					if ((opco[0] < minx) || 
						(opco[1] < miny) ||
						(opco[2] < minz) ||
						(opco[0] > maxx) || 
						(opco[1] > maxy) || 
						(opco[2] > maxz) ) {
							/* outside the padded boundbox --> collision object is too far away */ 
												BLI_ghashIterator_step(ihash);
							continue;				
					}					
				}
				else{
					/*aye that should be cached*/
					printf("missing cache error \n");
						BLI_ghashIterator_step(ihash);
					continue;				
				}

				/* do object level stuff */
				/* need to have user control for that since it depends on model scale */
				innerfacethickness =-ob->pd->pdef_sbift;
				outerfacethickness =ob->pd->pdef_sboft;
				fa = (ff*outerfacethickness-outerfacethickness);
				fa *= fa;
				fa = 1.0f/fa;
                avel[0]=avel[1]=avel[2]=0.0f;
				/* use mesh*/
				while (a--) {
					if (
						(opco[0] < mima->minx) || 
						(opco[0] > mima->maxx) || 
						(opco[1] < mima->miny) ||
						(opco[1] > mima->maxy) || 
						(opco[2] < mima->minz) ||
						(opco[2] > mima->maxz) 
						) {
							mface++;
							mima++;
							continue;
					}

					if (mvert){

						VECCOPY(nv1,mvert[mface->v1].co);						
						VECCOPY(nv2,mvert[mface->v2].co);
						VECCOPY(nv3,mvert[mface->v3].co);
						if (mface->v4){
							VECCOPY(nv4,mvert[mface->v4].co);
						}

						if (mprevvert){
							/* grab the average speed of the collider vertices
							before we spoil nvX 
							humm could be done once a SB steps but then we' need to store that too
							since the AABB reduced propabitlty to get here drasticallly
							it might be a nice tradeof CPU <--> memory
							*/
							VECSUB(vv1,nv1,mprevvert[mface->v1].co);
							VECSUB(vv2,nv2,mprevvert[mface->v2].co);
							VECSUB(vv3,nv3,mprevvert[mface->v3].co);
							if (mface->v4){
								VECSUB(vv4,nv4,mprevvert[mface->v4].co);
							}

							VecMulf(nv1,time);
							Vec3PlusStVec(nv1,(1.0f-time),mprevvert[mface->v1].co);

							VecMulf(nv2,time);
							Vec3PlusStVec(nv2,(1.0f-time),mprevvert[mface->v2].co);

							VecMulf(nv3,time);
							Vec3PlusStVec(nv3,(1.0f-time),mprevvert[mface->v3].co);

							if (mface->v4){
								VecMulf(nv4,time);
								Vec3PlusStVec(nv4,(1.0f-time),mprevvert[mface->v4].co);
							}
						}	
					}
					
					/* switch origin to be nv2*/
					VECSUB(edge1, nv1, nv2);
					VECSUB(edge2, nv3, nv2);
					VECSUB(dv1,opco,nv2); /* abuse dv1 to have vertex in question at *origin* of triangle */

					Crossf(d_nvect, edge2, edge1);
					n_mag = Normalize(d_nvect);
					facedist = Inpf(dv1,d_nvect);
					// so rules are
					//

					if ((facedist > innerfacethickness) && (facedist < outerfacethickness)){		
						if (point_in_tri_prism(opco, nv1, nv2, nv3) ){
							force_mag_norm =(float)exp(-ee*facedist);
							if (facedist > outerfacethickness*ff)
								force_mag_norm =(float)force_mag_norm*fa*(facedist - outerfacethickness)*(facedist - outerfacethickness);
							*damp=ob->pd->pdef_sbdamp;
							if (facedist > 0.0f){
								*damp *= (1.0f - facedist/outerfacethickness);
								Vec3PlusStVec(outerforceaccu,force_mag_norm,d_nvect);
								deflected = 3;

							}
							else {
								Vec3PlusStVec(innerforceaccu,force_mag_norm,d_nvect);
								if (deflected < 2) deflected = 2;
							}
							if ((mprevvert) && (*damp > 0.0f)){
								choose_winner(ve,opco,nv1,nv2,nv3,vv1,vv2,vv3);
								VECADD(avel,avel,ve);
								cavel ++;
							}
							*intrusion += facedist;
							ci++;
						}
					}		
					if (mface->v4){ /* quad */
						/* switch origin to be nv4 */
						VECSUB(edge1, nv3, nv4);
						VECSUB(edge2, nv1, nv4);
						VECSUB(dv1,opco,nv4); /* abuse dv1 to have vertex in question at *origin* of triangle */

						Crossf(d_nvect, edge2, edge1);
						n_mag = Normalize(d_nvect);
						facedist = Inpf(dv1,d_nvect);

						if ((facedist > innerfacethickness) && (facedist < outerfacethickness)){
							if (point_in_tri_prism(opco, nv1, nv3, nv4) ){
								force_mag_norm =(float)exp(-ee*facedist);
								if (facedist > outerfacethickness*ff)
									force_mag_norm =(float)force_mag_norm*fa*(facedist - outerfacethickness)*(facedist - outerfacethickness);
								*damp=ob->pd->pdef_sbdamp;
							if (facedist > 0.0f){
								*damp *= (1.0f - facedist/outerfacethickness);
								Vec3PlusStVec(outerforceaccu,force_mag_norm,d_nvect);
								deflected = 3;

							}
							else {
								Vec3PlusStVec(innerforceaccu,force_mag_norm,d_nvect);
								if (deflected < 2) deflected = 2;
							}

								if ((mprevvert) && (*damp > 0.0f)){
									choose_winner(ve,opco,nv1,nv3,nv4,vv1,vv3,vv4);
									VECADD(avel,avel,ve);
									cavel ++;
								}
							    *intrusion += facedist;
								ci++;
							}

						}
						if ((deflected < 2)&& (G.rt != 444)) // we did not hit a face until now
						{ // see if 'outer' hits an edge
							float dist;

							PclosestVL3Dfl(ve, opco, nv1, nv2);
 				            VECSUB(ve,opco,ve); 
							dist = Normalize(ve);
							if ((dist < outerfacethickness)&&(dist < mindistedge )){
								VECCOPY(coledge,ve);
								mindistedge = dist,
								deflected=1;
							}

							PclosestVL3Dfl(ve, opco, nv2, nv3);
 				            VECSUB(ve,opco,ve); 
							dist = Normalize(ve);
							if ((dist < outerfacethickness)&&(dist < mindistedge )){
								VECCOPY(coledge,ve);
								mindistedge = dist,
								deflected=1;
							}

							PclosestVL3Dfl(ve, opco, nv3, nv1);
 				            VECSUB(ve,opco,ve); 
							dist = Normalize(ve);
							if ((dist < outerfacethickness)&&(dist < mindistedge )){
								VECCOPY(coledge,ve);
								mindistedge = dist,
								deflected=1;
							}
							if (mface->v4){ /* quad */
								PclosestVL3Dfl(ve, opco, nv3, nv4);
								VECSUB(ve,opco,ve); 
								dist = Normalize(ve);
								if ((dist < outerfacethickness)&&(dist < mindistedge )){
									VECCOPY(coledge,ve);
									mindistedge = dist,
										deflected=1;
								}

								PclosestVL3Dfl(ve, opco, nv1, nv4);
								VECSUB(ve,opco,ve); 
								dist = Normalize(ve);
								if ((dist < outerfacethickness)&&(dist < mindistedge )){
									VECCOPY(coledge,ve);
									mindistedge = dist,
										deflected=1;
								}
							
							}


						}
					}
					mface++;
					mima++;					
				}/* while a */		
			} /* if(ob->pd && ob->pd->deflect) */
			BLI_ghashIterator_step(ihash);
	} /* while () */

	if (deflected == 1){ // no face but 'outer' edge cylinder sees vert
		force_mag_norm =(float)exp(-ee*mindistedge);
		if (mindistedge > outerfacethickness*ff)
			force_mag_norm =(float)force_mag_norm*fa*(mindistedge - outerfacethickness)*(mindistedge - outerfacethickness);
		Vec3PlusStVec(force,force_mag_norm,coledge);
		*damp=ob->pd->pdef_sbdamp;
		if (mindistedge > 0.0f){
			*damp *= (1.0f - mindistedge/outerfacethickness);
		}

	}
	if (deflected == 2){ //  face inner detected
		VECADD(force,force,innerforceaccu);
	}
	if (deflected == 3){ //  face outer detected
		VECADD(force,force,outerforceaccu);
	}

	BLI_ghashIterator_free(ihash);
	if (cavel) VecMulf(avel,1.0f/(float)cavel);
	VECCOPY(vel,avel);
	if (ci) *intrusion /= ci;
	if (deflected){ 
		VECCOPY(facenormal,force);
		Normalize(facenormal);
	}
	return deflected;	
}

/* not complete yet ..
   try to find a pos resolving all inside collisions 
*/
#if 0 //mute it for now 
int sb_detect_vertex_collisionCachedEx(float opco[3], float facenormal[3], float *damp,
									 float force[3], unsigned int par_layer,struct Object *vertexowner,
									 float time,float vel[3], float *intrusion)
{
	Object *ob;
	GHash *hash;
	GHashIterator *ihash;
	float nv1[3], nv2[3], nv3[3], nv4[3], edge1[3], edge2[3],d_nvect[3], dv1[3],ve[3],avel[3],
    vv1[3], vv2[3], vv3[3], vv4[3],
		facedist,n_mag,force_mag_norm,minx,miny,minz,maxx,maxy,maxz,
		innerfacethickness,outerfacethickness,
		closestinside,
		ee = 5.0f, ff = 0.1f, fa;
	int a, deflected=0, cavel=0;
/* init */
	*intrusion = 0.0f;
	hash  = vertexowner->soft->scratch->colliderhash;
	ihash =	BLI_ghashIterator_new(hash);
/* go */
    while (!BLI_ghashIterator_isDone(ihash) ) {

		ccd_Mesh *ccdm = BLI_ghashIterator_getValue	(ihash);
		ob             = BLI_ghashIterator_getKey	(ihash);
			/* only with deflecting set */
			if(ob->pd && ob->pd->deflect) {
				MFace *mface= NULL;
				MVert *mvert= NULL;
				MVert *mprevvert= NULL;
				ccdf_minmax *mima= NULL;

				if(ccdm){
					mface= ccdm->mface;
					mvert= ccdm->mvert;
					mprevvert= ccdm->mprevvert;
					mima= ccdm->mima;
					a = ccdm->totface;

					minx =ccdm->bbmin[0]; 
					miny =ccdm->bbmin[1]; 
					minz =ccdm->bbmin[2];

					maxx =ccdm->bbmax[0]; 
					maxy =ccdm->bbmax[1]; 
					maxz =ccdm->bbmax[2]; 

					if ((opco[0] < minx) || 
						(opco[1] < miny) ||
						(opco[2] < minz) ||
						(opco[0] > maxx) || 
						(opco[1] > maxy) || 
						(opco[2] > maxz) ) {
							/* outside the padded boundbox --> collision object is too far away */ 
												BLI_ghashIterator_step(ihash);
							continue;				
					}					
				}
				else{
					/*aye that should be cached*/
					printf("missing cache error \n");
						BLI_ghashIterator_step(ihash);
					continue;				
				}

				/* do object level stuff */
				/* need to have user control for that since it depends on model scale */
				innerfacethickness =-ob->pd->pdef_sbift;
				outerfacethickness =ob->pd->pdef_sboft;
				closestinside = innerfacethickness;
				fa = (ff*outerfacethickness-outerfacethickness);
				fa *= fa;
				fa = 1.0f/fa;
                avel[0]=avel[1]=avel[2]=0.0f;
				/* use mesh*/
				while (a--) {
					if (
						(opco[0] < mima->minx) || 
						(opco[0] > mima->maxx) || 
						(opco[1] < mima->miny) ||
						(opco[1] > mima->maxy) || 
						(opco[2] < mima->minz) ||
						(opco[2] > mima->maxz) 
						) {
							mface++;
							mima++;
							continue;
					}

					if (mvert){

						VECCOPY(nv1,mvert[mface->v1].co);						
						VECCOPY(nv2,mvert[mface->v2].co);
						VECCOPY(nv3,mvert[mface->v3].co);
						if (mface->v4){
							VECCOPY(nv4,mvert[mface->v4].co);
						}

						if (mprevvert){
							/* grab the average speed of the collider vertices
							before we spoil nvX 
							humm could be done once a SB steps but then we' need to store that too
							since the AABB reduced propabitlty to get here drasticallly
							it might be a nice tradeof CPU <--> memory
							*/
							VECSUB(vv1,nv1,mprevvert[mface->v1].co);
							VECSUB(vv2,nv2,mprevvert[mface->v2].co);
							VECSUB(vv3,nv3,mprevvert[mface->v3].co);
							if (mface->v4){
								VECSUB(vv4,nv4,mprevvert[mface->v4].co);
							}

							VecMulf(nv1,time);
							Vec3PlusStVec(nv1,(1.0f-time),mprevvert[mface->v1].co);

							VecMulf(nv2,time);
							Vec3PlusStVec(nv2,(1.0f-time),mprevvert[mface->v2].co);

							VecMulf(nv3,time);
							Vec3PlusStVec(nv3,(1.0f-time),mprevvert[mface->v3].co);

							if (mface->v4){
								VecMulf(nv4,time);
								Vec3PlusStVec(nv4,(1.0f-time),mprevvert[mface->v4].co);
							}
						}	
					}
					
					/* switch origin to be nv2*/
					VECSUB(edge1, nv1, nv2);
					VECSUB(edge2, nv3, nv2);
					VECSUB(dv1,opco,nv2); /* abuse dv1 to have vertex in question at *origin* of triangle */

					Crossf(d_nvect, edge2, edge1);
					n_mag = Normalize(d_nvect);
					facedist = Inpf(dv1,d_nvect);

					if ((facedist > closestinside) && (facedist < outerfacethickness)){		
//					if ((facedist > innerfacethickness) && (facedist < outerfacethickness)){		
						if (point_in_tri_prism(opco, nv1, nv2, nv3) ){
							force_mag_norm =(float)exp(-ee*facedist);
							if (facedist > outerfacethickness*ff)
								force_mag_norm =(float)force_mag_norm*fa*(facedist - outerfacethickness)*(facedist - outerfacethickness);
							    *damp=ob->pd->pdef_sbdamp;

								if (facedist > 0.0f){
									*damp *= (1.0f - facedist/outerfacethickness);								
									Vec3PlusStVec(force,force_mag_norm,d_nvect);
									if (deflected < 2){
										deflected = 1;
										if ((mprevvert) && (*damp > 0.0f)){
											choose_winner(ve,opco,nv1,nv2,nv3,vv1,vv2,vv3);
											VECADD(avel,avel,ve);
											cavel ++;
										}
									}
									
								}
								else{
									Vec3PlusStVec(force,force_mag_norm,d_nvect);
									VECCOPY(facenormal,d_nvect);
									if ((mprevvert) && (*damp > 0.0f)){
										choose_winner(avel,opco,nv1,nv2,nv3,vv1,vv2,vv3);
										cavel = 1;
										deflected = 2;
										closestinside = facedist;
									}
								}
							*intrusion = facedist;
						}
					}		
					if (mface->v4){ /* quad */
						/* switch origin to be nv4 */
						VECSUB(edge1, nv3, nv4);
						VECSUB(edge2, nv1, nv4);
						VECSUB(dv1,opco,nv4); /* abuse dv1 to have vertex in question at *origin* of triangle */

						Crossf(d_nvect, edge2, edge1);
						n_mag = Normalize(d_nvect);
						facedist = Inpf(dv1,d_nvect);

						if ((facedist > innerfacethickness) && (facedist < outerfacethickness)){
							if (point_in_tri_prism(opco, nv1, nv3, nv4) ){
								force_mag_norm =(float)exp(-ee*facedist);
								if (facedist > outerfacethickness*ff)
									force_mag_norm =(float)force_mag_norm*fa*(facedist - outerfacethickness)*(facedist - outerfacethickness);
								Vec3PlusStVec(force,force_mag_norm,d_nvect);
								*damp=ob->pd->pdef_sbdamp;

								if (facedist > 0.0f){
									*damp *= (1.0f - facedist/outerfacethickness);								
									Vec3PlusStVec(force,force_mag_norm,d_nvect);
									if (deflected < 2){
										deflected = 1;
										if ((mprevvert) && (*damp > 0.0f)){
											choose_winner(ve,opco,nv1,nv3,nv4,vv1,vv3,vv4);
											VECADD(avel,avel,ve);
											cavel ++;
										}
									}
									
								}
								else{
									Vec3PlusStVec(force,force_mag_norm,d_nvect);
									VECCOPY(facenormal,d_nvect);
									if ((mprevvert) && (*damp > 0.0f)){
										choose_winner(avel,opco,nv1,nv3,nv4,vv1,vv3,vv4);
										cavel = 1;
										deflected = 2;
										closestinside = facedist;
									}
								}



							    *intrusion = facedist;
							}

						}
					}
					mface++;
					mima++;					
				}/* while a */		
			} /* if(ob->pd && ob->pd->deflect) */
			BLI_ghashIterator_step(ihash);
	} /* while () */
	BLI_ghashIterator_free(ihash);
	if (cavel) VecMulf(avel,1.0f/(float)cavel);
	VECCOPY(vel,avel);

    /* we  did stay "outside" but have some close to contact forces
	   just to be complete fake a face normal
	*/
	if (deflected ==1){ 
		VECCOPY(facenormal,force);
		Normalize(facenormal);
	} 
	else{
		facenormal[0] = facenormal[1] = facenormal[2] = 0.0f;
	}
	return deflected;	
}
#endif



/* sandbox to plug in various deflection algos */
static int sb_deflect_face(Object *ob,float *actpos,float *facenormal,float *force,float *cf,float time,float *vel,float *intrusion)
{
	float s_actpos[3];
	int deflected;	
	VECCOPY(s_actpos,actpos);
	deflected= sb_detect_vertex_collisionCached(s_actpos, facenormal, cf, force , ob->lay, ob,time,vel,intrusion);
	//deflected= sb_detect_vertex_collisionCachedEx(s_actpos, facenormal, cf, force , ob->lay, ob,time,vel,intrusion);
	return(deflected);
}


static void softbody_calc_forces(Object *ob, float forcetime, float timenow)
{
/* rule we never alter free variables :bp->vec bp->pos in here ! 
 * this will ruin adaptive stepsize AKA heun! (BM) 
 */
	SoftBody *sb= ob->soft;	/* is supposed to be there */
	BodyPoint  *bp;
	BodyPoint *bproot;
	BodySpring *bs;	
	ListBase *do_effector;
	float iks, ks, kd, gravity, actspringlen, forcefactor, sd[3];
	float fieldfactor = 1000.0f, windfactor  = 250.0f;   
	float tune = sb->ballstiff;
	int a, b,  do_deflector,do_selfcollision,do_springcollision,do_aero;
	
	
	gravity = sb->grav * sb_grav_force_scale(ob);	
	
	/* check conditions for various options */
	do_deflector= query_external_colliders(ob);
	do_effector= pdInitEffectors(ob,NULL);
	do_selfcollision=((ob->softflag & OB_SB_EDGES) && (sb->bspring)&& (ob->softflag & OB_SB_SELF));
	do_springcollision=do_deflector && (ob->softflag & OB_SB_EDGES) &&(ob->softflag & OB_SB_EDGECOLL);
	do_aero=((sb->aeroedge)&& (ob->softflag & OB_SB_EDGES));
	
	iks  = 1.0f/(1.0f-sb->inspring)-1.0f ;/* inner spring constants function */
	bproot= sb->bpoint; /* need this for proper spring addressing */
	

	
	if (do_springcollision || do_aero)  scan_for_ext_spring_forces(ob,timenow);
	if (do_deflector) {
		float defforce[3];
		do_deflector = sb_detect_aabb_collisionCached(defforce,ob->lay,ob,timenow);
	}

	if (do_selfcollision ){ 
		float ce[3]; 
		VecMidf(ce,sb->scratch->aabbmax,sb->scratch->aabbmin);
		for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
			bp->octantflag = set_octant_flags(ce,bp->pos,bp->colball);
		}
	}

	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		/* clear forces  accumulator */
		bp->force[0]= bp->force[1]= bp->force[2]= 0.0;

		/* naive ball self collision */
		/* needs to be done if goal snaps or not */
		if(do_selfcollision){
			 	int attached;
				BodyPoint   *obp;
				int c,b;
				float velcenter[3],dvel[3],def[3];
				float distance;
				float compare;

				for(c=sb->totpoint, obp= sb->bpoint; c>=a; c--, obp++) {
					
					if ((bp->octantflag & obp->octantflag) == 0) continue;

					compare = (obp->colball + bp->colball);		
					VecSubf(def, bp->pos, obp->pos);

					/* rather check the AABBoxes before ever calulating the real distance */
					/* mathematically it is completly nuts, but performace is pretty much (3) times faster */
					if ((ABS(def[0]) > compare) || (ABS(def[1]) > compare) || (ABS(def[2]) > compare)) continue;

                    distance = Normalize(def);
					if (distance < compare ){
						/* exclude body points attached with a spring */
						attached = 0;
						for(b=obp->nofsprings;b>0;b--){
							bs = sb->bspring + obp->springs[b-1];
							if (( sb->totpoint-a == bs->v2)  || ( sb->totpoint-a == bs->v1)){
								attached=1;
								continue;}
						}
						if (!attached){
							float f = tune/(distance) + tune/(compare*compare)*distance - 2.0f*tune/compare ;

							VecMidf(velcenter, bp->vec, obp->vec);
							VecSubf(dvel,velcenter,bp->vec);
							VecMulf(dvel,sb->nodemass);

							Vec3PlusStVec(bp->force,sb->balldamp,dvel);
							Vec3PlusStVec(bp->force,f*(1.0f-sb->balldamp),def);
							/* exploit force(a,b) == -force(b,a) part2/2 */
							VecSubf(dvel,velcenter,obp->vec);
							VecMulf(dvel,sb->nodemass);

							Vec3PlusStVec(obp->force,sb->balldamp,dvel);
							Vec3PlusStVec(obp->force,-f*(1.0f-sb->balldamp),def);

						}
					}
				}
		}
		/* naive ball self collision done */

		if(bp->goal < SOFTGOALSNAP){ /* ommit this bp when it snaps */
			float auxvect[3];  
			float velgoal[3];
			float absvel =0, projvel= 0;
			
			/* do goal stuff */
			if(ob->softflag & OB_SB_GOAL) {
				/* true elastic goal */
				VecSubf(auxvect,bp->origT,bp->pos);
				ks  = 1.0f/(1.0f- bp->goal*sb->goalspring)-1.0f ;
				bp->force[0]+= ks*(auxvect[0]);
				bp->force[1]+= ks*(auxvect[1]);
				bp->force[2]+= ks*(auxvect[2]);
				/* calulate damping forces generated by goals*/
				VecSubf(velgoal,bp->origS, bp->origE);
				kd =  sb->goalfrict * sb_fric_force_scale(ob) ;
				
				if (forcetime > 0.0 ) { /* make sure friction does not become rocket motor on time reversal */
					bp->force[0]-= kd * (velgoal[0] + bp->vec[0]);
					bp->force[1]-= kd * (velgoal[1] + bp->vec[1]);
					bp->force[2]-= kd * (velgoal[2] + bp->vec[2]);
				}
				else {
					bp->force[0]-= kd * (velgoal[0] - bp->vec[0]);
					bp->force[1]-= kd * (velgoal[1] - bp->vec[1]);
					bp->force[2]-= kd * (velgoal[2] - bp->vec[2]);
				}
			}
			/* done goal stuff */
			
			
			/* gravitation */
			bp->force[2]-= gravity*sb->nodemass; /* individual mass of node here */

			
			/* particle field & vortex */
			if(do_effector) {
				float force[3]= {0.0f, 0.0f, 0.0f};
				float speed[3]= {0.0f, 0.0f, 0.0f};
				float eval_sb_fric_force_scale = sb_fric_force_scale(ob); /* just for calling function once */
				
				pdDoEffectors(do_effector, bp->pos, force, speed, (float)G.scene->r.cfra, 0.0f, PE_WIND_AS_SPEED);
				
				/* apply forcefield*/
				VecMulf(force,fieldfactor* eval_sb_fric_force_scale); 
				VECADD(bp->force, bp->force, force);
				
				/* BP friction in moving media */	
				kd= sb->mediafrict* eval_sb_fric_force_scale;  
				bp->force[0] -= kd * (bp->vec[0] + windfactor*speed[0]/eval_sb_fric_force_scale);
				bp->force[1] -= kd * (bp->vec[1] + windfactor*speed[1]/eval_sb_fric_force_scale);
				bp->force[2] -= kd * (bp->vec[2] + windfactor*speed[2]/eval_sb_fric_force_scale);
				/* now we'll have nice centrifugal effect for vortex */
				
			}
			else {
				/* BP friction in media (not) moving*/
				kd= sb->mediafrict* sb_fric_force_scale(ob);  
				/* assume it to be proportional to actual velocity */
				bp->force[0]-= bp->vec[0]*kd;
				bp->force[1]-= bp->vec[1]*kd;
				bp->force[2]-= bp->vec[2]*kd;
				/* friction in media done */
			}
			/* +++cached collision targets */
			bp->choke = 0.0f;
			bp->flag &= ~SBF_DOFUZZY;
			if(do_deflector) {
				float cfforce[3],defforce[3] ={0.0f,0.0f,0.0f}, vel[3] = {0.0f,0.0f,0.0f}, facenormal[3], cf = 1.0f,intrusion;
				kd = 1.0f;

				if (sb_deflect_face(ob,bp->pos,facenormal,defforce,&cf,timenow,vel,&intrusion)){
					if (intrusion < 0.0f){
						/*bjornmose:  uugh.. what an evil hack 
						violation of the 'don't touch bp->pos in here' rule 
						but works nice, like this-->
						we predict the solution beeing out of the collider
						in heun step No1 and leave the heun step No2 adapt to it
						so we kind of introduced a implicit solver for this case 
						*/
						Vec3PlusStVec(bp->pos,-intrusion,facenormal);

						sb->scratch->flag |= SBF_DOFUZZY;
						bp->flag |= SBF_DOFUZZY;
					    bp->choke = sb->choke*0.01f;
					}
					else{
							VECSUB(cfforce,bp->vec,vel);
							Vec3PlusStVec(bp->force,-cf*50.0f,cfforce);
					}

					Vec3PlusStVec(bp->force,kd,defforce);
				}

			}
			/* ---cached collision targets */

			/* +++springs */
			if(ob->softflag & OB_SB_EDGES) {
				if (sb->bspring){ /* spring list exists at all ? */
					for(b=bp->nofsprings;b>0;b--){
						bs = sb->bspring + bp->springs[b-1];
						if (do_springcollision || do_aero){
							VecAddf(bp->force,bp->force,bs->ext_force);
							if (bs->flag & BSF_INTERSECT)
								bp->choke = bs->cf; 

						}

						if (( (sb->totpoint-a) == bs->v1) ){ 
							actspringlen= VecLenf( (bproot+bs->v2)->pos, bp->pos);
							VecSubf(sd,(bproot+bs->v2)->pos, bp->pos);
							Normalize(sd);
							
							/* friction stuff V1 */
							VecSubf(velgoal,bp->vec,(bproot+bs->v2)->vec);
							kd = sb->infrict * sb_fric_force_scale(ob);
							absvel  = Normalize(velgoal);
							projvel = ABS(Inpf(sd,velgoal));
							kd *= absvel * projvel;
							Vec3PlusStVec(bp->force,-kd,velgoal);
							
							if(bs->len > 0.0f) /* check for degenerated springs */
								forcefactor = (bs->len - actspringlen)/bs->len * iks;
							else
								forcefactor = actspringlen * iks;
							forcefactor *= bs->strength; 
							
							Vec3PlusStVec(bp->force,-forcefactor,sd);
							
						}
						
						if (( (sb->totpoint-a) == bs->v2) ){ 
							actspringlen= VecLenf( (bproot+bs->v1)->pos, bp->pos);
							VecSubf(sd,bp->pos,(bproot+bs->v1)->pos);
							Normalize(sd);
							
							/* friction stuff V2 */
							VecSubf(velgoal,bp->vec,(bproot+bs->v1)->vec);
							kd = sb->infrict * sb_fric_force_scale(ob);
							absvel  = Normalize(velgoal);
							projvel = ABS(Inpf(sd,velgoal));
							kd *= absvel * projvel;
							Vec3PlusStVec(bp->force,-kd,velgoal);
							
							if(bs->len > 0.0)
								forcefactor = (bs->len - actspringlen)/bs->len * iks;
							else
								forcefactor = actspringlen * iks;
							forcefactor *= bs->strength; 
							Vec3PlusStVec(bp->force,+forcefactor,sd);							
						}
					}/* loop springs */
				}/* existing spring list */ 
			}/*any edges*/
			/* ---springs */
		}/*omit on snap	*/
	}/*loop all bp's*/


	/* finally add forces caused by face collision */
    if (ob->softflag & OB_SB_FACECOLL) scan_for_ext_face_forces(ob,timenow);
	/* cleanup */
	if(do_effector) pdEndEffectors(do_effector);
}



static void softbody_apply_forces(Object *ob, float forcetime, int mode, float *err)
{
	/* time evolution */
	/* actually does an explicit euler step mode == 0 */
	/* or heun ~ 2nd order runge-kutta steps, mode 1,2 */
	SoftBody *sb= ob->soft;	/* is supposed to be there */
	BodyPoint *bp;
	float dx[3],dv[3],aabbmin[3],aabbmax[3],cm[3]={0.0f,0.0f,0.0f};
	float timeovermass;
	float maxerrpos= 0.0f,maxerrvel = 0.0f;
	int a,fuzzy=0;

    forcetime *= sb_time_scale(ob);
    
    aabbmin[0]=aabbmin[1]=aabbmin[2] = 1e20f;
    aabbmax[0]=aabbmax[1]=aabbmax[2] = -1e20f;

	/* claim a minimum mass for vertex */
	if (sb->nodemass > 0.009999f) timeovermass = forcetime/sb->nodemass;
	else timeovermass = forcetime/0.009999f;
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		if(bp->goal < SOFTGOALSNAP){
			
			/* so here is (v)' = a(cceleration) = sum(F_springs)/m + gravitation + some friction forces  + more forces*/
			/* the ( ... )' operator denotes derivate respective time */
			/* the euler step for velocity then becomes */
			/* v(t + dt) = v(t) + a(t) * dt */ 
			bp->force[0]*= timeovermass; /* individual mass of node here */ 
			bp->force[1]*= timeovermass;
			bp->force[2]*= timeovermass;
			/* some nasty if's to have heun in here too */
			VECCOPY(dv,bp->force); 

			if (mode == 1){
				VECCOPY(bp->prevvec, bp->vec);
				VECCOPY(bp->prevdv, dv);
			}

			if (mode ==2){
				/* be optimistic and execute step */
				bp->vec[0] = bp->prevvec[0] + 0.5f * (dv[0] + bp->prevdv[0]);
				bp->vec[1] = bp->prevvec[1] + 0.5f * (dv[1] + bp->prevdv[1]);
				bp->vec[2] = bp->prevvec[2] + 0.5f * (dv[2] + bp->prevdv[2]);
				/* compare euler to heun to estimate error for step sizing */
				maxerrvel = MAX2(maxerrvel,ABS(dv[0] - bp->prevdv[0]));
				maxerrvel = MAX2(maxerrvel,ABS(dv[1] - bp->prevdv[1]));
				maxerrvel = MAX2(maxerrvel,ABS(dv[2] - bp->prevdv[2]));
			}
			else {VECADD(bp->vec, bp->vec, bp->force);}

			/* so here is (x)'= v(elocity) */
			/* the euler step for location then becomes */
			/* x(t + dt) = x(t) + v(t) * dt */ 
			
			VECCOPY(dx,bp->vec);
			dx[0]*=forcetime; 
			dx[1]*=forcetime; 
			dx[2]*=forcetime; 
			/* again some nasty if's to have heun in here too */
			if (mode ==1){
				VECCOPY(bp->prevpos,bp->pos);
				VECCOPY(bp->prevdx ,dx);
			}
			
			if (mode ==2){
				bp->pos[0] = bp->prevpos[0] + 0.5f * ( dx[0] + bp->prevdx[0]);
				bp->pos[1] = bp->prevpos[1] + 0.5f * ( dx[1] + bp->prevdx[1]);
				bp->pos[2] = bp->prevpos[2] + 0.5f* ( dx[2] + bp->prevdx[2]);
				maxerrpos = MAX2(maxerrpos,ABS(dx[0] - bp->prevdx[0]));
				maxerrpos = MAX2(maxerrpos,ABS(dx[1] - bp->prevdx[1]));
				maxerrpos = MAX2(maxerrpos,ABS(dx[2] - bp->prevdx[2]));

/* bp->choke is set when we need to pull a vertex or edge out of the collider. 
   the collider object signals to get out by pushing hard. on the other hand 
   we don't want to end up in deep space so we add some <viscosity> 
   to balance that out */
				if (bp->choke > 0.0f){
					bp->vec[0] = bp->vec[0]*(1.0f - bp->choke);
					bp->vec[1] = bp->vec[1]*(1.0f - bp->choke);
					bp->vec[2] = bp->vec[2]*(1.0f - bp->choke);
				}

			}
			else { VECADD(bp->pos, bp->pos, dx);}
		}/*snap*/
		/* so while we are looping BPs anyway do statistics on the fly */
		aabbmin[0] = MIN2(aabbmin[0],bp->pos[0]);
		aabbmin[1] = MIN2(aabbmin[1],bp->pos[1]);
		aabbmin[2] = MIN2(aabbmin[2],bp->pos[2]);
		aabbmax[0] = MAX2(aabbmax[0],bp->pos[0]);
		aabbmax[1] = MAX2(aabbmax[1],bp->pos[1]);
		aabbmax[2] = MAX2(aabbmax[2],bp->pos[2]);
		if (bp->flag & SBF_DOFUZZY) fuzzy =1;
	} /*for*/

	if (sb->totpoint) VecMulf(cm,1.0f/sb->totpoint);
	if (sb->scratch){
		VECCOPY(sb->scratch->aabbmin,aabbmin);
		VECCOPY(sb->scratch->aabbmax,aabbmax);
	}
	
	if (err){ /* so step size will be controlled by biggest difference in slope */
		if (sb->solverflags & SBSO_OLDERR)
		*err = MAX2(maxerrpos,maxerrvel);
		else
		*err = maxerrpos;
		//printf("EP %f EV %f \n",maxerrpos,maxerrvel);
		if (fuzzy){
			*err /= sb->fuzzyness;
		}
	}
}

/* used by heun when it overshoots */
static void softbody_restore_prev_step(Object *ob)
{
	SoftBody *sb= ob->soft;	/* is supposed to be there*/
	BodyPoint *bp;
	int a;
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		VECCOPY(bp->vec, bp->prevvec);
		VECCOPY(bp->pos, bp->prevpos);
	}
}

/* care for bodypoints taken out of the 'ordinary' solver step
** because they are screwed to goal by bolts
** they just need to move along with the goal in time 
** we need to adjust them on sub frame timing in solver 
** so now when frame is done .. put 'em to the position at the end of frame
*/
static void softbody_apply_goalsnap(Object *ob)
{
	SoftBody *sb= ob->soft;	/* is supposed to be there */
	BodyPoint *bp;
	int a;
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		if (bp->goal >= SOFTGOALSNAP){
			VECCOPY(bp->prevpos,bp->pos);
			VECCOPY(bp->pos,bp->origT);
		}		
	}
}

/* expects full initialized softbody */
static void interpolate_exciter(Object *ob, int timescale, int time)
{
	SoftBody *sb= ob->soft;
	BodyPoint *bp;
	float f;
	int a;
	
	f = (float)time/(float)timescale;
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {	
		bp->origT[0] = bp->origS[0] + f*(bp->origE[0] - bp->origS[0]); 
		bp->origT[1] = bp->origS[1] + f*(bp->origE[1] - bp->origS[1]); 
		bp->origT[2] = bp->origS[2] + f*(bp->origE[2] - bp->origS[2]); 
		if (bp->goal >= SOFTGOALSNAP){
			bp->vec[0] = bp->origE[0] - bp->origS[0];
			bp->vec[1] = bp->origE[1] - bp->origS[1];
			bp->vec[2] = bp->origE[2] - bp->origS[2];
		}
	}
	
}


/* ************ convertors ********** */

/*  for each object type we need;
    - xxxx_to_softbody(Object *ob)      : a full (new) copy, creates SB geometry
*/

static void get_scalar_from_vertexgroup(Object *ob, int vertID, short groupindex, float *target)
/* result 0 on success, else indicates error number
-- kind of *inverse* result defintion,
-- but this way we can signal error condition to caller  
-- and yes this function must not be here but in a *vertex group module*
*/
{
	MDeformVert *dv= NULL;
	int i;
	
	/* spot the vert in deform vert list at mesh */
	if(ob->type==OB_MESH) {
		Mesh *me= ob->data;
		if (me->dvert)
			dv = me->dvert + vertID;
	}
	else if(ob->type==OB_LATTICE) {	/* not yet supported in softbody btw */
		Lattice *lt= ob->data;
		if (lt->dvert)
			dv = lt->dvert + vertID;
	}
	if(dv) {
		/* Lets see if this vert is in the weight group */
		for (i=0; i<dv->totweight; i++){
			if (dv->dw[i].def_nr == groupindex){
				*target= dv->dw[i].weight; /* got it ! */
				break;
			}
		}
	}
} 

/* Resetting a Mesh SB object's springs */  
/* Spring lenght are caculted from'raw' mesh vertices that are NOT altered by modifier stack. */ 
static void springs_from_mesh(Object *ob)
{
	SoftBody *sb;
	Mesh *me= ob->data;
	BodyPoint *bp;
	int a;
	float scale =1.0f;
	
	sb= ob->soft;	
	if (me && sb)
	{ 
	/* using bp->origS as a container for spring calcualtions here
	** will be overwritten sbObjectStep() to receive 
	** actual modifier stack positions
	*/
		if(me->totvert) {    
			bp= ob->soft->bpoint;
			for(a=0; a<me->totvert; a++, bp++) {
				VECCOPY(bp->origS, me->mvert[a].co);                            
				Mat4MulVecfl(ob->obmat, bp->origS);
			}
			
		}
		/* recalculate spring length for meshes here */
		/* special hidden feature! shrink to fit */
		if (G.rt > 500){
			scale = (G.rt - 500) / 100.0f;
		}
		for(a=0; a<sb->totspring; a++) {
			BodySpring *bs = &sb->bspring[a];
			bs->len= scale*VecLenf(sb->bpoint[bs->v1].origS, sb->bpoint[bs->v2].origS);
		}
	}
}


/* makes totally fresh start situation */
static void mesh_to_softbody(Object *ob)
{
	SoftBody *sb;
	Mesh *me= ob->data;
	MEdge *medge= me->medge;
	BodyPoint *bp;
	BodySpring *bs;
	float goalfac;
	int a, totedge;
	if (ob->softflag & OB_SB_EDGES) totedge= me->totedge;
	else totedge= 0;
	
	/* renew ends with ob->soft with points and edges, also checks & makes ob->soft */
	renew_softbody(ob, me->totvert, totedge);
		
	/* we always make body points */
	sb= ob->soft;	
	bp= sb->bpoint;
	goalfac= ABS(sb->maxgoal - sb->mingoal);
	
	for(a=0; a<me->totvert; a++, bp++) {
		/* get scalar values needed  *per vertex* from vertex group functions,
		so we can *paint* them nicly .. 
		they are normalized [0.0..1.0] so may be we need amplitude for scale
		which can be done by caller but still .. i'd like it to go this way 
		*/ 
		
		if((ob->softflag & OB_SB_GOAL) && sb->vertgroup) {
			get_scalar_from_vertexgroup(ob, a,(short) (sb->vertgroup-1), &bp->goal);
			/* do this always, regardless successfull read from vertex group */
			bp->goal= sb->mingoal + bp->goal*goalfac;
		}
		/* a little ad hoc changing the goal control to be less *sharp* */
		bp->goal = (float)pow(bp->goal, 4.0f);
			
		/* to proove the concept
		this would enable per vertex *mass painting*
		strcpy(name,"SOFTMASS");
		error = get_scalar_from_named_vertexgroup(ob,name, a,&temp);
		if (!error) bp->mass = temp * ob->rangeofmass;
		*/
	}

	/* but we only optionally add body edge springs */
	if (ob->softflag & OB_SB_EDGES) {
		if(medge) {
			bs= sb->bspring;
			for(a=me->totedge; a>0; a--, medge++, bs++) {
				bs->v1= medge->v1;
				bs->v2= medge->v2;
				bs->strength= 1.0;
				bs->order=1;
			}
			
			
			/* insert *diagonal* springs in quads if desired */
			if (ob->softflag & OB_SB_QUADS) {
				add_mesh_quad_diag_springs(ob);
			}
			
			build_bps_springlist(ob); /* scan for springs attached to bodypoints ONCE */
			/* insert *other second order* springs if desired */
			if (sb->secondspring > 0.0000001f) {
				add_2nd_order_springs(ob,sb->secondspring); /* exploits the the first run of build_bps_springlist(ob);*/
				build_bps_springlist(ob); /* yes we need to do it again*/
			}
			springs_from_mesh(ob); /* write the 'rest'-lenght of the springs */
           	if (ob->softflag & OB_SB_SELF) {calculate_collision_balls(ob);}
			
		}
		
	}

}

static void mesh_faces_to_scratch(Object *ob)
{
	SoftBody *sb= ob->soft;	
	Mesh *me= ob->data;
	MFace *mface;
	BodyFace *bodyface;
	int a;
	/* alloc and copy faces*/
	
	bodyface = sb->scratch->bodyface = MEM_mallocN(sizeof(BodyFace)*me->totface,"SB_body_Faces");
	//memcpy(sb->scratch->mface,me->mface,sizeof(MFace)*me->totface);
	mface = me->mface;
	for(a=0; a<me->totface; a++, mface++, bodyface++) {
		bodyface->v1 = mface->v1;
		bodyface->v2 = mface->v2;
		bodyface->v3 = mface->v3;
		bodyface->v4 = mface->v4;
		bodyface->ext_force[0] = bodyface->ext_force[1] = bodyface->ext_force[2] = 0.0f;
		bodyface->flag =0;								
	}
	sb->scratch->totface = me->totface;
}

/*
helper function to get proper spring length 
when object is rescaled
*/
float globallen(float *v1,float *v2,Object *ob)
{
	float p1[3],p2[3];
	VECCOPY(p1,v1);
	Mat4MulVecfl(ob->obmat, p1);	
	VECCOPY(p2,v2);
	Mat4MulVecfl(ob->obmat, p2);
	return VecLenf(p1,p2);
}

static void makelatticesprings(Lattice *lt,	BodySpring *bs, int dostiff,Object *ob)
{
	BPoint *bp=lt->def, *bpu;
	int u, v, w, dv, dw, bpc=0, bpuc;
	
	dv= lt->pntsu;
	dw= dv*lt->pntsv;
	
	for(w=0; w<lt->pntsw; w++) {
		
		for(v=0; v<lt->pntsv; v++) {
			
			for(u=0, bpuc=0, bpu=NULL; u<lt->pntsu; u++, bp++, bpc++) {
				
				if(w) {
					bs->v1 = bpc;
					bs->v2 = bpc-dw;
				    bs->strength= 1.0;
					bs->order=1;
					bs->len= globallen((bp-dw)->vec, bp->vec,ob);
					bs++;
				}
				if(v) {
					bs->v1 = bpc;
					bs->v2 = bpc-dv;
				    bs->strength= 1.0;
					bs->order=1;
					bs->len= globallen((bp-dv)->vec, bp->vec,ob);
					bs++;
				}
				if(u) {
					bs->v1 = bpuc;
					bs->v2 = bpc;
				    bs->strength= 1.0;
					bs->order=1;
					bs->len= globallen((bpu)->vec, bp->vec,ob);
					bs++;
				}
				
				if (dostiff) {

  					if(w){
						if( v && u ) {
							bs->v1 = bpc;
							bs->v2 = bpc-dw-dv-1;
							bs->strength= 1.0;
					bs->order=2;
							bs->len= globallen((bp-dw-dv-1)->vec, bp->vec,ob);
							bs++;
						}						
						if( (v < lt->pntsv-1) && (u) ) {
							bs->v1 = bpc;
							bs->v2 = bpc-dw+dv-1;
							bs->strength= 1.0;
					bs->order=2;
							bs->len= globallen((bp-dw+dv-1)->vec, bp->vec,ob);
							bs++;
						}						
					}

					if(w < lt->pntsw -1){
						if( v && u ) {
							bs->v1 = bpc;
							bs->v2 = bpc+dw-dv-1;
							bs->strength= 1.0;
					bs->order=2;
							bs->len= globallen((bp+dw-dv-1)->vec, bp->vec,ob);
							bs++;
						}						
						if( (v < lt->pntsv-1) && (u) ) {
							bs->v1 = bpc;
							bs->v2 = bpc+dw+dv-1;
							bs->strength= 1.0;
					bs->order=2;
							 bs->len= globallen((bp+dw+dv-1)->vec, bp->vec,ob);
							bs++;
						}						
					}
				}
				bpu = bp;
				bpuc = bpc;
			}
		}
	}
}


/* makes totally fresh start situation */
static void lattice_to_softbody(Object *ob)
{
	Lattice *lt= ob->data;
	SoftBody *sb;
	int totvert, totspring = 0;

	totvert= lt->pntsu*lt->pntsv*lt->pntsw;

	if (ob->softflag & OB_SB_EDGES){
		totspring = ((lt->pntsu -1) * lt->pntsv 
		          + (lt->pntsv -1) * lt->pntsu) * lt->pntsw
				  +lt->pntsu*lt->pntsv*(lt->pntsw -1);
		if (ob->softflag & OB_SB_QUADS){
			totspring += 4*(lt->pntsu -1) *  (lt->pntsv -1)  * (lt->pntsw-1);
		}
	}
	

	/* renew ends with ob->soft with points and edges, also checks & makes ob->soft */
	renew_softbody(ob, totvert, totspring);
	sb= ob->soft;	/* can be created in renew_softbody() */
	
	/* weights from bpoints, same code used as for mesh vertices */
	if((ob->softflag & OB_SB_GOAL) && sb->vertgroup) {
		BodyPoint *bp= sb->bpoint;
		BPoint *bpnt= lt->def;
		float goalfac= ABS(sb->maxgoal - sb->mingoal);
		int a;

		for(a=0; a<totvert; a++, bp++, bpnt++) {
			bp->goal= sb->mingoal + bpnt->weight*goalfac;
			/* a little ad hoc changing the goal control to be less *sharp* */
			bp->goal = (float)pow(bp->goal, 4.0f);
		}
	}	
	
	/* create some helper edges to enable SB lattice to be usefull at all */
	if (ob->softflag & OB_SB_EDGES){
		makelatticesprings(lt,ob->soft->bspring,ob->softflag & OB_SB_QUADS,ob);
		build_bps_springlist(ob); /* link bps to springs */
	}
}

/* makes totally fresh start situation */
static void curve_surf_to_softbody(Object *ob)
{
	Curve *cu= ob->data;
	SoftBody *sb;
	BodyPoint *bp;
	BodySpring *bs;
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bpnt;
	float goalfac;
	int a, curindex=0;
	int totvert, totspring = 0, setgoal=0;
	
	totvert= count_curveverts(&cu->nurb);
	
	if (ob->softflag & OB_SB_EDGES){
		if(ob->type==OB_CURVE) {
			totspring= totvert - BLI_countlist(&cu->nurb);
		}
	}
	
	/* renew ends with ob->soft with points and edges, also checks & makes ob->soft */
	renew_softbody(ob, totvert, totspring);
	sb= ob->soft;	/* can be created in renew_softbody() */
		
	/* set vars now */
	goalfac= ABS(sb->maxgoal - sb->mingoal);
	bp= sb->bpoint;
	bs= sb->bspring;
	
	/* weights from bpoints, same code used as for mesh vertices */
	if((ob->softflag & OB_SB_GOAL) && sb->vertgroup)
		setgoal= 1;
		
	for(nu= cu->nurb.first; nu; nu= nu->next) {
		if(nu->bezt) {
			for(bezt=nu->bezt, a=0; a<nu->pntsu; a++, bezt++, bp+=3, curindex+=3) {
				if(setgoal) {
					bp->goal= sb->mingoal + bezt->weight*goalfac;
					/* a little ad hoc changing the goal control to be less *sharp* */
					bp->goal = (float)pow(bp->goal, 4.0f);
					
					/* all three triples */
					(bp+1)->goal= bp->goal;
					(bp+2)->goal= bp->goal;
				}
				
				if(totspring) {
					if(a>0) {
						bs->v1= curindex-1;
						bs->v2= curindex;
						bs->strength= 1.0;
						bs->order=1;
						bs->len= globallen( (bezt-1)->vec[2], bezt->vec[0], ob );
						bs++;
					}
					bs->v1= curindex;
					bs->v2= curindex+1;
					bs->strength= 1.0;
					bs->order=1;
					bs->len= globallen( bezt->vec[0], bezt->vec[1], ob );
					bs++;
					
					bs->v1= curindex+1;
					bs->v2= curindex+2;
					bs->strength= 1.0;
					bs->order=1;
					bs->len= globallen( bezt->vec[1], bezt->vec[2], ob );
					bs++;
				}
			}
		}
		else {
			for(bpnt=nu->bp, a=0; a<nu->pntsu*nu->pntsv; a++, bpnt++, bp++, curindex++) {
				if(setgoal) {
					bp->goal= sb->mingoal + bpnt->weight*goalfac;
					/* a little ad hoc changing the goal control to be less *sharp* */
					bp->goal = (float)pow(bp->goal, 4.0f);
				}
				if(totspring && a>0) {
					bs->v1= curindex-1;
					bs->v2= curindex;
					bs->strength= 1.0;
					bs->order=1;
					bs->len= globallen( (bpnt-1)->vec, bpnt->vec , ob );
					bs++;
				}
			}
		}
	}
	
	if(totspring)
	{
		build_bps_springlist(ob); /* link bps to springs */
		if (ob->softflag & OB_SB_SELF) {calculate_collision_balls(ob);}
	}
}


/* copies softbody result back in object */
static void softbody_to_object(Object *ob, float (*vertexCos)[3], int numVerts, int local)
{
	BodyPoint *bp= ob->soft->bpoint;
	int a;

	/* inverse matrix is not uptodate... */
	Mat4Invert(ob->imat, ob->obmat);
	
	for(a=0; a<numVerts; a++, bp++) {
		VECCOPY(vertexCos[a], bp->pos);
		if(local==0) 
			Mat4MulVecfl(ob->imat, vertexCos[a]);	/* softbody is in global coords, baked optionally not */
	}
}

/* return 1 if succesfully baked and applied step */
static int softbody_baked_step(Object *ob, float framenr, float (*vertexCos)[3], int numVerts)
{
	SoftBody *sb= ob->soft;
	SBVertex *key0, *key1, *key2, *key3;
	BodyPoint *bp;
	float data[4], sfra, efra, cfra, dfra, fac;	/* start, end, current, delta */
	int ofs1, a;

	/* precondition check */
	if(sb==NULL || sb->keys==NULL || sb->totkey==0) return 0;
	/* so we got keys, but no bodypoints... even without simul we need it for the bake */	 
	if(sb->bpoint==NULL) sb->bpoint= MEM_callocN( sb->totpoint*sizeof(BodyPoint), "bodypoint");	 
 	
	/* convert cfra time to system time */
	sfra= (float)sb->sfra;
	cfra= bsystem_time(ob, framenr, 0.0);
	efra= (float)sb->efra;
	dfra= (float)sb->interval;

	/* offset in keys array */
	ofs1= (int)floor( (cfra-sfra)/dfra );

	if(ofs1 < 0) {
		key0=key1=key2=key3= *sb->keys;
	}
	else if(ofs1 >= sb->totkey-1) {
		key0=key1=key2=key3= *(sb->keys+sb->totkey-1);
	}
	else {
		key1= *(sb->keys+ofs1);
		key2= *(sb->keys+ofs1+1);

		if(ofs1>0) key0= *(sb->keys+ofs1-1);
		else key0= key1;
		
		if(ofs1<sb->totkey-2) key3= *(sb->keys+ofs1+2);
		else key3= key2;
	}
	
	sb->ctime= cfra;	/* needed? */
	
	/* timing */
	fac= ((cfra-sfra)/dfra) - (float)ofs1;
	CLAMP(fac, 0.0, 1.0);
	set_four_ipo(fac, data, KEY_BSPLINE);
	if (key0&&key1&&key2&&key3) // may be null because we SHIFT_ESCAPED 
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++, key0++, key1++, key2++, key3++) {
		bp->pos[0]= data[0]*key0->vec[0] +  data[1]*key1->vec[0] + data[2]*key2->vec[0] + data[3]*key3->vec[0];
		bp->pos[1]= data[0]*key0->vec[1] +  data[1]*key1->vec[1] + data[2]*key2->vec[1] + data[3]*key3->vec[1];
		bp->pos[2]= data[0]*key0->vec[2] +  data[1]*key1->vec[2] + data[2]*key2->vec[2] + data[3]*key3->vec[2];
	}
	
	softbody_to_object(ob, vertexCos, numVerts, sb->local);
	
	return 1;
}

/* only gets called after succesfully doing softbody_step */
/* already checked for OB_SB_BAKE flag */
static void softbody_baked_add(Object *ob, float framenr)
{
	SoftBody *sb= ob->soft;
	SBVertex *key;
	BodyPoint *bp;
	float sfra, efra, cfra, dfra, fac1;	/* start, end, current, delta */
	int ofs1, a;
	
	/* convert cfra time to system time */
	sfra= (float)sb->sfra;
	fac1= ob->sf; ob->sf= 0.0f;	/* disable startframe */
	cfra= bsystem_time(ob, framenr, 0.0);
	ob->sf= fac1;
	efra= (float)sb->efra;
	dfra= (float)sb->interval;
	
	if(sb->totkey==0) {
		if(sb->sfra >= sb->efra) return;		/* safety, UI or py setting allows */
		if(sb->interval<1) sb->interval= 1;		/* just be sure */
		
		sb->totkey= 1 + (int)(ceil( (efra-sfra)/dfra ) );
		sb->keys= MEM_callocN( sizeof(void *)*sb->totkey, "sb keys");
	}
	
	/* inverse matrix might not be uptodate... */
	Mat4Invert(ob->imat, ob->obmat);
	
	/* now find out if we have to store a key */
	
	/* offset in keys array */
	if(cfra>=(efra)) {
		ofs1= sb->totkey-1;
		fac1= 0.0;
	}
	else {
		ofs1= (int)floor( (cfra-sfra)/dfra );
		fac1= ((cfra-sfra)/dfra) - (float)ofs1;
	}	
	if( fac1 < 1.0/dfra ) {
		
		key= *(sb->keys+ofs1);
		if(key == NULL) {
			*(sb->keys+ofs1)= key= MEM_mallocN(sb->totpoint*sizeof(SBVertex), "softbody key");
			
			for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++, key++) {
				VECCOPY(key->vec, bp->pos);
				if(sb->local)
					Mat4MulVecfl(ob->imat, key->vec);
			}
		}
	}
}
/* +++ ************ maintaining scratch *************** */
void sb_new_scratch(SoftBody *sb)
{
	if (!sb) return;
	sb->scratch = MEM_callocN(sizeof(SBScratch), "SBScratch");
	sb->scratch->colliderhash = BLI_ghash_new(BLI_ghashutil_ptrhash,BLI_ghashutil_ptrcmp);
	sb->scratch->bodyface = NULL;
	sb->scratch->totface = 0;
	sb->scratch->aabbmax[0]=sb->scratch->aabbmax[1]=sb->scratch->aabbmax[2] = 1.0e30f;
	sb->scratch->aabbmin[0]=sb->scratch->aabbmin[1]=sb->scratch->aabbmin[2] = -1.0e30f;
	
}
/* --- ************ maintaining scratch *************** */

/* ************ Object level, exported functions *************** */

/* allocates and initializes general main data */
SoftBody *sbNew(void)
{
	SoftBody *sb;
	
	sb= MEM_callocN(sizeof(SoftBody), "softbody");
	
	sb->mediafrict= 0.5f; 
	sb->nodemass= 1.0f;
	sb->grav= 9.8f; 
	sb->physics_speed= 1.0f;
	sb->rklimit= 0.1f;

	sb->goalspring= 0.5f; 
	sb->goalfrict= 0.0f; 
	sb->mingoal= 0.0f;  
	sb->maxgoal= 1.0f;
	sb->defgoal= 0.7f;
	
	sb->inspring= 0.5f;
	sb->infrict= 0.5f; 
	
	sb->interval= 10;
	sb->sfra= G.scene->r.sfra;
	sb->efra= G.scene->r.efra;

	sb->colball  = 0.49f;
	sb->balldamp = 0.50f;
	sb->ballstiff= 1.0f;
	sb->sbc_mode = 1;


	sb->minloops = 10;

	sb->choke = 3;
	sb_new_scratch(sb);
	return sb;
}

/* frees all */
void sbFree(SoftBody *sb)
{
	free_softbody_intern(sb);
	MEM_freeN(sb);
}


/* makes totally fresh start situation */
void sbObjectToSoftbody(Object *ob)
{
	ob->softflag |= OB_SB_REDO;

	free_softbody_intern(ob->soft);
}

static int object_has_edges(Object *ob) 
{
	if(ob->type==OB_MESH) {
		return ((Mesh*) ob->data)->totedge;
	}
	else if(ob->type==OB_LATTICE) {
		return 1;
	}
	else {
		return 0;
	}
}

/* SB global visible functions */ 
void sbSetInterruptCallBack(int (*f)(void))
{
	SB_localInterruptCallBack = f;
}


/* simulates one step. framenr is in frames */
void sbObjectStep(Object *ob, float framenr, float (*vertexCos)[3], int numVerts)
{
	SoftBody *sb;
	BodyPoint *bp;
	int a;
	float dtime,ctime,forcetime,err;

	/* baking works with global time */
	if(!(ob->softflag & OB_SB_BAKEDO) )
		if(softbody_baked_step(ob, framenr, vertexCos, numVerts) ) return;


	/* This part only sets goals and springs, based on original mesh/curve/lattice data.
	Copying coordinates happens in next chunk by setting softbody flag OB_SB_RESET */
	/* remake softbody if: */
	if(		(ob->softflag & OB_SB_REDO) ||		/* signal after weightpainting */
		(ob->soft==NULL) ||					/* just to be nice we allow full init */
		(ob->soft->bpoint==NULL) || 		/* after reading new file, or acceptable as signal to refresh */
		(numVerts!=ob->soft->totpoint) ||	/* should never happen, just to be safe */
		((ob->softflag & OB_SB_EDGES) && !ob->soft->bspring && object_has_edges(ob))) /* happens when in UI edges was set */
	{
		switch(ob->type) {
		case OB_MESH:
			mesh_to_softbody(ob);
			break;
		case OB_LATTICE:
			lattice_to_softbody(ob);
			break;
		case OB_CURVE:
		case OB_SURF:
			curve_surf_to_softbody(ob);
			break;
		default:
			renew_softbody(ob, numVerts, 0);
			break;
		}

		/* still need to update to correct vertex locations, happens on next step */
		ob->softflag |= OB_SB_RESET; 
		ob->softflag &= ~OB_SB_REDO;
	}

	sb= ob->soft;

	/* still no points? go away */
	if(sb->totpoint==0) return;


	/* checking time: */

	ctime= bsystem_time(ob, framenr, 0.0);

	if (ob->softflag&OB_SB_RESET) {
		dtime = 0.0;
	} else {
		dtime= ctime - sb->ctime;
	}

	/* the simulator */

	/* update the vertex locations */
	if (dtime!=0.0) {
		for(a=0,bp=sb->bpoint; a<numVerts; a++, bp++) {
			/* store where goals are now */ 
			VECCOPY(bp->origS, bp->origE);
			/* copy the position of the goals at desired end time */
			VECCOPY(bp->origE, vertexCos[a]);
			/* vertexCos came from local world, go global */
			Mat4MulVecfl(ob->obmat, bp->origE); 
			/* just to be save give bp->origT a defined value
			will be calulated in interpolate_exciter()*/
			VECCOPY(bp->origT, bp->origE); 
		}
	}

	if((ob->softflag&OB_SB_RESET) ||	/* got a reset signal */
		(dtime<0.0) ||					/* back in time */
		(dtime>=9.9*G.scene->r.framelen) /* too far forward in time --> goals won't be accurate enough */
		)
	{
		for(a=0,bp=sb->bpoint; a<numVerts; a++, bp++) {
			VECCOPY(bp->pos, vertexCos[a]);
			Mat4MulVecfl(ob->obmat, bp->pos);  /* yep, sofbody is global coords*/
			VECCOPY(bp->origS, bp->pos);
			VECCOPY(bp->origE, bp->pos);
			VECCOPY(bp->origT, bp->pos);
			bp->vec[0]= bp->vec[1]= bp->vec[2]= 0.0f;

			/* the bp->prev*'s are for rolling back from a canceled try to propagate in time
			adaptive step size algo in a nutshell:
			1.  set sheduled time step to new dtime
			2.  try to advance the sheduled time step, beeing optimistic execute it
			3.  check for success
			3.a we 're fine continue, may be we can increase sheduled time again ?? if so, do so! 
			3.b we did exceed error limit --> roll back, shorten the sheduled time and try again at 2.
			4.  check if we did reach dtime 
			4.a nope we need to do some more at 2.
			4.b yup we're done
			*/

			VECCOPY(bp->prevpos, bp->pos);
			VECCOPY(bp->prevvec, bp->vec);
			VECCOPY(bp->prevdx, bp->vec);
			VECCOPY(bp->prevdv, bp->vec);
		}
        /* make a nice clean scratch struc */
		free_scratch(sb); /* clear if any */
		sb_new_scratch(sb); /* make a new */
	    sb->scratch->needstobuildcollider=1; 


        /* copy some info to scratch */
		switch(ob->type) {
		case OB_MESH:
			if (ob->softflag & OB_SB_FACECOLL) mesh_faces_to_scratch(ob);
			break;
		case OB_LATTICE:
			break;
		case OB_CURVE:
		case OB_SURF:
			break;
		default:
			break;
		}



		ob->softflag &= ~OB_SB_RESET;
	}
	else if(dtime>0.0) {
	    double sct,sst=PIL_check_seconds_timer();
		ccd_update_deflector_hache(ob,sb->scratch->colliderhash);


			if(sb->scratch->needstobuildcollider){
				if (query_external_colliders(ob)){
					ccd_build_deflector_hache(ob,sb->scratch->colliderhash);
				}
				sb->scratch->needstobuildcollider=0;
			}


			if (TRUE) {	/*  */
				/* special case of 2nd order Runge-Kutta type AKA Heun */
				float forcetimemax = 1.0f;
				float forcetimemin = 0.001f;
				float timedone =0.0; /* how far did we get without violating error condition */
									 /* loops = counter for emergency brake
									 * we don't want to lock up the system if physics fail
				*/
				int loops =0 ; 
				SoftHeunTol = sb->rklimit; /* humm .. this should be calculated from sb parameters and sizes */
				if (sb->minloops > 0) forcetimemax = 1.0f / sb->minloops;
				
				if (sb->maxloops > 0) forcetimemin = 1.0f / sb->maxloops;
                
				
				//forcetime = dtime; /* hope for integrating in one step */
				forcetime =forcetimemax; /* hope for integrating in one step */
				while ( (ABS(timedone) < ABS(dtime)) && (loops < 2000) )
				{
					/* set goals in time */ 
					interpolate_exciter(ob,200,(int)(200.0*(timedone/dtime)));
					
					sb->scratch->flag &= ~SBF_DOFUZZY;
					/* do predictive euler step */
					softbody_calc_forces(ob, forcetime,timedone/dtime);
					softbody_apply_forces(ob, forcetime, 1, NULL);


					/* crop new slope values to do averaged slope step */
					softbody_calc_forces(ob, forcetime,timedone/dtime);
					softbody_apply_forces(ob, forcetime, 2, &err);

					softbody_apply_goalsnap(ob);
					
					if (err > SoftHeunTol) { /* error needs to be scaled to some quantity */
						
						if (forcetime > forcetimemin){
							forcetime = MAX2(forcetime / 2.0f,forcetimemin);
							softbody_restore_prev_step(ob);
							//printf("down,");
						}
						else {
							timedone += forcetime;
						}
					}
					else {
						float newtime = forcetime * 1.1f; /* hope for 1.1 times better conditions in next step */
						
						if (sb->scratch->flag & SBF_DOFUZZY){
							//if (err > SoftHeunTol/(2.0f*sb->fuzzyness)) { /* stay with this stepsize unless err really small */
							newtime = forcetime;
							//}
						}
						else {
							if (err > SoftHeunTol/2.0f) { /* stay with this stepsize unless err really small */
								newtime = forcetime;
							}
						}
						timedone += forcetime;
						newtime=MIN2(forcetimemax,MAX2(newtime,forcetimemin));
						//if (newtime > forcetime) printf("up,");
						if (forcetime > 0.0)
							forcetime = MIN2(dtime - timedone,newtime);
						else 
							forcetime = MAX2(dtime - timedone,newtime);
					}
					loops++;
					if(sb->solverflags & SBSO_MONITOR ){
						sct=PIL_check_seconds_timer();
						if (sct-sst > 0.5f) printf("%3.0f%% \r",100.0f*timedone);
					}
					if (SB_localInterruptCallBack && SB_localInterruptCallBack()) break;

				}
				/* move snapped to final position */
				interpolate_exciter(ob, 2, 2);
				softbody_apply_goalsnap(ob);
				
				//				if(G.f & G_DEBUG){
				if(sb->solverflags & SBSO_MONITOR ){
					if (loops > HEUNWARNLIMIT) /* monitor high loop counts */
						printf("\r       needed %d steps/frame ",loops);
				}
				
			}
			else{
				/* do brute force explicit euler */
				/* removed but left this branch for better integrators / solvers (BM) */
				/* yah! Nicholas Guttenberg (NichG) here is the place to plug in */
			}
			if(sb->solverflags & SBSO_MONITOR ){
				sct=PIL_check_seconds_timer();
				if (sct-sst > 0.5f) printf(" solver time %f %s \r",sct-sst,ob->id.name);
			}
	}

	softbody_to_object(ob, vertexCos, numVerts, 0);
	sb->ctime= ctime;


	if(ob->softflag & OB_SB_BAKEDO) softbody_baked_add(ob, framenr);
}

