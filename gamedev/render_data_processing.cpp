
#include <vector>
#include <cstdio>
#include <iostream>
#include <cmath>
#include <cassert>
#include <algorithm>
#include <inttypes.h>

using namespace std;



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



struct AABB
{
	float vmin[3];
	float vmax[3];
};

bool AABBIntersect(const AABB& a, const AABB& b)
{
	return (a.vmin[0] < b.vmax[0]
		&& a.vmin[1] < b.vmax[1]
		&& a.vmin[2] < b.vmax[2]
		&& b.vmin[0] < a.vmax[0]
		&& b.vmin[1] < a.vmax[1]
		&& b.vmin[2] < a.vmax[2]);
}

struct DrawItemInfo
{
	uint16_t part;
	uint8_t type;
};

struct DrawItemID
{
	bool operator == (const DrawItemID& o) const
	{
		return mii == o.mii && part == o.part;
	}

	uint32_t mii;
	uint16_t part;
};

void compareDrawItems(const vector<DrawItemID>& ref, const vector<DrawItemID>& cur)
{
	if (ref.size() != cur.size())
	{
		printf("Array size mismatch: ref=%u cur=%u\n",
			unsigned(ref.size()), unsigned(cur.size()));
	}
	for (size_t i = 0; i < ref.size(); ++i)
	{
		if (!(ref[i] == cur[i]))
		{
			printf("Array data mismatch at %u (ref=%u:%u cur=%u:%u)\n",
				unsigned(i), ref[i].mii, ref[i].part, cur[i].mii, cur[i].part);
		}
	}
}



struct Method
{
	static const int WIDTH = 23;
	static const int COUNT = WIDTH * WIDTH * WIDTH;

	virtual void AddMeshInstance(const AABB& bbox, DrawItemInfo* drawitems, int numdrawitems) = 0;
	virtual void Process(const AABB& camBox, uint8_t typemask) = 0;

	void Fill()
	{
		int nummi = 0;
		DrawItemInfo dii1[] = { { 0, 0 } };
		DrawItemInfo dii2[] = { { 0, 1 }, { 1, 1 } };
		DrawItemInfo dii3[] = { { 0, 0 }, { 1, 1 }, { 2, 1 } };
		DrawItemInfo dii4[] = { { 0, 0 }, { 1, 0 }, { 2, 0 }, { 3, 1 } };
		for (int z = 0; z < WIDTH; ++z)
		{
			for (int y = 0; y < WIDTH; ++y)
			{
				for (int x = 0; x < WIDTH; ++x)
				{
					int id = x + y * WIDTH + z * WIDTH * WIDTH;
					AABB bbox =
					{
						{ (float) x, (float) y, (float) z },
						{ x + 1.0f, y + 1.0f, z + 1.0f }
					};
					switch (id % 4)
					{
					case 0: AddMeshInstance(bbox, dii1, 1); break;
					case 1: AddMeshInstance(bbox, dii2, 2); break;
					case 2: AddMeshInstance(bbox, dii3, 3); break;
					case 3: AddMeshInstance(bbox, dii4, 4); break;
					}
					nummi++;
				}
			}
		}
		//printf("                                info: %d mesh instances\n", nummi);
	}

	vector<DrawItemID> drawn;
};



// ██████╗  █████╗ ███████╗███████╗██╗     ██╗███╗   ██╗███████╗
// ██╔══██╗██╔══██╗██╔════╝██╔════╝██║     ██║████╗  ██║██╔════╝
// ██████╔╝███████║███████╗█████╗  ██║     ██║██╔██╗ ██║█████╗  
// ██╔══██╗██╔══██║╚════██║██╔══╝  ██║     ██║██║╚██╗██║██╔══╝  
// ██████╔╝██║  ██║███████║███████╗███████╗██║██║ ╚████║███████╗
// ╚═════╝ ╚═╝  ╚═╝╚══════╝╚══════╝╚══════╝╚═╝╚═╝  ╚═══╝╚══════╝
//                                                              
struct BaselineMethod : Method
{
	struct MeshInst;
	struct DrawItem
	{
		MeshInst* MI;
		uint16_t part;
		uint8_t type;
		char pad[80];
	};
	struct MeshInst
	{
		vector<DrawItem> drawitems;
		AABB bbox;
		uint32_t id;
		char pad[500];
	};
	struct Sortable
	{
		uint64_t key;
		DrawItem* DI;
	};

	~BaselineMethod()
	{
		for (auto* p : randomallocs)
			delete[] p;
	}
	void AddMeshInstance(const AABB& bbox, DrawItemInfo* drawitems, int numdrawitems)
	{
		MeshInst* inst = new MeshInst;
		inst->bbox = bbox;
		inst->id = meshinsts.size();
		for (int i = 0; i < numdrawitems; ++i)
		{
			inst->drawitems.push_back({ inst, drawitems[i].part, drawitems[i].type });
		}
		if (meshinsts.size() % 2 == 0)
			randomallocs.push_back(new char[32]);
		if (meshinsts.size() % 2000 == 0)
			randomallocs.push_back(new char[123567]);
		meshinsts.push_back(inst);
	}
	void Process(const AABB& camBox, uint8_t typemask)
	{
		culled.reserve(meshinsts.size());
		culled.clear();

		double t0 = Time();

		uint32_t mii = 0;
		for (auto inst : meshinsts)
		{
			if (AABBIntersect(inst->bbox, camBox))
			{
				for (auto& di : inst->drawitems)
				{
					if ((1 << di.type) & typemask)
						culled.push_back({ (uint64_t(di.type) << 63) | (uint64_t(di.part) << 32) | di.MI->id, &di });
				}
			}
			mii++;
		}

		double t1 = Time();
		tCull += t1 - t0;

		TRadixSort<uint64_t>(culled.data(), culled.data() + culled.size(), [](const Sortable& s) { return s.key; });

		double t2 = Time();
		tSort += t2 - t1;

		drawn.reserve(culled.size());
		drawn.clear();

		double t3 = Time();

		for (auto& s : culled)
			drawn.push_back({ s.DI->MI->id, s.DI->part });

		double t4 = Time();
		tMove += t4 - t3;
		nRuns++;

		//printf("processed %u mesh insts, culled to %u draw items\n", mii, (int) drawn.size());
	}

	vector<MeshInst*> meshinsts;
	vector<Sortable> culled;
	vector<char*> randomallocs;

	static double tCull;
	static double tSort;
	static double tMove;
	static int nRuns;
};
double BaselineMethod::tCull;
double BaselineMethod::tSort;
double BaselineMethod::tMove;
int BaselineMethod::nRuns;



// ██╗   ██╗██████╗ 
// ██║   ██║╚════██╗
// ██║   ██║ █████╔╝
// ╚██╗ ██╔╝██╔═══╝ 
//  ╚████╔╝ ███████╗
//   ╚═══╝  ╚══════╝
//                  
// baseline -> separate AABBs from DrawItems
struct V2Method : Method
{
	struct DrawItem
	{
		uint16_t part;
		uint8_t type;
	};
	struct Sortable
	{
		uint64_t key;
		uint32_t mii;
		uint16_t part;
	};

	void AddMeshInstance(const AABB& bbox, DrawItemInfo* drawitems, int numdrawitems)
	{
		miBB.push_back(bbox);
		miDI.emplace_back();
		for (int i = 0; i < numdrawitems; ++i)
			miDI.back().push_back({ drawitems[i].part, drawitems[i].type });
	}
	void Process(const AABB& camBox, uint8_t typemask)
	{
		culled.reserve(miBB.size());
		culled.clear();

		double t0 = Time();

		uint32_t mii = 0;
		for ( ; mii < miBB.size(); ++mii)
		{
			if (AABBIntersect(miBB[mii], camBox))
			{
				for (auto& di : miDI[mii])
				{
					if ((1 << di.type) & typemask)
						culled.push_back({ (uint64_t(di.type) << 63) | (uint64_t(di.part) << 32) | mii, mii, di.part });
				}
			}
		}

		double t1 = Time();
		tCull += t1 - t0;

		TRadixSort<uint64_t>(culled.data(), culled.data() + culled.size(), [](const Sortable& s) { return s.key; });

		double t2 = Time();
		tSort += t2 - t1;

		drawn.reserve(culled.size());
		drawn.clear();

		double t3 = Time();

		for (auto& s : culled)
			drawn.push_back({ s.mii, s.part });

		double t4 = Time();
		tMove += t4 - t3;
		nRuns++;

		//printf("processed %u mesh insts, culled to %u draw items\n", mii, (int) drawn.size());
	}

	vector<AABB> miBB;
	vector<vector<DrawItem>> miDI;
	vector<Sortable> culled;

	static double tCull;
	static double tSort;
	static double tMove;
	static int nRuns;
};
double V2Method::tCull;
double V2Method::tSort;
double V2Method::tMove;
int V2Method::nRuns;



// ██╗   ██╗██████╗ 
// ██║   ██║╚════██╗
// ██║   ██║ █████╔╝
// ╚██╗ ██╔╝ ╚═══██╗
//  ╚████╔╝ ██████╔╝
//   ╚═══╝  ╚═════╝ 
//                  
// v2 -> join data
struct V3Method : Method
{
	struct Sortable
	{
		uint64_t key;
		uint32_t dii;
	};

	void AddMeshInstance(const AABB& bbox, DrawItemInfo* drawitems, int numdrawitems)
	{
		uint32_t mii = curMII++;
		for (int i = 0; i < numdrawitems; ++i)
		{
			diBB.push_back(bbox);
			diMII.push_back(mii);
			diPart.push_back(drawitems[i].part);
			diType.push_back(drawitems[i].type);
		}
	}
	void Process(const AABB& camBox, uint8_t typemask)
	{
		culled.reserve(diBB.size());
		culled.clear();

		double t0 = Time();

		uint32_t dii = 0;
		for ( ; dii < diBB.size(); ++dii)
		{
			if (((1 << diType[dii]) & typemask) && AABBIntersect(diBB[dii], camBox))
			{
				culled.push_back({ (uint64_t(diType[dii]) << 63) | (uint64_t(diPart[dii]) << 32) | diMII[dii], dii });
			}
		}

		double t1 = Time();
		tCull += t1 - t0;

		TRadixSort<uint64_t>(culled.data(), culled.data() + culled.size(), [](const Sortable& s) { return s.key; });

		double t2 = Time();
		tSort += t2 - t1;

		drawn.reserve(culled.size());
		drawn.clear();

		double t3 = Time();

		for (auto& s : culled)
			drawn.push_back({ diMII[s.dii], diPart[s.dii] });

		double t4 = Time();
		tMove += t4 - t3;
		nRuns++;

		//printf("processed %u draw items, culled to %u draw items\n", di, (int) drawn.size());
	}

	vector<AABB> diBB;
	vector<uint32_t> diMII;
	vector<uint16_t> diPart;
	vector<uint8_t> diType;
	uint32_t curMII = 0;
	vector<Sortable> culled;

	static double tCull;
	static double tSort;
	static double tMove;
	static int nRuns;
};
double V3Method::tCull;
double V3Method::tSort;
double V3Method::tMove;
int V3Method::nRuns;



// ██╗   ██╗██╗  ██╗
// ██║   ██║██║  ██║
// ██║   ██║███████║
// ╚██╗ ██╔╝╚════██║
//  ╚████╔╝      ██║
//   ╚═══╝       ╚═╝
//                  
// v2 -> type bucketing + multiple stable radix sorts with different params + sortable size reduction
struct V4Method : Method
{
	struct DrawItem
	{
		uint16_t part;
		uint8_t type;
	};
	struct Sortable
	{
		uint32_t mii;
		uint16_t part;
	};

	void AddMeshInstance(const AABB& bbox, DrawItemInfo* drawitems, int numdrawitems)
	{
		miBB.push_back(bbox);
		miDI.emplace_back();
		for (int i = 0; i < numdrawitems; ++i)
			miDI.back().push_back({ drawitems[i].part, drawitems[i].type });
	}
	void Process(const AABB& camBox, uint8_t typemask)
	{
		culled[0].reserve(miBB.size());
		culled[0].clear();
		culled[1].reserve(miBB.size());
		culled[1].clear();

		double t0 = Time();

		uint32_t mii = 0;
		for ( ; mii < miBB.size(); ++mii)
		{
			if (AABBIntersect(miBB[mii], camBox))
			{
				for (auto& di : miDI[mii])
				{
					if ((1 << di.type) & typemask)
						culled[di.type].push_back({ mii, di.part });
				}
			}
		}

		double t1 = Time();
		tCull += t1 - t0;

		TRadixSort<uint32_t>(culled[0].data(), culled[0].data() + culled[0].size(), [](const Sortable& s) { return s.mii; });
		TRadixSort<uint16_t>(culled[0].data(), culled[0].data() + culled[0].size(), [](const Sortable& s) { return s.part; });
		TRadixSort<uint32_t>(culled[1].data(), culled[1].data() + culled[1].size(), [](const Sortable& s) { return s.mii; });
		TRadixSort<uint16_t>(culled[1].data(), culled[1].data() + culled[1].size(), [](const Sortable& s) { return s.part; });

		double t2 = Time();
		tSort += t2 - t1;

		drawn.reserve(culled[0].size() + culled[1].size());
		drawn.clear();

		double t3 = Time();

		for (auto& s : culled[0])
			drawn.push_back({ s.mii, s.part });
		for (auto& s : culled[1])
			drawn.push_back({ s.mii, s.part });

		double t4 = Time();
		tMove += t4 - t3;
		nRuns++;

		//printf("processed %u mesh insts, culled to %u draw items\n", mii, (int) drawn.size());
	}

	vector<AABB> miBB;
	vector<vector<DrawItem>> miDI;
	vector<Sortable> culled[2];

	static double tCull;
	static double tSort;
	static double tMove;
	static int nRuns;
};
double V4Method::tCull;
double V4Method::tSort;
double V4Method::tMove;
int V4Method::nRuns;



// ██╗   ██╗███████╗
// ██║   ██║██╔════╝
// ██║   ██║███████╗
// ╚██╗ ██╔╝╚════██║
//  ╚████╔╝ ███████║
//   ╚═══╝  ╚══════╝
//                  
// v2 -> inline DrawItems
// !! LIMITATION: max. 4 DrawItems per mesh instance
struct V5Method : Method
{
	struct MIDrawItems
	{
		uint16_t parts[4];
		uint8_t types[4];
		uint8_t count;
	};
	struct Sortable
	{
		uint64_t key;
		uint32_t mii;
		uint16_t part;
	};

	void AddMeshInstance(const AABB& bbox, DrawItemInfo* drawitems, int numdrawitems)
	{
		miBB.push_back(bbox);
		miDI.emplace_back();
		MIDrawItems& DI = miDI.back();
		DI.count = numdrawitems;
		for (int i = 0; i < numdrawitems; ++i)
		{
			DI.parts[i] = drawitems[i].part;
			DI.types[i] = drawitems[i].type;
		}
	}
	void Process(const AABB& camBox, uint8_t typemask)
	{
		culled.reserve(miBB.size());
		culled.clear();

		double t0 = Time();

		uint32_t mii = 0;
		for ( ; mii < miBB.size(); ++mii)
		{
			if (AABBIntersect(miBB[mii], camBox))
			{
				auto& DI = miDI[mii];
				for (uint8_t i = 0; i < DI.count; ++i)
				{
					if ((1 << DI.types[i]) & typemask)
						culled.push_back({ (uint64_t(DI.types[i]) << 63) | (uint64_t(DI.parts[i]) << 32) | mii, mii, DI.parts[i] });
				}
			}
		}

		double t1 = Time();
		tCull += t1 - t0;

		TRadixSort<uint64_t>(culled.data(), culled.data() + culled.size(), [](const Sortable& s) { return s.key; });

		double t2 = Time();
		tSort += t2 - t1;

		drawn.reserve(culled.size());
		drawn.clear();

		double t3 = Time();

		for (auto& s : culled)
			drawn.push_back({ s.mii, s.part });

		double t4 = Time();
		tMove += t4 - t3;
		nRuns++;

		//printf("processed %u mesh insts, culled to %u draw items\n", mii, (int) drawn.size());
	}

	vector<AABB> miBB;
	vector<MIDrawItems> miDI;
	vector<Sortable> culled;

	static double tCull;
	static double tSort;
	static double tMove;
	static int nRuns;
};
double V5Method::tCull;
double V5Method::tSort;
double V5Method::tMove;
int V5Method::nRuns;



template<class MethodT, int NumIters> void RunTestM(const char* name, MethodT& m)
{
	m.Fill();
	auto t0 = Time();
	for (int i = 0; i < NumIters; ++i)
	{
		m.Process({ { 3.5f, 4.1f, 2.2f }, { 19.8f, 18.3f, 19.9f } }, 0x1);
		m.Process({ { 3.5f, 4.1f, 2.2f }, { 19.8f, 18.3f, 19.9f } }, 0x1 | 0x2);
	}
	auto t1 = Time();
	printf("%8s: %.4f ms (cull: %.4f, sort:%.4f, move:%.4f)\n",
		name,
		(t1 - t0) * 1000 / (NumIters * 2),
		1000 * MethodT::tCull / MethodT::nRuns,
		1000 * MethodT::tSort / MethodT::nRuns,
		1000 * MethodT::tMove / MethodT::nRuns);
}
template<class MethodT, int NumIters> void RunTest(const char* name, const vector<DrawItemID>& refDrawn)
{
	MethodT m;
	RunTestM<MethodT, NumIters>(name, m);
	compareDrawItems(refDrawn, m.drawn);
}



int main()
{
	puts("### Methods for processing of rendering data ###\n"
		"###   (mesh instances/parts/draw commands)   ###");
	printf("> instances=%d\n", Method::COUNT);

	const int NUMITERS = 100;
	vector<DrawItemID> refDrawn;
	{
		BaselineMethod m;
		RunTestM<BaselineMethod, NUMITERS>("baseline", m);
		refDrawn = m.drawn;
	}

	RunTest<V2Method, NUMITERS>("v2", refDrawn);
	RunTest<V3Method, NUMITERS>("v3", refDrawn);
	RunTest<V4Method, NUMITERS>("v4", refDrawn);
	RunTest<V5Method, NUMITERS>("v5", refDrawn);
}
