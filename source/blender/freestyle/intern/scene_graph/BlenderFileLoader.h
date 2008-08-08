#ifndef  BLENDER_FILE_LOADER_H
# define BLENDER_FILE_LOADER_H

# include <string.h>              
# include <float.h>

# include "../system/FreestyleConfig.h"
# include "NodeGroup.h"
# include "NodeTransform.h"
# include "NodeShape.h"
# include "IndexedFaceSet.h"
# include "../geometry/BBox.h"
# include "../geometry/Geom.h"
# include "../geometry/GeomCleaner.h"

#ifdef __cplusplus
extern "C" {
#endif

	#include "DNA_material_types.h"
	#include "DNA_scene_types.h"
	#include "render_types.h"
	#include "renderdatabase.h"
	
	#include "BKE_mesh.h"
	#include "BKE_scene.h"
	#include "MTC_matrixops.h"
	#include "MTC_vectorops.h"
	
#ifdef __cplusplus
}
#endif


class NodeGroup;

class LIB_SCENE_GRAPH_EXPORT BlenderFileLoader
{
public:
  /*! Builds a MaxFileLoader */
	BlenderFileLoader(Render *re);
  virtual ~BlenderFileLoader();

  /*! Loads the 3D scene and returns a pointer to the scene root node */
  NodeGroup * Load();

  /*! Gets the number of read faces */
  inline unsigned int numFacesRead() {return _numFacesRead;}

  /*! Gets the smallest edge size read */
  inline real minEdgeSize() {return _minEdgeSize;}

protected:
	void insertShapeNode(ObjectRen *obr, int id);

protected:
	Render* _re;
  NodeGroup* _Scene;
  unsigned _numFacesRead;
  real _minEdgeSize;
};

#endif // BLENDER_FILE_LOADER_H
