/*

Copyright (c) 2008, Arvid Norberg
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

#include "libtorrent/lazy_entry.hpp"
#include <iostream>
#include <iomanip>
#include <cstring>

namespace
{
	const float lazy_entry_grow_factor = 1.5f;
	const int lazy_entry_dict_init = 5;
	const int lazy_entry_list_init = 5;
}

namespace libtorrent
{
	int fail_bdecode(lazy_entry& ret)
	{
		ret = lazy_entry();
		return -1;
	}

	// fills in 'val' with what the string between start and the
	// first occurance of the delimiter is interpreted as an int.
	// return the pointer to the delimiter, or 0 if there is a
	// parse error. val should be initialized to zero
	char const* parse_int(char const* start, char const* end, char delimiter, boost::int64_t& val)
	{
		while (start < end && *start != delimiter)
		{
			using namespace std;
			if (!isdigit(*start)) { return 0; }
			val *= 10;
			val += *start - '0';
			++start;
		}
		return start;
	}

	char const* find_char(char const* start, char const* end, char delimiter)
	{
		while (start < end && *start != delimiter) ++start;
		return start;
	}

	// return 0 = success
	int lazy_bdecode(char const* start, char const* end, lazy_entry& ret, int depth_limit)
	{
		ret.clear();
		if (start == end) return 0;

		std::vector<lazy_entry*> stack;

		stack.push_back(&ret);
		while (start < end)
		{
			if (stack.empty()) break; // done!

			lazy_entry* top = stack.back();

			if (int(stack.size()) > depth_limit) return fail_bdecode(ret);
			if (start >= end) return fail_bdecode(ret);
			char t = *start;
			++start;
			if (start >= end && t != 'e') return fail_bdecode(ret);

			switch (top->type())
			{
				case lazy_entry::dict_t:
				{
					if (t == 'e')
					{
						top->set_end(start);
						stack.pop_back();
						continue;
					}
					boost::int64_t len = t - '0';
					start = parse_int(start, end, ':', len);
					if (start == 0 || start + len + 3 > end || *start != ':') return fail_bdecode(ret);
					++start;
					if (start == end) fail_bdecode(ret);
					lazy_entry* ent = top->dict_append(start);
					start += len;
					if (start >= end) fail_bdecode(ret);
					stack.push_back(ent);
					t = *start;
					++start;
					break;
				}
				case lazy_entry::list_t:
				{
					if (t == 'e')
					{
						top->set_end(start);
						stack.pop_back();
						continue;
					}
					lazy_entry* ent = top->list_append();
					stack.push_back(ent);
					break;
				}
				default: break;
			}

			top = stack.back();
			switch (t)
			{
				case 'd':
					top->construct_dict(start - 1);
					continue;
				case 'l':
					top->construct_list(start - 1);
					continue;
				case 'i':
				{
					char const* int_start = start;
					start = find_char(start, end, 'e');
					top->construct_int(int_start, start - int_start);
					if (start == end) return fail_bdecode(ret);
					TORRENT_ASSERT(*start == 'e');
					++start;
					stack.pop_back();
					continue;
				}
				default:
				{
					using namespace std;
					if (!isdigit(t)) return fail_bdecode(ret);

					boost::int64_t len = t - '0';
					start = parse_int(start, end, ':', len);
					if (start == 0 || start + len + 1 > end || *start != ':') return fail_bdecode(ret);
					++start;
					top->construct_string(start, int(len));
					stack.pop_back();
					start += len;
					continue;
				}
			}
			return 0;
		}
		return 0;
	}

	size_type lazy_entry::int_value() const
	{
		TORRENT_ASSERT(m_type == int_t);
		boost::int64_t val = 0;
		bool negative = false;
		if (*m_data.start == '-') negative = true;
		parse_int(negative?m_data.start+1:m_data.start, m_data.start + m_size, 'e', val);
		if (negative) val = -val;
		return val;
	}

	lazy_entry* lazy_entry::dict_append(char const* name)
	{
		TORRENT_ASSERT(m_type == dict_t);
		TORRENT_ASSERT(m_size <= m_capacity);
		if (m_capacity == 0)
		{
			int capacity = lazy_entry_dict_init;
			m_data.dict = new (std::nothrow) std::pair<char const*, lazy_entry>[capacity];
			if (m_data.dict == 0) return 0;
			m_capacity = capacity;
		}
		else if (m_size == m_capacity)
		{
			int capacity = m_capacity * lazy_entry_grow_factor;
			std::pair<char const*, lazy_entry>* tmp = new (std::nothrow) std::pair<char const*, lazy_entry>[capacity];
			if (tmp == 0) return 0;
			std::memcpy(tmp, m_data.dict, sizeof(std::pair<char const*, lazy_entry>) * m_size);
			for (int i = 0; i < m_size; ++i) m_data.dict[i].second.release();
			delete[] m_data.dict;
			m_data.dict = tmp;
			m_capacity = capacity;
		}

		TORRENT_ASSERT(m_size < m_capacity);
		std::pair<char const*, lazy_entry>& ret = m_data.dict[m_size++];
		ret.first = name;
		return &ret.second;
	}

	namespace
	{
		// the number of decimal digits needed
		// to represent the given value
		int num_digits(int val)
		{
			int ret = 1;
			while (val >= 10)
			{
				++ret;
				val /= 10;
			}
			return ret;
		}
	}

	void lazy_entry::construct_string(char const* start, int length)
	{
		TORRENT_ASSERT(m_type == none_t);
		m_type = string_t;
		m_data.start = start;
		m_size = length;
		m_begin = start - 1 - num_digits(length);
		m_end = start + length;
	}

	namespace
	{
		// str1 is null-terminated
		// str2 is not, str2 is len2 chars
		bool string_equal(char const* str1, char const* str2, int len2)
		{
			while (len2 > 0)
			{
				if (*str1 != *str2) return false;
				if (*str1 == 0) return false;
				++str1;
				++str2;
				--len2;
			}
			return *str1 == 0;
		}
	}

	std::string lazy_entry::dict_find_string_value(char const* name) const
	{
		lazy_entry const* e = dict_find(name);
		if (e == 0 || e->type() != lazy_entry::string_t) return std::string();
		return e->string_value();
	}

	lazy_entry const* lazy_entry::dict_find_string(char const* name) const
	{
		lazy_entry const* e = dict_find(name);
		if (e == 0 || e->type() != lazy_entry::string_t) return 0;
		return e;
	}

	size_type lazy_entry::dict_find_int_value(char const* name, size_type default_val) const
	{
		lazy_entry const* e = dict_find(name);
		if (e == 0 || e->type() != lazy_entry::int_t) return default_val;
		return e->int_value();
	}

	lazy_entry const* lazy_entry::dict_find_dict(char const* name) const
	{
		lazy_entry const* e = dict_find(name);
		if (e == 0 || e->type() != lazy_entry::dict_t) return 0;
		return e;
	}

	lazy_entry const* lazy_entry::dict_find_list(char const* name) const
	{
		lazy_entry const* e = dict_find(name);
		if (e == 0 || e->type() != lazy_entry::list_t) return 0;
		return e;
	}

	lazy_entry* lazy_entry::dict_find(char const* name)
	{
		TORRENT_ASSERT(m_type == dict_t);
		for (int i = 0; i < m_size; ++i)
		{
			std::pair<char const*, lazy_entry> const& e = m_data.dict[i];
			if (string_equal(name, e.first, e.second.m_begin - e.first))
				return &m_data.dict[i].second;
		}
		return 0;
	}

	lazy_entry* lazy_entry::list_append()
	{
		TORRENT_ASSERT(m_type == list_t);
		TORRENT_ASSERT(m_size <= m_capacity);
		if (m_capacity == 0)
		{
			int capacity = lazy_entry_list_init;
			m_data.list = new (std::nothrow) lazy_entry[capacity];
			if (m_data.list == 0) return 0;
			m_capacity = capacity;
		}
		else if (m_size == m_capacity)
		{
			int capacity = m_capacity * lazy_entry_grow_factor;
			lazy_entry* tmp = new (std::nothrow) lazy_entry[capacity];
			if (tmp == 0) return 0;
			std::memcpy(tmp, m_data.list, sizeof(lazy_entry) * m_size);
			for (int i = 0; i < m_size; ++i) m_data.list[i].release();
			delete[] m_data.list;
			m_data.list = tmp;
			m_capacity = capacity;
		}

		TORRENT_ASSERT(m_size < m_capacity);
		return m_data.list + (m_size++);
	}

	std::string lazy_entry::list_string_value_at(int i) const
	{
		lazy_entry const* e = list_at(i);
		if (e == 0 || e->type() != lazy_entry::string_t) return std::string();
		return e->string_value();
	}

	size_type lazy_entry::list_int_value_at(int i, size_type default_val) const
	{
		lazy_entry const* e = list_at(i);
		if (e == 0 || e->type() != lazy_entry::int_t) return default_val;
		return e->int_value();
	}

	void lazy_entry::clear()
	{
		switch (m_type)
		{
			case list_t: delete[] m_data.list; break;
			case dict_t: delete[] m_data.dict; break;
			default: break;
		}
		m_data.start = 0;
		m_size = 0;
		m_capacity = 0;
		m_type = none_t;
	}

	std::pair<char const*, int> lazy_entry::data_section() const
	{
		typedef std::pair<char const*, int> return_t;
		return return_t(m_begin, m_end - m_begin);
	}

	std::ostream& operator<<(std::ostream& os, lazy_entry const& e)
	{
		switch (e.type())
		{
			case lazy_entry::none_t: return os << "none";
			case lazy_entry::int_t: return os << std::dec << std::setw(0) << e.int_value();
			case lazy_entry::string_t:
			{
				bool printable = true;
				char const* str = e.string_ptr();
				for (int i = 0; i < e.string_length(); ++i)
				{
					using namespace std;
					if (isprint((unsigned char)str[i])) continue;
					printable = false;
					break;
				}
				os << "'";
				if (printable) return os << e.string_value() << "'";
				for (int i = 0; i < e.string_length(); ++i)
					os << std::hex << std::setfill('0') << std::setw(2)
					<< int((unsigned char)(str[i]));
				return os << "'";
			}
			case lazy_entry::list_t:
			{
				os << "[";
				bool one_liner = (e.list_size() == 0
					|| (e.list_at(0)->type() == lazy_entry::int_t
						&& e.list_size() < 20)
					|| (e.list_at(0)->type() == lazy_entry::string_t
						&& (e.list_at(0)->string_length() < 10
							|| e.list_size() < 2))
						&& e.list_size() < 5);
				if (!one_liner) os << "\n";
				for (int i = 0; i < e.list_size(); ++i)
				{
					if (i == 0 && one_liner) os << " ";
					os << *e.list_at(i);
					if (i < e.list_size() - 1) os << (one_liner?", ":",\n");
					else os << (one_liner?" ":"\n");
				}
				return os << "]";
			}
			case lazy_entry::dict_t:
			{
				os << "{";
				bool one_liner = (e.dict_size() == 0
					|| e.dict_at(0).second->type() == lazy_entry::int_t
					|| (e.dict_at(0).second->type() == lazy_entry::string_t
						&& e.dict_at(0).second->string_length() < 30)
					|| e.dict_at(0).first.size() < 10)
					&& e.dict_size() < 5;

				if (!one_liner) os << "\n";
				for (int i = 0; i < e.dict_size(); ++i)
				{
					if (i == 0 && one_liner) os << " ";
					std::pair<std::string, lazy_entry const*> ent = e.dict_at(i);
					os << "'" << ent.first << "': " << *ent.second;
					if (i < e.dict_size() - 1) os << (one_liner?", ":",\n");
					else os << (one_liner?" ":"\n");
				}
				return os << "}";
			}
		}
		return os;
	}

};
