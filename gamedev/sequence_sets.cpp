
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



typedef int BOOL;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef void* HANDLE;
#define WINAPI __stdcall
struct LARGE_INTEGER
{
	long long QuadPart;
};
#define STD_OUTPUT_HANDLE ((DWORD)-11)
extern "C"
{
BOOL WINAPI QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency);
BOOL WINAPI QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount);
HANDLE WINAPI GetStdHandle(DWORD nStdHandle);
BOOL WINAPI SetConsoleTextAttribute(HANDLE hConsoleOutput, WORD wAttributes);
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



uint64_t Proc(uint64_t v)
{
	return (v >> 1) * v + v * v * (v >> 2) + v;
}



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
			out += Proc(data[val]);
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
			out += Proc(data[val]);
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
			out += Proc(data[val]);
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
			out += Proc(data[val]);
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
			out += Proc(data[val]);
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
			out += Proc(data[val]);
		return out;
	}

	vector<uint32_t> sortedIndices;
};



struct Set_SparseArrSet
{
	static constexpr const char* NAME = "SparseArrSet";

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
		for (size_t i = 0, which = 0, num = used.size(); i < num && which < count; ++i)
		{
			if (used[i])
			{
				out += Proc(data[i]);
				which++;
			}
		}
		return out;
	}

	vector<uint8_t> used;
	uint32_t count = 0;
};



template<int version>
struct Set_SpArrBitSet
{
	static constexpr const char* NAME = version == 2 ? "SpArrBitSet2" : "SpArrBitSet1";

	using BitUnit = uint64_t;
	static constexpr int BITCOUNT = sizeof(BitUnit) * 8;
	static constexpr uint32_t BITMASK = BITCOUNT - 1;
	static constexpr int BITSHIFT = 6; // log2(64)

	FINLINE uint32_t BitDivUp(uint32_t v)
	{
		return (v + BITMASK) >> BITSHIFT;
	}
	FINLINE bool BitGet(uint32_t v)
	{
		return (used[v >> BITSHIFT] & (1ULL << (v & BITMASK))) != 0;
	}
	FINLINE void BitSet(uint32_t v)
	{
		used[v >> BITSHIFT] |= 1ULL << (v & BITMASK);
	}
	FINLINE void BitClear(uint32_t v)
	{
		used[v >> BITSHIFT] &= ~(1ULL << (v & BITMASK));
	}

	void Prewarm(size_t s)
	{
		used.reserve(BitDivUp(s));
	}
	void Add(uint32_t v)
	{
		if (v >= used.size() * BITCOUNT)
			used.resize(BitDivUp(v + 1 + used.size()), 0); // excessive resizing to avoid frequent resizing
		else if (BitGet(v))
			return;
		BitSet(v);
		count++;
	}
	void Remove(uint32_t v)
	{
		if (v >= used.size() * BITCOUNT || !BitGet(v))
			return;
		BitClear(v);
		count--;
	}
	size_t Count()
	{
		return count;
	}
	uint64_t ProcessData(uint64_t* data)
	{
		uint64_t out = 0;
		if (version == 2)
		{
			for (size_t chid = 0, which = 0; chid < used.size() && which < count; ++chid)
			{
				if (BitUnit bits = used[chid])
				{
					size_t base = chid << BITSHIFT;
					uint64_t comp = 1;
					for (size_t i = 0; i < 64; ++i, comp <<= 1)
					{
						if (bits & comp)
						{
							out += Proc(data[base + i]);
							which++;
						}
					}
				}
			}
		}
		else if (version == 1)
		{
			for (size_t i = 0, which = 0, num = used.size() * BITCOUNT; i < num && which < count; ++i)
			{
				if (BitGet(i))
				{
					out += Proc(data[i]);
					which++;
				}
			}
		}
		return out;
	}

	vector<BitUnit> used;
	uint32_t count = 0;
};



struct Results
{
	struct Test
	{
		const char* name;
		double numbers[5];
	};

	Results()
	{
		hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	}
	~Results()
	{
		Print();
	}
	void AddResult(const char* name, double a, double b, double c, double d, double e)
	{
		data.push_back({ name, { a, b, c, d, e }});
	}
	void Print()
	{
		double mins[5] = { 9999, 9999, 9999, 9999, 9999 };
		double maxs[5] = { 0, 0, 0, 0, 0 };
		double meds[5];

		for (int i = 0; i < 5; ++i)
		{
			double vals[5];
			int j = 0;
			for (auto& t : data)
			{
				vals[j++] = t.numbers[i];
				if (mins[i] > t.numbers[i])
					mins[i] = t.numbers[i];
				if (maxs[i] < t.numbers[i])
					maxs[i] = t.numbers[i];
			}
			sort(vals, vals + 5);
			meds[i] = vals[2];
		}

		for (auto& t : data)
		{
			printf("%12s:", t.name);
			for (int i = 0; i < 5; ++i)
			{
				double up = maxs[i] - meds[i];
				double dn = meds[i] - mins[i];
				double mnd = up < dn ? up : dn;
				double minv = meds[i] - mnd;
				double maxv = meds[i] + mnd;
				double q = minv == maxv ? 0.5 : (t.numbers[i] - minv) / (maxv - minv);
				int col;
				if (q < 0.1)
					col = 10;
				else if (q < 0.3)
					col = 2;
				else if (q > 0.9)
					col = 12;
				else if (q > 0.7)
					col = 4;
				else
					col = 6;

				printf("|");
				fflush(stdout);
				SetConsoleTextAttribute(hConsole, col);
				printf("%9.4f", t.numbers[i]);
				fflush(stdout);
				SetConsoleTextAttribute(hConsole, 7);
			}
			puts("");
		}
	}

	vector<Test> data;
	HANDLE hConsole;
};

#define BPRED1 {if (rand() % 2) rand();}
#define BPRED6 BPRED1 BPRED1 BPRED1 BPRED1 BPRED1 BPRED1
#define BPRED36 BPRED6 BPRED6 BPRED6 BPRED6 BPRED6 BPRED6
#define BPRED216 BPRED36 BPRED36 BPRED36 BPRED36 BPRED36 BPRED36

template<class Set, int Count> void RunTests(uint32_t* indices, uint64_t* values, Results& results, uint64_t ref)
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
	uint64_t val = s.ProcessData(values);
	__asm__ __volatile__("" :: "m" (val));
	double tProc = Time() - t;
	ASSERT(val == ref);

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

	results.AddResult(
		Set::NAME
		, tAdd * 1000
		, tProc * 1000
		, tRemove * 1000
		, tAddAftRem * 1000
		, tRevRemove * 1000
	);
};

template<int Count> void RunAllTests(uint32_t* indices, uint64_t* values, Results& results)
{
	uint64_t ref = 0;
	for (int i = 0; i < Count; ++i)
		ref += Proc(values[indices[i]]);

	RunTests<Set_CPPSet, Count>(indices, values, results, ref);
	RunTests<Set_CPPUoSet, Count>(indices, values, results, ref);
	RunTests<Set_CPPUoSet2, Count>(indices, values, results, ref);
	RunTests<Set_SortedArray, Count>(indices, values, results, ref);
	RunTests<Set_ArraySet, Count>(indices, values, results, ref);
	RunTests<Set_SortArrSet, Count>(indices, values, results, ref);
	RunTests<Set_SparseArrSet, Count>(indices, values, results, ref);
	RunTests<Set_SpArrBitSet<1>, Count>(indices, values, results, ref);
	RunTests<Set_SpArrBitSet<2>, Count>(indices, values, results, ref);
}



#define N1K 1000
#define N10K 10000
#define N100K 100000
#define N1000K 1000000
void iteminfo(int icount, int maxid)
{
	printf("item count: %d  max.item ID:%d  (%g%%)\n", icount, maxid, icount * 100.0 / maxid);
	printf("%12s |%9s|%9s|%9s|%9s|%9s\n", "Method", "Add", "Process", "Remove", "AddAftRem", "RevRemove");
}
int main()
{
	puts("### Methods for storing arbitrary indexed subsets of array data ###");
	puts("### (times are in milliseconds)                                 ###");
	puts("");

	vector<uint32_t> indices;
	vector<uint64_t> values;
	indices.resize(N100K);
	values.resize(N100K);
	for (int i = 0; i < N100K; ++i)
	{
		indices[i] = i;
		values[i] = (i * 101U) & 0xffffU;
	}

	{
		std::random_device rd;
		std::mt19937 g(rd());
		std::shuffle(indices.begin(), indices.end(), g);
	}

	iteminfo(N100K, N100K);
	{
		Results r;
		RunAllTests<N100K>(indices.data(), values.data(), r);
	}
	puts("");

	iteminfo(N10K, N100K);
	{
		Results r;
		RunAllTests<N10K>(indices.data(), values.data(), r);
	}
	puts("");

	iteminfo(N1K, N100K);
	{
		Results r;
		RunAllTests<N1K>(indices.data(), values.data(), r);
	}
	puts("");

	vector<uint32_t> indices0;
	vector<uint64_t> values0;
	indices0.resize(N100K);
	values0.resize(N100K);
	for (int i = 0; i < N100K; ++i)
	{
		indices0[i] = i;
		values0[i] = (i * 101U) & 0xffffU;
	}

	{
		std::random_device rd;
		std::mt19937 g(rd());
		std::shuffle(indices0.begin(), indices0.end(), g);
	}

	iteminfo(N100K, N1000K);
	{
		Results r;
		RunAllTests<N100K>(indices0.data(), values0.data(), r);
	}
	puts("");

	iteminfo(N10K, N1000K);
	{
		Results r;
		RunAllTests<N10K>(indices0.data(), values0.data(), r);
	}
	puts("");

	iteminfo(N1K, N1000K);
	{
		Results r;
		RunAllTests<N1K>(indices0.data(), values0.data(), r);
	}
	puts("");

}


