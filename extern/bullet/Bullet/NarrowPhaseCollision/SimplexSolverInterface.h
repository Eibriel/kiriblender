/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/



#ifndef SIMPLEX_SOLVER_INTERFACE_H
#define SIMPLEX_SOLVER_INTERFACE_H

#include "SimdVector3.h"
#include "SimdPoint3.h"

#define NO_VIRTUAL_INTERFACE 1
#ifdef NO_VIRTUAL_INTERFACE
#include "VoronoiSimplexSolver.h"
#define SimplexSolverInterface VoronoiSimplexSolver
#else
/// for simplices from 1 to 4 vertices
/// for example Johnson-algorithm or alternative approaches based on
/// voronoi regions or barycentric coordinates
class SimplexSolverInterface
{
	public:
		virtual ~SimplexSolverInterface() {};

	virtual void reset() = 0;

	virtual void addVertex(const SimdVector3& w, const SimdPoint3& p, const SimdPoint3& q) = 0;
	
	virtual bool closest(SimdVector3& v) = 0;

	virtual SimdScalar maxVertex() = 0;

	virtual bool fullSimplex() const = 0;

	virtual int getSimplex(SimdPoint3 *pBuf, SimdPoint3 *qBuf, SimdVector3 *yBuf) const = 0;

	virtual bool inSimplex(const SimdVector3& w) = 0;
	
	virtual void backup_closest(SimdVector3& v) = 0;

	virtual bool emptySimplex() const = 0;

	virtual void compute_points(SimdPoint3& p1, SimdPoint3& p2) = 0;

	virtual int numVertices() const =0;


};
#endif
#endif //SIMPLEX_SOLVER_INTERFACE_H

