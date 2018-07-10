
#include <vector>
#include <unordered_map>
#include <cstdio>
#include <iostream>
#include <cmath>
#include <cassert>
#include <algorithm>
#include <inttypes.h>

using namespace std;



#define FINLINE inline __attribute__((always_inline))



#define BOOL int
#define WINAPI __stdcall
struct LARGE_INTEGER
{
	long long QuadPart;
};
extern "C"
{
BOOL WINAPI QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency);
BOOL WINAPI QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount);
}
double Time()
{
	LARGE_INTEGER count, freq;
	QueryPerformanceCounter(&count);
	QueryPerformanceFrequency(&freq);
	return double(count.QuadPart) / double(freq.QuadPart);
}



void ERROR(const char* expr, const char* file, int line)
{
	fprintf(stderr, "Assertion failed:\n\n\t%s\n\nFile: %s\nLine: %d\n", expr, file, line);
	exit(1);
}
#define ASSERT(x) if (!(x)) { ERROR(#x, __FILE__, __LINE__); }



// http://bitsquid.blogspot.com/2011/09/managing-decoupling-part-4-id-lookup.html - The STL Method
struct Storage_STLMethod
{
	static constexpr const char* NAME = "STLMethod";

	using Handle = uint32_t;

	void Prewarm(size_t num)
	{
		idMap.reserve(num);
	}
	Handle Add(uint64_t v)
	{
		Handle id = idAlloc++;
		uint64_t* data = new uint64_t(v);
		idMap.insert({ id, data });
		return id;
	}
	void Remove(Handle h)
	{
		auto it = idMap.find(h);
		if (it != idMap.end())
		{
			delete it->second;
			idMap.erase(it);
		}
	}
	size_t Count()
	{
		return idMap.size();
	}
	uint64_t* GetDataPtr(Handle h)
	{
		// no validation in release builds
		return idMap.find(h)->second;
	}
	uint64_t AddAllNumbers()
	{
		uint64_t out = 0;
		for (auto p : idMap)
			out += *p.second;
		return out;
	}

	// pointer because normally objects would contain more than one member
	unordered_map<Handle, uint64_t*> idMap;
	uint32_t idAlloc = 0;
};



// https://blog.molecular-matters.com/2013/07/24/adventures-in-data-oriented-design-part-3c-external-references/
struct Storage_SparseDense
{
	static const uint32_t INVALID_INDEX = (1 << 20) - 1;
	static constexpr const char* NAME = "SparseDense";

	struct Handle
	{
		uint32_t at : 20;
		uint32_t gen : 12;
	};

	void Prewarm(size_t num)
	{
		data.reserve(num);
		whereInSparse.reserve(num);
		sparse.reserve(num);
	}
	Handle Add(uint64_t v)
	{
		if (lastFree == INVALID_INDEX)
		{
			// sparse array too short
			Handle ret = { uint32_t(sparse.size()) & 0xfffff, 0 };
			sparse.push_back({ uint32_t(data.size()) & 0xfffff, 0 });
			data.push_back(v);
			whereInSparse.push_back(sparse.size() - 1);
			return ret;
		}
		else
		{
			// take from free list
			uint32_t at = lastFree;
			lastFree = sparse[at].at;
			Handle ret = { at, sparse[at].gen };
			data.push_back(v);
			whereInSparse.push_back(at);
			return ret;
		}
	}
	void Remove(Handle h)
	{
		// no validation in release builds
		uint32_t densePos = sparse[h.at].at;
		if (densePos < data.size() - 1)
		{
			// copy from last
			data[densePos] = data.back();
			sparse[whereInSparse[data.size() - 1]].at = densePos;
		}
		data.pop_back();
		whereInSparse.pop_back();
		sparse[h.at].at = lastFree;
		sparse[h.at].gen++;
		lastFree = h.at;
	}
	size_t Count()
	{
		return data.size();
	}
	uint64_t* GetDataPtr(Handle h)
	{
		// no validation in release builds
		return &data[sparse[h.at].at];
	}
	uint64_t AddAllNumbers()
	{
		uint64_t out = 0;
		for (auto v : data)
			out += v;
		return out;
	}

	vector<uint64_t> data;
	vector<uint32_t> whereInSparse;
	vector<Handle> sparse;
	uint32_t lastFree = INVALID_INDEX;
};



template <class T> T TMAX(T a, T b) { return a > b ? a : b; }
struct PagePool
{
	// Guaranteed alignment: 8 bytes

	// Block when used:
	// [...object...]
	// Block when unused:
	// [pointer to next free block][unused...]

	PagePool(size_t unitSize, size_t pageSize = 8192) :
		_unitSize(TMAX(unitSize, sizeof(void*))),
		_pageSize(((pageSize - sizeof(void*)) / _unitSize) * _unitSize + sizeof(void*))
	{
		ASSERT(unitSize <= pageSize);
		// to trigger first time test failure
		_lastPageInitMarker = static_cast<char*>(_lastPage) + _pageSize;
	}
	~PagePool()
	{
		void* curr = _lastPage;
		while (curr)
		{
			void* next;
			memcpy(&next, curr, sizeof(next));
			free(curr);
			curr = next;
		}
	}
	size_t GetMemoryUsage() const
	{
		return _numPages * _pageSize;
	}
	void* Alloc()
	{
		if (_nextFreeBlock)
		{
			// return block from free list
			void* mem = _nextFreeBlock;
			memcpy(&_nextFreeBlock, mem, sizeof(mem));
			return mem;
		}

		if (_lastPageInitMarker != static_cast<char*>(_lastPage) + _pageSize)
		{
		havePageNow:
			// return block from last allocated page
			void* mem = _lastPageInitMarker;
			_lastPageInitMarker += _unitSize;
			return mem;
		}

		// no free blocks or pages, allocate a page and try again
		_AllocPage();
		goto havePageNow;
	}
	void Free(void* mem)
	{
		memcpy(mem, &_nextFreeBlock, sizeof(_nextFreeBlock));
		_nextFreeBlock = mem;
	}

	void _AllocPage()
	{
		void* page = malloc(_pageSize);
		memcpy(page, &_lastPage, sizeof(_lastPage));
		_lastPage = page;
		_lastPageInitMarker = static_cast<char*>(page) + sizeof(void*);
		_numPages++;
	}

	size_t _unitSize;
	size_t _pageSize;
	size_t _numPages = 0;
	void* _lastPage = nullptr;
	char* _lastPageInitMarker = nullptr;
	void* _nextFreeBlock = nullptr;
};

// Original method (or forgotten sources)
struct Storage_WeakRefs
{
	static constexpr const char* NAME = "WeakRefs";

	struct WeakRef
	{
		static PagePool weakRefPool;
		FINLINE void* operator new (size_t) { return weakRefPool.Alloc(); }
		FINLINE void operator delete (void* p) { weakRefPool.Free(p); }

		uint32_t at;
		uint32_t refCount;
	};
	static void ReleaseWeakRef(WeakRef* weakRef)
	{
		if (--weakRef->refCount <= 0)
			delete weakRef;
	}
	struct Handle
	{
		FINLINE Handle() : weakRef(nullptr) {}
		FINLINE Handle(WeakRef* w) : weakRef(w) { if (w) w->refCount++; }
		FINLINE Handle(const Handle& h) : weakRef(h.weakRef) { if (weakRef) weakRef->refCount++; }
		FINLINE Handle(Handle&& h) : weakRef(h.weakRef) { h.weakRef = nullptr; }
		FINLINE ~Handle()
		{
			if (weakRef)
				ReleaseWeakRef(weakRef);
		}
		Handle& operator = (const Handle& h)
		{
			if (weakRef)
				ReleaseWeakRef(weakRef);
			weakRef = h.weakRef;
			if (weakRef)
				weakRef->refCount++;
			return *this;
		}
		Handle& operator = (Handle&& h)
		{
			if (weakRef)
				ReleaseWeakRef(weakRef);
			weakRef = h.weakRef;
			h.weakRef = nullptr;
			return *this;
		}

		WeakRef* weakRef;
	};

	void Prewarm(size_t num)
	{
		weakRefs.reserve(num);
		data.reserve(num);
	}
	Handle Add(uint64_t v)
	{
		WeakRef* w = new WeakRef;
		w->at = data.size();
		w->refCount = 1;
		weakRefs.push_back(w);
		data.push_back(v);
		return w;
	}
	void Remove(const Handle& h)
	{
		uint32_t at = h.weakRef->at;
		if (at < data.size() - 1)
		{
			data[at] = data.back();
			weakRefs[at] = weakRefs.back();
		}
		ReleaseWeakRef(h.weakRef);
		data.pop_back();
		weakRefs.pop_back();
	}
	size_t Count()
	{
		return data.size();
	}
	uint64_t* GetDataPtr(Handle h)
	{
		// no validation in release builds
		return &data[h.weakRef->at];
	}
	uint64_t AddAllNumbers()
	{
		uint64_t out = 0;
		for (auto v : data)
			out += v;
		return out;
	}

	vector<WeakRef*> weakRefs;
	vector<uint64_t> data;
};
PagePool Storage_WeakRefs::WeakRef::weakRefPool(sizeof(Storage_WeakRefs::WeakRef));



template<class Storage, size_t Count> void RunTests(uint64_t* values)
{
	double t;
	Storage s;

	vector<typename Storage::Handle> handles;
	handles.resize(Count);
	s.Prewarm(Count);

	t = Time();
	for (size_t i = 0; i < Count; ++i)
		handles[i] = s.Add(values[i]);
	double tAdd = Time() - t;
	ASSERT(s.Count() == Count);

	t = Time();
	uint64_t val = s.AddAllNumbers();
	__asm__ __volatile__("" :: "m" (val));
	double tSum = Time() - t;

	t = Time();
	for (size_t i = 0; i < Count; ++i)
		s.Remove(handles[i]);
	double tRemove = Time() - t;
	ASSERT(s.Count() == 0);

	t = Time();
	for (size_t i = 0; i < Count; ++i)
		handles[i] = s.Add(values[i]);
	double tAddAftRem = Time() - t;
	ASSERT(s.Count() == Count);

	t = Time();
	for (size_t i = Count; i > 0; )
		s.Remove(handles[--i]);
	double tRevRemove = Time() - t;
	ASSERT(s.Count() == 0);

	printf("%12s:|%9.4f|%9.4f|%9.4f|%9.4f|%9.4f\n",
		Storage::NAME
		, tAdd * 1000
		, tSum * 1000
		, tRemove * 1000
		, tAddAftRem * 1000
		, tRevRemove * 1000
	);
}



#define COUNT 100000
int main()
{
	puts("### Methods for storing arrays with persistent handles ###");
	puts("### (times are in milliseconds)                        ###");
	printf("item count: %d\n", COUNT);
	puts("");

	vector<uint64_t> values;
	values.resize(COUNT);
	for (int i = 0; i < COUNT; ++i)
		values[i] = (i * 101U) & 0xffffU;

	printf("%12s |%9s|%9s|%9s|%9s|%9s\n", "Method", "Add", "Sum", "Remove", "AddAftRem", "RevRemove");
	RunTests<Storage_STLMethod, COUNT>(values.data());
	RunTests<Storage_SparseDense, COUNT>(values.data());
	RunTests<Storage_WeakRefs, COUNT>(values.data());
}



