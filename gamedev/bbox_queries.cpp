
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



struct AABB
{
	float vmin[3];
	float vmax[3];
};

bool AABBIntersect(const AABB& a, const AABB& b)
{
	return a.vmin[0] < b.vmax[0]
		&& a.vmin[1] < b.vmax[1]
		&& a.vmin[2] < b.vmax[2]
		&& b.vmin[0] < a.vmax[0]
		&& b.vmin[1] < a.vmax[1]
		&& b.vmin[2] < a.vmax[2];
}

bool AABBEnclosing(const AABB& b, const AABB& s)
{
	return b.vmin[0] <= s.vmin[0]
		&& b.vmin[1] <= s.vmin[1]
		&& b.vmin[2] <= s.vmin[2]
		&& b.vmax[0] >= s.vmax[0]
		&& b.vmax[1] >= s.vmax[1]
		&& b.vmax[2] >= s.vmax[2];
}

AABB AABBCombine(const AABB& a, const AABB& b)
{
	return
	{
		{ min(a.vmin[0], b.vmin[0]), min(a.vmin[1], b.vmin[1]), min(a.vmin[2], b.vmin[2]) },
		{ max(a.vmax[0], b.vmax[0]), max(a.vmax[1], b.vmax[1]), max(a.vmax[2], b.vmax[2]) },
	};
}

FINLINE float AABBSize(const AABB& b)
{
	return (b.vmax[0] - b.vmin[0]) * (b.vmax[1] - b.vmin[1]) * (b.vmax[2] - b.vmin[2]);
}



struct Storage_AABBArray
{
	static constexpr const char* NAME = "AABBArray";

	using Handle = uint32_t*;

	void Prewarm(size_t num)
	{
		handles.reserve(num);
		bboxes.reserve(num);
	}
	size_t Count() { return handles.size(); }
	void Validate() {}
	Handle Add(const AABB& bb)
	{
		uint32_t* h = new uint32_t(handles.size());
		handles.push_back(h);
		bboxes.push_back(bb);
		return h;
	}
	void Remove(Handle p)
	{
		uint32_t id = *p;
		if (id + 1 < handles.size())
		{
			handles[id] = handles.back();
			bboxes[id] = bboxes.back();
			*handles[id] = id;
		}
		delete p;
		handles.pop_back();
		bboxes.pop_back();
	}
	const AABB& GetAABB(Handle p)
	{
		return bboxes[*p];
	}
	void Query(const AABB& bb, vector<Handle>& out)
	{
		for (size_t i = 0, num = bboxes.size(); i < num; ++i)
		{
			if (AABBIntersect(bb, bboxes[i]))
				out.push_back(handles[i]);
		}
	}

	vector<Handle> handles;
	vector<AABB> bboxes;
};



// http://www.randygaul.net/2013/08/06/dynamic-aabb-tree/
struct Storage_DynAABBTree
{
	static constexpr const char* NAME = "DynAABBTree";

	struct Node
	{
		static const int32_t Null = -1;

		FINLINE bool IsLeaf() const { return right == Null; }

		AABB aabb;
		union
		{
			int32_t parent;
			int32_t nextFree;
		};
		union
		{
			struct
			{
				int32_t left;
				int32_t right;
			};
			void* userData;
		};
		// leaf = 0, free = -1
		int32_t height;
	};
	using Handle = int32_t;

	void Prewarm(size_t num)
	{
		nodes.reserve(num);
	}
	size_t Count() { return count; }
	FINLINE int32_t ID(const Node& N) { return &N - nodes.data(); }
	void _SyncHierarchy(int32_t i)
	{
		while (i != Node::Null)
		{
			auto& L = nodes[i];

#if 1 // ROTATIONS
#define SIZE_BASED
			if (nodes[L.left].height) // potential right rotation
			{
				float sizeCur = AABBSize(nodes[L.left].aabb) + AABBSize(nodes[L.right].aabb);
				float sizeRot = AABBSize(nodes[nodes[L.left].left].aabb) + AABBSize(AABBCombine(nodes[nodes[L.left].right].aabb, nodes[L.right].aabb));
			//	if (sizeCur == sizeRot) printf("[%d] right? htL=%d htR=%d\n", i, nodes[nodes[L.left].left].height, nodes[L.right].height); else printf("[%d] right? sizeCur=%g sizeRot=%g\n", i, sizeCur, sizeRot);
				if (
#ifdef SIZE_BASED
					sizeCur > sizeRot || (sizeCur == sizeRot &&
#else
					(
#endif
					nodes[nodes[L.left].left].height > nodes[L.right].height + 1))
				{
			//		puts("right!");
					auto& RT = L;
					auto& PV = nodes[L.left];
					RT.left = PV.right;
					nodes[RT.left].parent = ID(RT);
					PV.right = ID(RT);
					PV.parent = RT.parent;
					RT.parent = ID(PV);
					if (PV.parent == Node::Null)
						root = ID(PV);
					else
					{
						auto& PP = nodes[PV.parent];
						(PP.left == ID(RT) ? PP.left : PP.right) = ID(PV);
					}
					RT.aabb = AABBCombine(nodes[RT.left].aabb, nodes[RT.right].aabb);
					RT.height = std::max(nodes[RT.left].height, nodes[RT.right].height) + 1;
					i = ID(RT);
				//	Validate(false);
					continue;
				}
			}
			if (nodes[L.right].height) // potential left rotation
			{
				float sizeCur = AABBSize(nodes[L.right].aabb) + AABBSize(nodes[L.left].aabb);
				float sizeRot = AABBSize(nodes[nodes[L.right].right].aabb) + AABBSize(AABBCombine(nodes[nodes[L.right].left].aabb, nodes[L.left].aabb));
			//	if (sizeCur == sizeRot) printf("[%d] left?  htL=%d htR=%d\n", i, nodes[L.left].height, nodes[nodes[L.right].right].height); else printf("[%d] left?  sizeCur=%g sizeRot=%g\n", i, sizeCur, sizeRot);
				if (
#ifdef SIZE_BASED
					sizeCur > sizeRot || (sizeCur == sizeRot &&
#else
					(
#endif
					nodes[L.left].height + 1 < nodes[nodes[L.right].right].height))
				{
				//	puts("left!");
					auto& RT = L;
					auto& PV = nodes[L.right];
					RT.right = PV.left;
					nodes[RT.right].parent = ID(RT);
					PV.left = ID(RT);
					PV.parent = RT.parent;
					RT.parent = ID(PV);
					if (PV.parent == Node::Null)
						root = ID(PV);
					else
					{
						auto& PP = nodes[PV.parent];
						(PP.left == ID(RT) ? PP.left : PP.right) = ID(PV);
					}
					RT.aabb = AABBCombine(nodes[RT.left].aabb, nodes[RT.right].aabb);
					RT.height = std::max(nodes[RT.left].height, nodes[RT.right].height) + 1;
					i = ID(RT);
				//	Validate(false);
					continue;
				}
			}
#endif

			L.aabb = AABBCombine(nodes[L.left].aabb, nodes[L.right].aabb);
			L.height = std::max(nodes[L.left].height, nodes[L.right].height) + 1;

			i = L.parent;
		}
	}
	Handle Add(const AABB& bb)
	{
		int32_t id;
		{
			auto& N = _AllocNode();
			N.aabb = bb;
			N.height = 0;
			N.left = Node::Null;
			N.right = Node::Null;
			id = &N - nodes.data();
		}

		if (count > 1) // general case
		{
			int32_t firstNonEnclosingNode = root;
			while (nodes[firstNonEnclosingNode].height != 0)
			{
				auto& L = nodes[firstNonEnclosingNode];
				bool encL = AABBEnclosing(nodes[L.left].aabb, bb);
				bool encR = AABBEnclosing(nodes[L.right].aabb, bb);
				if (encL && encR)
				{
					// TODO improve?
					firstNonEnclosingNode = L.left;
				}
				else if (encL)
					firstNonEnclosingNode = L.left;
				else if (encR)
					firstNonEnclosingNode = L.right;
				else
					break;
			}

			// allocate a new branch
			auto& NL = _AllocNode();
			int32_t idNL = &NL - nodes.data();

			// link branch to parent
			auto& FNE = nodes[firstNonEnclosingNode];
			NL.parent = FNE.parent;
			if (FNE.parent != Node::Null)
			{
				auto& P = nodes[FNE.parent];
				(P.left == firstNonEnclosingNode ? P.left : P.right) = idNL;
			}
			else
			{
				root = idNL;
			}

			// link children to branch
			NL.left = id;
			NL.right = firstNonEnclosingNode;
			nodes[id].parent = idNL;
			FNE.parent = idNL;

			numNodes += 2;
		//	count++;
		//	puts("post-add 2+");
		//	NL.height = 1;
		//	Validate(false);
			_SyncHierarchy(idNL);
		//	count--;
		}
		else if (count == 1) // first leaf
		{
			auto& L = _AllocNode();
			L.aabb = AABBCombine(bb, nodes[root].aabb);
			L.height = 1;
			L.left = id;
			L.right = root;
			L.parent = Node::Null;
			int32_t lid = &L - nodes.data();
			nodes[id].parent = lid;
			nodes[root].parent = lid;
			root = lid;
			numNodes += 2;
		}
		else // first node
		{
			nodes[id].parent = Node::Null;
			root = id;
			numNodes++;
		}
		count++;
	//	Validate();
	//	printf("inserted %d\n", id);
		return id;
	}
	Node& _AllocNode()
	{
		if (firstFree != Node::Null)
		{
			Node& N = nodes[firstFree];
			firstFree = N.nextFree;
			return N;
		}
		nodes.emplace_back();
		return nodes.back();
	}
	void Remove(Handle p)
	{
		if (count > 1) // parent is leaf
		{
			auto& N = nodes[p];
			auto& P = nodes[N.parent];
			uint32_t o = P.left != p ? P.left : P.right;
			auto& O = nodes[o];
			O.parent = P.parent;
			if (P.parent != Node::Null)
			{
				auto& PP = nodes[P.parent];
				(PP.left == N.parent ? PP.left : PP.right) = o;
				_SyncHierarchy(P.parent);
			}
			else
			{
				root = &O - nodes.data();
			}
			_ReleaseNode(N.parent);
		}
		else // last node (nodes[p].parent = Node::Null)
		{
			root = Node::Null;
		}
		count--;
		_ReleaseNode(p);
	}
	void _ReleaseNode(Handle p)
	{
		nodes[p].height = -1;
		nodes[p].nextFree = firstFree;
		firstFree = p;
		numNodes--;
	}
	void Validate(bool aabbs = true, bool visits = true)
	{
		uint32_t numVisited = 0, numLeaves = 0;
		if (root != Node::Null)
			_ValidateNode(root, aabbs, numVisited, numLeaves);

		if (visits)
		{
			if (numVisited != numNodes)
			{
				Dump();
				printf("expected %u nodes, visited %u\n", numNodes, numVisited);
			}
			ASSERT(numVisited == numNodes);

			if (numLeaves != count)
			{
				Dump();
				printf("expected %u nodes, visited %u\n", count, numLeaves);
			}
			ASSERT(numLeaves == count);
		}

		// test value (should be as close to log2(nodes) as possible):
		printf("[debug DynAABBTree] max. height: %d [log2(nodes)=%g]\n", root != Node::Null ? nodes[root].height : -1, log(numNodes) / log(2));
	}
	void _ValidateNode(int32_t node, bool aabbs, uint32_t& numVisited, uint32_t& numLeaves)
	{
		ASSERT(node != Node::Null);
		auto& N = nodes[node];
		numVisited++;

		if (N.height > 0)
		{
			if (nodes[N.left].parent != node)
			{
				Dump();
				printf("node %d left child %d parent is wrong: %d\n", node, N.left, nodes[N.left].parent);
			}
			ASSERT(nodes[N.left].parent == node);

			ASSERT(nodes[N.right].parent == node);
			_ValidateNode(N.left, aabbs, numVisited, numLeaves);
			_ValidateNode(N.right, aabbs, numVisited, numLeaves);

			if (aabbs)
			{
				if (!AABBEnclosing(N.aabb, nodes[N.left].aabb))
				{
					auto& C = nodes[N.left];
					Dump();
					printf("node %d AABB (%g;%g;%g - %g;%g;%g) not enclosing child %d AABB (%g;%g;%g - %g;%g;%g)\n",
						node,
						N.aabb.vmin[0], N.aabb.vmin[1], N.aabb.vmin[2], N.aabb.vmax[0], N.aabb.vmax[1], N.aabb.vmax[2],
						N.left,
						C.aabb.vmin[0], C.aabb.vmin[1], C.aabb.vmin[2], C.aabb.vmax[0], C.aabb.vmax[1], C.aabb.vmax[2]);
				}
				ASSERT(AABBEnclosing(N.aabb, nodes[N.left].aabb));

				ASSERT(AABBEnclosing(N.aabb, nodes[N.right].aabb));
			}
		}
		else numLeaves++;
	}
	void Dump()
	{
		puts("/// DUMP ///");
		if (root != Node::Null)
			_DumpNode(root, 0);
	}
	void _DumpNode(int32_t node, int depth)
	{
		for (int i = 0; i < depth; ++i)
			printf("  ");
		auto& N = nodes[node];
		printf("node %d left=%d right=%d height=%d aabb=[%g;%g;%g - %g;%g;%g]\n",
			node, N.left, N.right, N.height,
			N.aabb.vmin[0], N.aabb.vmin[1], N.aabb.vmin[2],
			N.aabb.vmax[0], N.aabb.vmax[1], N.aabb.vmax[2]);
	}
	const AABB& GetAABB(Handle p)
	{
		return nodes[p].aabb;
	}
	void Query(const AABB& bb, vector<Handle>& out)
	{
		_QueryNode(bb, out, root);
	}
	void _QueryNode(const AABB& bb, vector<Handle>& out, int32_t node)
	{
		if (node == Node::Null)
			return;
		auto& N = nodes[node];
		if (!AABBIntersect(bb, N.aabb))
			return;
		if (N.height == 0)
			out.push_back(node);
		else
		{
			_QueryNode(bb, out, N.left);
			_QueryNode(bb, out, N.right);
		}
	}

	vector<Node> nodes;
	int32_t firstFree = Node::Null;
	int32_t count = 0;
	int32_t numNodes = 0;
	int32_t root = Node::Null;
};



template<class Storage> void RunTests(size_t width)
{
	double t;
	Storage s;
	size_t count = width * width;
	size_t qwidth = width / 2;
	size_t qcount = qwidth * qwidth;

	vector<typename Storage::Handle> handles;
	vector<typename Storage::Handle> queryresult;
	vector<AABB> insbboxes;
	vector<AABB> querybboxes;
	handles.resize(count);
	queryresult.reserve(count); // < worst possible case
	insbboxes.resize(count);
	querybboxes.resize(qcount);
	s.Prewarm(count);

	for (size_t y = 0; y < width; ++y)
	{
		for (size_t x = 0; x < width; ++x)
		{
			insbboxes[x + y * width] = { { x + 0.f, y + 0.f, 25 }, { x + 1.f, y + 1.f, 75 } };
		}
	}
	for (size_t y = 0; y < qwidth; ++y)
	{
		for (size_t x = 0; x < qwidth; ++x)
		{
			querybboxes[x + y * qwidth] =
			{
				{ x * 2 + 0.5f, y * 2 + 0.5f, (101 * (x + 1) + y) % 40 + 5.f },
				{ x * 2 + 1.5f, y * 2 + 1.5f, (101 * (x + 1) + y) % 40 + 55.f },
			};
		}
	}

	t = Time();
	for (size_t i = 0; i < count; ++i)
		handles[i] = s.Add(insbboxes[i]);
	double tAdd = Time() - t;
	ASSERT(s.Count() == count);

	s.Validate();

	// test query run
	for (size_t i = 0; i < qcount; ++i)
	{
		queryresult.clear();
		s.Query(querybboxes[i], queryresult);
		if (queryresult.size() != 4)
			printf("#queryresult=%u\n",(unsigned)queryresult.size());
		ASSERT(queryresult.size() == 4);
		for (const auto& h : queryresult)
			ASSERT(AABBIntersect(s.GetAABB(h), querybboxes[i]));
	}

	// timed query run
	t = Time();
	for (size_t i = 0; i < qcount; ++i)
	{
		queryresult.clear();
		s.Query(querybboxes[i], queryresult);
		__asm__ __volatile__("" :: "m" (queryresult));
	}
	double tQuery = Time() - t;

	t = Time();
	for (size_t i = 0; i < count; ++i)
		s.Remove(handles[i]);
	double tRemove = Time() - t;
	ASSERT(s.Count() == 0);

	printf("%12s:|%9.4f|%9.4f|%9.4f\n",
		Storage::NAME
		, tAdd * 1000
		, tQuery * 1000
		, tRemove * 1000
	);
}



void RunAllTestsForCount(unsigned width)
{
	printf("item count: %u (width: %u), query count: %u (width: %u)\n", width * width, width, width * width / 4, width / 2);
	printf("%12s |%9s|%9s|%9s\n", "Method", "Add", "Query", "Remove");
	RunTests<Storage_AABBArray>(width);
	RunTests<Storage_DynAABBTree>(width);
	puts("");
}



int main()
{
	puts("### Methods for storing AABBs for fast queries ###");
	puts("### (times are in milliseconds)                ###");
	puts("");
	RunAllTestsForCount(50);
	RunAllTestsForCount(100);
	RunAllTestsForCount(200);
}


