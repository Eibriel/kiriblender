//
//  Filename         : ArbitraryGridDensityProvider.h
//  Author(s)        : Alexander Beels
//  Purpose          : Class to define a cell grid surrounding
//                     the projected image of a scene
//  Date of creation : 2011-2-5
//
///////////////////////////////////////////////////////////////////////////////


//
//  Copyright (C) : Please refer to the COPYRIGHT file distributed 
//   with this source distribution. 
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef ARBITRARYGRIDDENSITYPROVIDER_H
#define ARBITRARYGRIDDENSITYPROVIDER_H

#include "GridDensityProvider.h"

class ArbitraryGridDensityProvider : public GridDensityProvider {
	// Disallow copying and assignment
	ArbitraryGridDensityProvider (const ArbitraryGridDensityProvider& other);
	ArbitraryGridDensityProvider& operator= (const ArbitraryGridDensityProvider& other);

public:
	ArbitraryGridDensityProvider(OccluderSource& source, const real proscenium[4], unsigned numCells);
	ArbitraryGridDensityProvider(OccluderSource& source, const BBox<Vec3r>& bbox, const GridHelpers::Transform& transform, unsigned numCells);
	ArbitraryGridDensityProvider(OccluderSource& source, unsigned numCells);
	virtual ~ArbitraryGridDensityProvider ();

protected:
	unsigned numCells;

private:
	void initialize (const real proscenium[4]);
};

class ArbitraryGridDensityProviderFactory : public GridDensityProviderFactory {
public:
	ArbitraryGridDensityProviderFactory(unsigned numCells);
	~ArbitraryGridDensityProviderFactory ();

	auto_ptr<GridDensityProvider> newGridDensityProvider(OccluderSource& source, const real proscenium[4]);
	auto_ptr<GridDensityProvider> newGridDensityProvider(OccluderSource& source, const BBox<Vec3r>& bbox, const GridHelpers::Transform& transform);
	auto_ptr<GridDensityProvider> newGridDensityProvider(OccluderSource& source);
protected:
	unsigned numCells;
};

#endif // ARBITRARYGRIDDENSITYPROVIDER_H

