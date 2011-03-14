//
//  Filename         : PointerSequence.h
//  Author(s)        : Alexander Beels
//  Purpose          : Class to define a cell grid surrounding
//                     the projected image of a scene
//  Date of creation : 22/11/2010
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

//
// Simple RAII wrappers for std:: sequential containers
//
///////////////////////////////////////////////////////////////////////////////

//
// PointerSequence
//
// Produces a wrapped version of a sequence type (std::vector, std::deque, std::list)
// that will take ownership of pointers tht it stores.  Those pointers will be deleted
// in its destructor.
//
// Because the contained pointers are wholly owned by the sequence, you cannot make a
// copy of the sequence.  Making a copy would result in a double free.
//
// This is a no-frills class that provides no additional facilities.  The user is
// responsible for managing any pointers that are removed from the list, and for making 
// sure that any pointers contained in the class are not deleted elsewhere.  Because 
// this class does no reference counting, the user must also make sure that any pointer
// appears only once in the sequence.
//
// If more sophisticated facilities are needed, use tr1::shared_ptr or boost::shared_ptr.
// This class is only intended to allow one to eke by in projects where tr1 or boost are
// not available.
//
// Usage: The template takes two parameters, the standard container, and the class held
// in the container.  This is a limitation of C++ templates, where T::iterator is not a
// type when T is a template parameter.  If anyone knows a way around this
// limitation, then the second parameter can be eliminated.
//
// Example:
//   PointerSequence<vector<Widget*>, Widget*> v;
//   v.push_back(new Widget);
//   cout << v[0] << endl; // operator[] is provided by std::vector, not by PointerSequence
//   v.destroy(); // Deletes all pointers in sequence and sets them to NULL.
//
// The idiom for removing a pointer from a sequence is:
//   Widget* w = v[3];
//   v.erase(v.begin() + 3); // or v[3] = 0;
// The user is now responsible for disposing of w properly.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef  POINTERSEQUENCE_H
# define POINTERSEQUENCE_H

#include <algorithm>

template <typename C, typename T>
class PointerSequence : public C {
	PointerSequence (PointerSequence& other);
	PointerSequence& operator= (PointerSequence& other);
	static void destroyer (T t) {
		delete t;
	}
public:
	PointerSequence () {};
	~PointerSequence () {
		destroy();
	}
	void destroy () {
		for_each (this->begin(), this->end(), destroyer);
	}
};

#endif // POINTERSEQUENCE_H

