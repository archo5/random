
#include <vector>
#include <set>
#include <unordered_set>
#include <cstdio>
#include <cmath>
#include <cassert>
#include <random>
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



void ERROR(const char* expr, const char* func, const char* file, int line)
{
	fprintf(stderr, "\nAssertion failed:\n\n\t%s\n\nFunction: %s\nFile: %s\nLine: %d\n", expr, func, file, line);
	exit(1);
}
#define ASSERT(x) if (!(x)) { ERROR(#x, __PRETTY_FUNCTION__, __FILE__, __LINE__); }



struct Set_CPPSet
{
	static constexpr const char* NAME = "C++Set(O)";

	void Prewarm(size_t s)
	{
		// no such option
		// indices.reserve(s);
	}
	void Add(uint32_t v)
	{
		indices.insert(v);
	}
	void Remove(uint32_t v)
	{
		indices.erase(v);
	}
	size_t Count()
	{
		return indices.size();
	}
	uint64_t ProcessData(uint64_t* data)
	{
		uint64_t out = 0;
		for (auto val : indices)
			out += data[val];
		return out;
	}

	set<uint32_t> indices;
};



struct Set_CPPUoSet
{
	static constexpr const char* NAME = "C++Set(U)";

	void Prewarm(size_t s)
	{
		indices.reserve(s);
	}
	void Add(uint32_t v)
	{
		indices.insert(v);
	}
	void Remove(uint32_t v)
	{
		indices.erase(v);
	}
	size_t Count()
	{
		return indices.size();
	}
	uint64_t ProcessData(uint64_t* data)
	{
		uint64_t out = 0;
		for (auto val : indices)
			out += data[val];
		return out;
	}

	unordered_set<uint32_t> indices;
};



// adapted from https://stackoverflow.com/a/40457313/1648140
template<class T> const T& PassThru(const T& val) { return val; }
template<class NT, class F = decltype(PassThru<NT>), class T, int NumPasses = sizeof(NT)> void TRadixSort(T* begin, T* end, F f = PassThru<NT>, T* mem = nullptr)
{
	size_t mIndex[NumPasses][256] = { 0 }; // count / index matrix
	T* b = mem ? mem : new T[end - begin];
	T* bend = b + (end - begin);
	// generate histograms
	for (T* it = begin; it != end; ++it)
	{
		NT u = f(*it);
		for (size_t j = 0; j < NumPasses; j++)
		{
			mIndex[j][(size_t) (u & 0xff)]++;
			u >>= 8;
		}
	}
	// convert to indices
	for (size_t j = 0; j < NumPasses; j++)
	{
		size_t m = 0;
		for (size_t i = 0; i < 256; i++)
		{
			size_t n = mIndex[j][i];
			mIndex[j][i] = m;
			m += n;
		}
	}
	// radix sort
	for (size_t j = 0; j < NumPasses; j++)
	{
		//  sort by current lsb
		for (T* it = begin; it != end; ++it)
		{
			auto u = f(*it);
			size_t m = (size_t) (u >> (j << 3)) & 0xff;
			b[mIndex[j][m]++] = std::move(*it);
		}
		std::swap(begin, b);
		std::swap(end, bend);
	}
	if (!mem)
		delete[] b;
}

struct Set_CPPUoSet2
{
	static constexpr const char* NAME = "C++Set(U)2";

	void Prewarm(size_t s)
	{
		indices.reserve(s);
		sortedIndices.reserve(s);
	}
	void Add(uint32_t v)
	{
		indices.insert(v);
	}
	void Remove(uint32_t v)
	{
		indices.erase(v);
	}
	size_t Count()
	{
		return indices.size();
	}
	uint64_t ProcessData(uint64_t* data)
	{
		uint64_t out = 0;
		sortedIndices.assign(indices.begin(), indices.end());
		TRadixSort<uint32_t>(sortedIndices.data(), sortedIndices.data() + sortedIndices.size());
		for (auto val : sortedIndices)
			out += data[val];
		return out;
	}

	unordered_set<uint32_t> indices;
	vector<uint32_t> sortedIndices;
};



template<typename T> typename std::vector<T>::iterator insert_sorted(std::vector<T>& vec, const T& item)
{
	return vec.insert(std::upper_bound(vec.begin(), vec.end(), item), item);
}

template<typename T> void erase_sorted(std::vector<T>& vec, const T& item)
{
    auto pr = std::equal_range(std::begin(vec), std::end(vec), item);
    vec.erase(pr.first, pr.second);
}

struct Set_SortedArray
{
	static constexpr const char* NAME = "SortedArray";

	void Prewarm(size_t s)
	{
		indices.reserve(s);
	}
	void Add(uint32_t v)
	{
		if (std::binary_search(indices.begin(), indices.end(), v))
			return;
		insert_sorted(indices, v);
	}
	void Remove(uint32_t v)
	{
		erase_sorted(indices, v);
	}
	size_t Count()
	{
		return indices.size();
	}
	uint64_t ProcessData(uint64_t* data)
	{
		uint64_t out = 0;
		for (auto val : indices)
			out += data[val];
		return out;
	}

	vector<uint32_t> indices;
};



struct Set_ArraySet
{
	static constexpr const char* NAME = "ArraySet";
	const uint32_t INVALID_POS = (uint32_t) -1;

	void Prewarm(size_t s)
	{
		indices.reserve(s);
		positions.reserve(s);
	}
	void Add(uint32_t v)
	{
		if (v >= positions.size())
			positions.resize(v + 1 + positions.size(), INVALID_POS); // excessive resizing to avoid frequent resizing
		else if (positions[v] != INVALID_POS)
			return;
		positions[v] = indices.size();
		indices.push_back(v);
	}
	void Remove(uint32_t v)
	{
		if (v >= positions.size() || positions[v] == INVALID_POS)
			return;
		uint32_t at = positions[v];
		if (at < indices.size() - 1)
		{
			positions[indices.back()] = at;
			indices[at] = indices.back();
		}
		indices.pop_back();
		positions[v] = INVALID_POS;
	}
	size_t Count()
	{
		return indices.size();
	}
	uint64_t ProcessData(uint64_t* data)
	{
		uint64_t out = 0;
		for (auto val : indices)
			out += data[val];
		return out;
	}

	vector<uint32_t> indices;
	vector<uint32_t> positions;
};



struct Set_SortArrSet : Set_ArraySet
{
	static constexpr const char* NAME = "SortArrSet";

	void Prewarm(size_t s)
	{
		Set_ArraySet::Prewarm(s);
		sortedIndices.reserve(s);
	}
	uint64_t ProcessData(uint64_t* data)
	{
		sortedIndices.assign(indices.begin(), indices.end());
		TRadixSort<uint32_t>(sortedIndices.data(), sortedIndices.data() + sortedIndices.size());

		uint64_t out = 0;
		for (auto val : sortedIndices)
			out += data[val];
		return out;
	}

	vector<uint32_t> sortedIndices;
};



struct Set_SparseArrSet
{
	static constexpr const char* NAME = "SparseArrSet";
	const uint32_t INVALID_POS = (uint32_t) -1;

	void Prewarm(size_t s)
	{
		used.reserve(s);
	}
	void Add(uint32_t v)
	{
		if (v >= used.size())
			used.resize(v + 1 + used.size(), 0); // excessive resizing to avoid frequent resizing
		else if (used[v])
			return;
		used[v] = 1;
		count++;
	}
	void Remove(uint32_t v)
	{
		if (v >= used.size() || !used[v])
			return;
		used[v] = 0;
		count--;
	}
	size_t Count()
	{
		return count;
	}
	uint64_t ProcessData(uint64_t* data)
	{
		uint64_t out = 0;
		for (size_t i = 0, which = 0; i < used.size() && which < count; ++i, ++which)
			if (used[i])
				out += data[i];
		return out;
	}

	vector<uint8_t> used;
	uint32_t count = 0;
};



#define BPRED1 {if (rand() % 2) rand();}
#define BPRED6 BPRED1 BPRED1 BPRED1 BPRED1 BPRED1 BPRED1
#define BPRED36 BPRED6 BPRED6 BPRED6 BPRED6 BPRED6 BPRED6
#define BPRED216 BPRED36 BPRED36 BPRED36 BPRED36 BPRED36 BPRED36

template<class Set, int Count> void RunTests(uint32_t* indices, uint64_t* data)
{
	double t;
	Set s;

	s.Prewarm(Count);

	t = Time();
	for (int i = 0; i < Count; ++i)
		s.Add(indices[i]);
	double tAdd = Time() - t;
	ASSERT(s.Count() == Count);

	// try to overload the branch predictor before the processing loop
	// this hopefully simulates a more realistic workload
	// as a side effect, this should wreck the instruction cache somewhat
	BPRED216;BPRED216;

	t = Time();
	uint64_t val = s.ProcessData(data);
	__asm__ __volatile__("" :: "m" (val));
	double tProc = Time() - t;

	t = Time();
	for (int i = 0; i < Count; ++i)
		s.Remove(indices[i]);
	double tRemove = Time() - t;
	ASSERT(s.Count() == 0);

	t = Time();
	for (int i = 0; i < Count; ++i)
		s.Add(indices[i]);
	double tAddAftRem = Time() - t;
	ASSERT(s.Count() == Count);

	t = Time();
	for (int i = Count - 1; i >= 0; --i)
		s.Remove(indices[i]);
	double tRevRemove = Time() - t;
	ASSERT(s.Count() == 0);

	printf("%12s:|%9.4f|%9.4f|%9.4f|%9.4f|%9.4f\n",
		Set::NAME
		, tAdd * 1000
		, tProc * 1000
		, tRemove * 1000
		, tAddAftRem * 1000
		, tRevRemove * 1000
	);
};



#define COUNT 100000
#define COUNT2 1000
int main()
{
	puts("### Methods for storing arbitrary indexed subsets of array data ###");
	puts("### (times are in milliseconds)                                 ###");
	puts("");

	vector<uint32_t> indices;
	vector<uint64_t> values;
	indices.resize(COUNT);
	values.resize(COUNT);
	for (int i = 0; i < COUNT; ++i)
	{
		indices[i] = i;
		values[i] = (i * 101U) & 0xffffU;
	}

	std::random_device rd;
	std::mt19937 g(rd());
	std::shuffle(indices.begin(), indices.end(), g);

	printf("item count: %d\n", COUNT);
	printf("%12s |%9s|%9s|%9s|%9s|%9s\n", "Method", "Add", "Process", "Remove", "AddAftRem", "RevRemove");
	RunTests<Set_CPPSet, COUNT>(indices.data(), values.data());
	RunTests<Set_CPPUoSet, COUNT>(indices.data(), values.data());
	RunTests<Set_CPPUoSet2, COUNT>(indices.data(), values.data());
	RunTests<Set_SortedArray, COUNT>(indices.data(), values.data());
	RunTests<Set_ArraySet, COUNT>(indices.data(), values.data());
	RunTests<Set_SortArrSet, COUNT>(indices.data(), values.data());
	RunTests<Set_SparseArrSet, COUNT>(indices.data(), values.data());
	puts("");

	printf("item count: %d\n", COUNT2);
	printf("%12s |%9s|%9s|%9s|%9s|%9s\n", "Method", "Add", "Process", "Remove", "AddAftRem", "RevRemove");
	RunTests<Set_CPPSet, COUNT2>(indices.data(), values.data());
	RunTests<Set_CPPUoSet, COUNT2>(indices.data(), values.data());
	RunTests<Set_CPPUoSet2, COUNT2>(indices.data(), values.data());
	RunTests<Set_SortedArray, COUNT2>(indices.data(), values.data());
	RunTests<Set_ArraySet, COUNT2>(indices.data(), values.data());
	RunTests<Set_SortArrSet, COUNT2>(indices.data(), values.data());
	RunTests<Set_SparseArrSet, COUNT2>(indices.data(), values.data());
	puts("");
}


