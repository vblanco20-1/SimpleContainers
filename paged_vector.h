

#pragma once
//try to avoid the iterator include as that one includes too much
#if  defined _MSC_VER
#include <xutility>
#else
#include <iterator>
#endif

template<typename T>
class paged_vector {
	using value_type = T;
	using size_type = size_t;
	using reference = value_type&;
	using pointer = value_type*;
	using const_reference = const value_type&;
	static constexpr int bits = 6;
	static constexpr int page_size = 1 << bits;
	static constexpr size_t mask = (page_size - 1);
	struct page {
		T elements[page_size];
	};
public:

	paged_vector() noexcept = default;
	paged_vector(const paged_vector& other) noexcept
	{
		resize(other.last_index);

		for (int i = 0; i < other.last_index; i++) {
			(*this)[i] = other[i];
		}
	}
	paged_vector& operator=(const paged_vector&other)
	{
		resize(other.last_index);

		for (int i = 0; i < other.last_index; i++) {
			(*this)[i] = other[i];
		}

		return *this;
	};

	~paged_vector() noexcept {
		clear();
	}
	paged_vector(paged_vector&& other) noexcept{
		last_index = other.last_index;
		page_array = other.page_array;		
	    page_capacity = other.page_capacity;

		other.page_array = nullptr;
		other.last_index = 0;
		other.page_capacity = 0;
	}
	paged_vector& operator=(paged_vector&& other) {
		clear();

		last_index = other.last_index;
		page_array = other.page_array;
		page_capacity = other.page_capacity;

		other.page_array = nullptr;
		other.last_index = 0;
		other.page_capacity = 0;

		return *this;
	}

	class iterator
	{
	public:
		using iterator_category = std::random_access_iterator_tag;
		using difference_type = int;
		using value_type = T;
		using reference = value_type&;
		using pointer = value_type*;

		iterator(int idx, paged_vector<T>* owner) : _idx(idx), _owner(owner) { }

		iterator& operator++() {
			return ++_idx, * this;
		}

		iterator operator++(int) {
			iterator orig = *this;
			return operator++(), orig;
		}

		iterator& operator+=(const difference_type value) {
			_idx += value;
			return *this;
		}

		iterator operator+(const difference_type value) const {
			iterator copy = *this;
			return (copy += value);
		}

		iterator& operator--() {
			return --_idx, * this;
		}

		iterator operator--(int) {
			iterator orig = *this;
			return operator--(), orig;
		}

		iterator& operator-=(const difference_type value) {
			_idx -= value;
			return *this;
		}

		iterator operator-(const difference_type value) const {
			iterator copy = *this;
			return (copy -= value);
		}

		difference_type operator-(const iterator& rhs) const {
			return difference_type(_idx - rhs._idx);
		}

		reference operator[](const difference_type value) const {
			int index = _idx + value;
			return (*_owner)[_idx];
		}

		reference operator*() { return (*_owner)[_idx]; }
		pointer operator->() { return &((*_owner)[_idx]); }
		bool operator==(const iterator& rhs) const { return _idx == rhs._idx && _owner == rhs._owner; }
		bool operator!=(const iterator& rhs) const { return !(*this == rhs); }
		bool operator<(const iterator& rhs) const { return _idx < rhs._idx; }
		bool operator>(const iterator& rhs)const { return _idx > rhs._idx; }
		bool operator<=(const iterator& rhs) const { return !(*this > rhs); }
		bool operator>=(const iterator& rhs) const { return !(*this < rhs); }


	private:
		int _idx;
		paged_vector* _owner;
	};

	iterator begin() {
		return iterator(0, this);
	}
	iterator end() {
		return iterator(static_cast<int>(last_index), this);
	}

	reference operator[](size_type index) {
		return page_array[index >> bits]->elements[index & mask];
	}
	const_reference operator[](size_type index) const {
		return page_array[index >> bits]->elements[index & mask];
	}
	size_type size() {
		return last_index;
	}
	size_type capacity()
	{
		return num_pages() * page_size;
	}
	void reserve(size_type size) {
		if (size > last_index) {
			resize_pages(page_divide_ceil(size));
		}
	}
	void clear() {
		if (page_array) {
			for (auto i = 0; i < this->page_capacity; i++) {
				destroy_page(i);
			}
		}
		last_index = 0;
		page_capacity = 0;
		page_array = nullptr;
	}
	void resize(size_type size) {
		resize(size,value_type{});
	}

	void resize(size_type size, const value_type& value) {

		auto old_size = last_index;

		auto new_pages = page_divide_ceil(size);

		if (size < last_index) {
			delete_range(size, last_index);
		}

		resize_pages(new_pages);
		
		if (size > old_size) {
			init_range(old_size, size, value);
		}

		last_index = size;
	}
	template<typename V>
	void push_back(const V& value) {
		const auto idx = last_index;
		resize(last_index + 1);
		operator[](idx) = value;
	}
	void pop_back() {
		resize(last_index - 1);
	}

	T& back() {
		return operator[](last_index - 1);
	}
private:

	static size_t page_divide_ceil(size_t idx) {
		return (idx + page_size - 1) / page_size;
	}

	size_type num_pages() const{
		return last_index >> bits;
	}
	void destroy_page(size_t index) {

		delete page_array[index];
		page_array[index] = nullptr;
	}
	void delete_range(size_t start, size_t end) {
		//destroy all elements
		for (auto i = start; i < end; i++) {
			(*this)[i].~T();
		}
	}
	void init_range(size_t start, size_t end, const value_type& value) {
		for (auto i = start; i < end; i++) {
			(*this)[i] = value;
		}
	}
	void create_page(size_t index) {
		page_array[index] = new page;
	}
	void resize_pages(size_type new_pages) {
		//no realloc needed
		if (new_pages < page_capacity) {
			//delete pages
			if (new_pages < num_pages()) {
				for (auto i = new_pages; i < num_pages(); i++) {
					destroy_page(i);
				}
			}
		}
		else if (new_pages > page_capacity) {
			page** new_array = new page * [new_pages];
			for (auto i = 0; i < page_capacity; i++) {
				new_array[i] = page_array[i];
			}
			delete[] page_array;
			page_array = new_array;
			for (auto i = page_capacity; i < new_pages; i++) {
				create_page(i);
			}
			page_capacity = new_pages;
		}
	}


	size_type last_index{ 0 };
	page** page_array{ nullptr };

	//size of page array
	size_type page_capacity{ 0 };
};
