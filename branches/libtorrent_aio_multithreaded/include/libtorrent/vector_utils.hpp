/*

Copyright (c) 2012-2013, Arvid Norberg
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef TORRENT_VECTOR_UTILS_HPP_INCLUDE
#define TORRENT_VECTOR_UTILS_HPP_INCLUDE

#include <vector>
#include <algorithm>

namespace libtorrent {

	template <class T>
	typename std::vector<T>::iterator sorted_find(std::vector<T>& container
		, T v)
	{
		typename std::vector<T>::iterator i = std::lower_bound(container.begin()
			, container.end(), v);
		if (i == container.end()) return container.end();
		if (*i != v) return container.end();
		return i;
	}

	template <class T>
	typename std::vector<T>::const_iterator sorted_find(std::vector<T> const& container
		, T v)
	{
		return sorted_find(const_cast<std::vector<T>&>(container), v);
	}

	template<class T>
	void sorted_insert(std::vector<T>& container, T v)
	{
		typename std::vector<T>::iterator i = std::lower_bound(container.begin()
			, container.end(), v);
		container.insert(i, v);
	}
}

#endif
