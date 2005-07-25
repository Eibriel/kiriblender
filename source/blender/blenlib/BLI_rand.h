/**
 * @file BLI_rand.h
 * 
 * Random number functions.
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
 
#ifndef BLI_RAND_H
#define BLI_RAND_H

	/* RNG is just an abstract random number generator
	 * type that avoids using globals, otherwise identical
	 * to BLI_rand functions below.
	 */
struct RNG;
typedef struct RNG RNG;

struct RNG*	rng_new			(unsigned int seed);
void		rng_free		(struct RNG* rng);

void		rng_seed		(struct RNG* rng, unsigned int seed);
int			rng_getInt		(struct RNG* rng);
double		rng_getDouble	(struct RNG* rng);
float		rng_getFloat	(struct RNG* rng);
void		rng_shuffleArray(struct RNG *rng, void *data, int elemSize, int numElems);

	/** Seed the random number generator */
void	BLI_srand		(unsigned int seed);

	/** Return a pseudo-random number N where 0<=N<(2^31) */
int		BLI_rand		(void);

	/** Return a pseudo-random number N where 0.0<=N<1.0 */
double	BLI_drand		(void);

	/** Return a pseudo-random number N where 0.0f<=N<1.0f */
float	BLI_frand		(void);

	/** Fills a block of memory starting at @a addr
	 * and extending @a len bytes with pseudo-random
	 * contents. This routine does not use nor modify
	 * the state of the BLI random number generator.
	 */
void	BLI_fillrand	(void *addr, int len);

	/** Shuffle an array randomly using the given seed.
	 * contents. This routine does not use nor modify
	 * the state of the BLI random number generator.
	 */
void	BLI_array_randomize	(void *data, int elemSize, int numElems, unsigned int seed);

#endif

