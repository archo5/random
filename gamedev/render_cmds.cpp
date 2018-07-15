
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



#ifdef _MSC_VER
#  define FINLINE __forceinline
#  define NOINLINE __declspec(noinline)
#else
#  define FINLINE inline __attribute__((always_inline))
#  define NOINLINE inline __attribute__((noinline))
#endif



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
#ifdef _MSC_VER
#define ASSERT(x) if (!(x)) { ERROR(#x, __FUNCSIG__, __FILE__, __LINE__); }
#else
#define ASSERT(x) if (!(x)) { ERROR(#x, __PRETTY_FUNCTION__, __FILE__, __LINE__); }
#endif



void FlushCache()
{
	const size_t moreThanCache = 16 * 1024 * 1024;
	int* p = new int[moreThanCache];
	for (size_t i = 0; i < moreThanCache; i++)
	{
		p[i] = rand();
	}
	delete[] p;
}



struct StackAllocator
{
	struct Page
	{
#define SA_PAGE_SIZE (65536 - sizeof(Page*) - sizeof(char*)) // 4088/4080
		char memory[SA_PAGE_SIZE];
		Page* prev;
		char* writePos;
	};

	void* Alloc(size_t sz)
	{
		// round up to maximum reasonable alignment
		if (sz % 8)
			sz += 8 - sz % 8;
	//	ASSERT(sz <= SA_PAGE_SIZE);
		if (!lastPage || lastPage->memory + SA_PAGE_SIZE - lastPage->writePos < sz)
		{
			Page* np;
			if (freeList)
			{
				// have free pages
				np = freeList;
				freeList = freeList->prev;
			}
			else
			{
				// new page needed
				np = new Page;
			}
			np->prev = lastPage;
			np->writePos = np->memory;
			lastPage = np;
		}
		void* out = lastPage->writePos;
		lastPage->writePos += sz;
		return out;
	}
	void ReservePages(size_t sz, size_t num)
	{
		if (!num || !sz)
			return;
		size_t maxNumPerPage = SA_PAGE_SIZE / sz;
		num += maxNumPerPage;
		while (num > maxNumPerPage)
		{
			auto* np = new Page;
			np->prev = freeList;
			freeList = np;
			num -= maxNumPerPage;
		}
	}
	void RemoveAll()
	{
		while (lastPage)
		{
			auto* p = lastPage->prev;
			lastPage->prev = freeList;
			freeList = lastPage;
			lastPage = p;
		}
	}

	Page* lastPage = nullptr;
	Page* freeList = nullptr;
};



#define COMMAND_1 1
#define COMMAND_2 2
#define COMMAND_3 3

struct ICommandProcessor
{
	virtual void Command1(uint16_t v) = 0;
	virtual void Command2(uint32_t v) = 0;
	virtual void Command3(uint64_t v) = 0;
	virtual void ProcessCommands(struct CommandBaseVirtArr** cmds, size_t count) = 0;
	virtual void ProcessCommands(struct CommandBaseSwitchArr** cmds, size_t count) = 0;
	virtual void ProcessCommands(struct CommandBaseVirtNode* cmds) = 0;
	virtual void ProcessCommands(struct CommandBaseSwitchNode* cmds) = 0;
	virtual void ProcessCommands(struct CommandFullArr* cmds, size_t count) = 0;
	virtual void ProcessCommands(struct CommandFullArr** cmds, size_t count) = 0;
	virtual void ProcessCommands(struct CommandFullNode* cmds) = 0;
};

struct CommandBaseVirtArr
{
	virtual void Execute(ICommandProcessor* cp) = 0;
};
struct Command1VA : CommandBaseVirtArr { void Execute(ICommandProcessor* cp) override { cp->Command1(arg); } uint16_t arg; };
struct Command2VA : CommandBaseVirtArr { void Execute(ICommandProcessor* cp) override { cp->Command2(arg); } uint32_t arg; };
struct Command3VA : CommandBaseVirtArr { void Execute(ICommandProcessor* cp) override { cp->Command3(arg); } uint64_t arg; };

struct CommandBaseVirtNode
{
	virtual void Execute(ICommandProcessor* cp) = 0;
	CommandBaseVirtNode* next;
};
struct Command1VN : CommandBaseVirtNode { void Execute(ICommandProcessor* cp) override { cp->Command1(arg); } uint16_t arg; uint32_t pad[4]; };
struct Command2VN : CommandBaseVirtNode { void Execute(ICommandProcessor* cp) override { cp->Command2(arg); } uint32_t arg; uint32_t pad[4]; };
struct Command3VN : CommandBaseVirtNode { void Execute(ICommandProcessor* cp) override { cp->Command3(arg); } uint64_t arg; uint32_t pad[4]; };

struct CommandBaseSwitchArr
{
	uint16_t type;
};
struct Command1SA : CommandBaseSwitchArr { FINLINE Command1SA() { type = COMMAND_1; } uint16_t arg; uint32_t pad[4]; };
struct Command2SA : CommandBaseSwitchArr { FINLINE Command2SA() { type = COMMAND_2; } uint32_t arg; uint32_t pad[4]; };
struct Command3SA : CommandBaseSwitchArr { FINLINE Command3SA() { type = COMMAND_3; } uint64_t arg; uint32_t pad[4]; };

struct CommandBaseSwitchNode
{
	CommandBaseSwitchNode* next;
	uint16_t type;
};
struct Command1SN : CommandBaseSwitchNode { FINLINE Command1SN() { type = COMMAND_1; } uint16_t arg; uint32_t pad[4]; };
struct Command2SN : CommandBaseSwitchNode { FINLINE Command2SN() { type = COMMAND_2; } uint32_t arg; uint32_t pad[4]; };
struct Command3SN : CommandBaseSwitchNode { FINLINE Command3SN() { type = COMMAND_3; } uint64_t arg; uint32_t pad[4]; };

struct CommandFullArr
{
	uint16_t arg1;
	uint32_t pad1[4];
	uint32_t arg2;
	uint32_t pad2[4];
	uint64_t arg3;
	uint32_t pad3[4];
};
struct CommandFullNode
{
	uint16_t arg1;
	uint32_t pad1[4];
	uint32_t arg2;
	uint32_t pad2[4];
	uint64_t arg3;
	uint32_t pad3[4];
	CommandFullNode* next;
};

struct BlackBoxAPI
{
	void NOINLINE Command1(uint64_t v)
	{
		state1 = v * v ^ v;
	}
	void NOINLINE Command2(uint16_t v)
	{
		state2 = v * v ^ v - v;
	}
	void NOINLINE Command3(uint32_t v)
	{
		counter *= v;
		counter -= v;
		counter ^= state1;
		counter *= v;
		counter ^= v;
		counter *= state2;
		counter += v;
		counter *= v;
	}
	uint64_t GetResult() { return counter; }

	uint64_t state1 = 1;
	uint64_t state2 = 2;
	uint64_t counter = 3;
};

struct CommandProcessor : ICommandProcessor
{
	virtual void Command1(uint16_t v) { api.Command1(v); }
	virtual void Command2(uint32_t v) { api.Command2(v); }
	virtual void Command3(uint64_t v) { api.Command3(v); }
	virtual void ProcessCommands(CommandBaseVirtArr** cmds, size_t count) { ProcessCommandsImpl(cmds, count); }
	virtual void ProcessCommands(CommandBaseSwitchArr** cmds, size_t count) { ProcessCommandsImpl(cmds, count); }
	virtual void ProcessCommands(CommandBaseVirtNode* cmds) { ProcessCommandsImpl(cmds); }
	virtual void ProcessCommands(CommandBaseSwitchNode* cmds) { ProcessCommandsImpl(cmds); }
	virtual void ProcessCommands(struct CommandFullArr* cmds, size_t count) { ProcessCommandsImpl(cmds, count); }
	virtual void ProcessCommands(struct CommandFullArr** cmds, size_t count) { ProcessCommandsImpl(cmds, count); }
	virtual void ProcessCommands(struct CommandFullNode* cmds) { ProcessCommandsImpl(cmds); }

	void ProcessCommandsImpl(CommandBaseVirtArr** cmds, size_t count)
	{
		for (auto cmd = cmds, end = cmds + count; cmd != end; ++cmd)
			(*cmd)->Execute(this);
	}
	void ProcessCommandsImpl(CommandBaseSwitchArr** cmds, size_t count)
	{
		for (auto cmd = cmds, end = cmds + count; cmd != end; ++cmd)
		{
			switch ((*cmd)->type)
			{
			case COMMAND_1:
				api.Command1(static_cast<Command1SA*>(*cmd)->arg);
				break;
			case COMMAND_2:
				api.Command2(static_cast<Command2SA*>(*cmd)->arg);
				break;
			case COMMAND_3:
				api.Command3(static_cast<Command3SA*>(*cmd)->arg);
				break;
			}
		}
	}
	void ProcessCommandsImpl(CommandBaseVirtNode* cmds)
	{
		for (auto* cmd = cmds; cmd; cmd = cmd->next)
			cmd->Execute(this);
	}
	void ProcessCommandsImpl(CommandBaseSwitchNode* cmds)
	{
		for (auto* cmd = cmds; cmd; cmd = cmd->next)
		{
			switch (cmd->type)
			{
			case COMMAND_1:
				api.Command1(static_cast<Command1SN*>(cmd)->arg);
				break;
			case COMMAND_2:
				api.Command2(static_cast<Command2SN*>(cmd)->arg);
				break;
			case COMMAND_3:
				api.Command3(static_cast<Command3SN*>(cmd)->arg);
				break;
			}
		}
	}
	void ProcessCommandsImpl(CommandFullArr* cmds, size_t count)
	{
		for (auto cmd = cmds, end = cmds + count; cmd != end; ++cmd)
		{
			api.Command1(cmd->arg1);
			api.Command2(cmd->arg2);
			api.Command3(cmd->arg3);
		}
	}
	void ProcessCommandsImpl(CommandFullArr** cmds, size_t count)
	{
		for (auto cmd = cmds, end = cmds + count; cmd != end; ++cmd)
		{
			api.Command1((*cmd)->arg1);
			api.Command2((*cmd)->arg2);
			api.Command3((*cmd)->arg3);
		}
	}
	void ProcessCommandsImpl(CommandFullNode* cmds)
	{
		for (auto* cmd = cmds; cmd; cmd = cmd->next)
		{
			api.Command1(cmd->arg1);
			api.Command2(cmd->arg2);
			api.Command3(cmd->arg3);
		}
	}

	BlackBoxAPI api;
};



template <class T, class S> NOINLINE T* DoNotGuess(S* val) { return val; }



struct CommandGenData
{
	uint16_t arg1;
	uint32_t arg2;
	uint32_t begin;
	uint32_t end;
};
#define NITER 100
#define N10K 10000
#define N100K 100000
int main()
{
	puts("### Methods for passing render commands ###");
	puts("### (times are in milliseconds)         ###");
	puts("");

	vector<uint64_t> values;
	vector<CommandGenData> genData;
	StackAllocator stackAlloc;
	values.resize(N100K);
	genData.reserve(N10K);
	for (int i = 0; i < N100K; ++i)
	{
		values[i] = i * 101ULL;
	}
	for (int i = 0; i < N10K; ++i)
	{
		genData.push_back({ uint16_t(i * 123U), i * 125U, (i) * 10U, (i + 1) * 10U });
	}
	stackAlloc.ReservePages(sizeof(CommandFullNode), N100K);

	// just call the commands directly
	uint64_t ref;
	{
		FlushCache();
		double times[1] = {};
		for (int iter = 0; iter < NITER; ++iter)
		{
			BlackBoxAPI api;
			double t = Time();
			for (int i = 0; i < N10K; ++i)
			{
				auto& gd = genData[i];
				api.Command1(gd.arg1);
				api.Command2(gd.arg2);
				for (auto j = gd.begin; j < gd.end; ++j)
					api.Command3(values[j]);
			}
			times[0] += (Time() - t) * 1000;
			ref = api.GetResult();
		}
		printf("%20s: %6.3f\n",
			"call API directly",
			times[0] / 100);
	}

	// call the commands via virtual interface
	{
		FlushCache();
		double times[1] = {};
		for (int iter = 0; iter < NITER; ++iter)
		{
			CommandProcessor cp;
			auto* icp = DoNotGuess<ICommandProcessor>(&cp);
			double t = Time();
			for (int i = 0; i < N10K; ++i)
			{
				auto& gd = genData[i];
				icp->Command1(gd.arg1);
				icp->Command2(gd.arg2);
				for (auto j = gd.begin; j < gd.end; ++j)
					icp->Command3(values[j]);
			}
			times[0] += (Time() - t) * 1000;
			ASSERT(cp.api.GetResult() == ref);
		}
		printf("%20s: %6.3f\n",
			"call thru virtual",
			times[0] / 100);
	}

	// full command array, creation & execution
	{
		FlushCache();
		//vector<CommandFullArr> cmds;
		//cmds.reserve(N100K);
		CommandFullArr* cmds = new CommandFullArr[N100K];
		double times[3] = {};
		for (int iter = 0; iter < NITER; ++iter)
		{
			//cmds.clear();
			auto* wcmd = cmds;
			CommandProcessor cp;
			auto* icp = DoNotGuess<ICommandProcessor>(&cp);
			double t0 = Time();
			for (int i = 0; i < N10K; ++i)
			{
				auto& gd = genData[i];
				for (auto j = gd.begin; j < gd.end; ++j)
				{
					CommandFullArr& cfa = *wcmd++;
					//cmds.emplace_back();
					//CommandFullArr& cfa = cmds.back();
					cfa.arg1 = gd.arg1;
					cfa.arg2 = gd.arg2;
					cfa.arg3 = values[j];
				}
			}
			double t1 = Time();
			//icp->ProcessCommands(cmds.data(), cmds.size());
			icp->ProcessCommands(cmds, wcmd - cmds);
			double t2 = Time();
			times[0] += (t2 - t0) * 1000;
			times[1] += (t1 - t0) * 1000;
			times[2] += (t2 - t1) * 1000;
			ASSERT(ref == cp.api.GetResult());
		}
		delete[] cmds;
		printf("%20s: %6.3f (arr:%6.3f, exec:%6.3f)\n",
			"full array",
			times[0] / 100,
			times[1] / 100,
			times[2] / 100);
	}

	// per-mesh command array, creation & execution (interleaved)
	{
		FlushCache();
		double times[1] = {};
		//vector<CommandFullArr> cmds;
		//cmds.reserve(N100K);
		CommandFullArr* cmds = new CommandFullArr[N100K];
		for (int iter = 0; iter < NITER; ++iter)
		{
			//cmds.clear();
			CommandProcessor cp;
			auto* icp = DoNotGuess<ICommandProcessor>(&cp);
			double t = Time();
			for (int i = 0; i < N10K; ++i)
			{
				auto* wcmd = cmds;
				auto& gd = genData[i];
				for (auto j = gd.begin; j < gd.end; ++j)
				{
					CommandFullArr& cfa = *wcmd++;
					//cmds.emplace_back();
					//CommandFullArr& cfa = cmds.back();
					cfa.arg1 = gd.arg1;
					cfa.arg2 = gd.arg2;
					cfa.arg3 = values[j];
				}
				//icp->ProcessCommands(cmds.data(), cmds.size());
				icp->ProcessCommands(cmds, wcmd - cmds);
				//cmds.clear();
			}
			times[0] += (Time() - t) * 1000;
			ASSERT(ref == cp.api.GetResult());
		}
		delete[] cmds;
		printf("%20s: %6.3f\n",
			"full array [mesh]",
			times[0] / 100);
	}

	// full command linked list, creation & execution
	{
		FlushCache();
		double times[3] = {};
		for (int iter = 0; iter < NITER; ++iter)
		{
			CommandProcessor cp;
			auto* icp = DoNotGuess<ICommandProcessor>(&cp);
			double t0 = Time();
			CommandFullNode* first = new (stackAlloc.Alloc(sizeof(CommandFullNode))) CommandFullNode;
			CommandFullNode* last = first;
			for (int i = 0; i < N10K; ++i)
			{
				auto& gd = genData[i];
				for (auto j = gd.begin; j < gd.end; ++j)
				{
					last = last->next = new (stackAlloc.Alloc(sizeof(CommandFullNode))) CommandFullNode;
					last->arg1 = gd.arg1;
					last->arg2 = gd.arg2;
					last->arg3 = values[j];
				}
			}
			last->next = nullptr;
			double t1 = Time();
			icp->ProcessCommands(first->next);
			double t2 = Time();
			times[0] += (t2 - t0) * 1000;
			times[1] += (t1 - t0) * 1000;
			times[2] += (t2 - t1) * 1000;
			ASSERT(ref == cp.api.GetResult());
			stackAlloc.RemoveAll();
		}
		printf("%20s: %6.3f (lst:%6.3f, exec:%6.3f)\n",
			"full node",
			times[0] / 100,
			times[1] / 100,
			times[2] / 100);
	}

	// switch command linked list, creation & execution
	{
		FlushCache();
		double times[3] = {};
		for (int iter = 0; iter < NITER; ++iter)
		{
			CommandProcessor cp;
			auto* icp = DoNotGuess<ICommandProcessor>(&cp);
			double t0 = Time();
			CommandBaseSwitchNode* first = new (stackAlloc.Alloc(sizeof(Command1SN))) Command1SN;
			CommandBaseSwitchNode* last = first;
			for (int i = 0; i < N10K; ++i)
			{
				auto& gd = genData[i];
				auto* cmd1 = new (stackAlloc.Alloc(sizeof(Command1SN))) Command1SN;
				last = last->next = cmd1;
				cmd1->arg = gd.arg1;
				auto* cmd2 = new (stackAlloc.Alloc(sizeof(Command2SN))) Command2SN;
				last = last->next = cmd2;
				cmd2->arg = gd.arg2;
				for (auto j = gd.begin; j < gd.end; ++j)
				{
					auto* cmd3 = new (stackAlloc.Alloc(sizeof(Command3SN))) Command3SN;
					last = last->next = cmd3;
					cmd3->arg = values[j];
				}
			}
			last->next = nullptr;
			double t1 = Time();
			icp->ProcessCommands(first->next);
			double t2 = Time();
			times[0] += (t2 - t0) * 1000;
			times[1] += (t1 - t0) * 1000;
			times[2] += (t2 - t1) * 1000;
			ASSERT(ref == cp.api.GetResult());
			stackAlloc.RemoveAll();
		}
		printf("%20s: %6.3f (lst:%6.3f, exec:%6.3f)\n",
			"switch node",
			times[0] / NITER,
			times[1] / NITER,
			times[2] / NITER);
	}

	// virtual command linked list, creation & execution
	{
		FlushCache();
		double times[3] = {};
		for (int iter = 0; iter < NITER; ++iter)
		{
			CommandProcessor cp;
			auto* icp = DoNotGuess<ICommandProcessor>(&cp);
			double t0 = Time();
			CommandBaseVirtNode* first = new (stackAlloc.Alloc(sizeof(Command1VN))) Command1VN;
			CommandBaseVirtNode* last = first;
			for (int i = 0; i < N10K; ++i)
			{
				auto& gd = genData[i];
				auto* cmd1 = new (stackAlloc.Alloc(sizeof(Command1VN))) Command1VN;
				last = last->next = cmd1;
				cmd1->arg = gd.arg1;
				auto* cmd2 = new (stackAlloc.Alloc(sizeof(Command2VN))) Command2VN;
				last = last->next = cmd2;
				cmd2->arg = gd.arg2;
				for (auto j = gd.begin; j < gd.end; ++j)
				{
					auto* cmd3 = new (stackAlloc.Alloc(sizeof(Command3VN))) Command3VN;
					last = last->next = cmd3;
					cmd3->arg = values[j];
				}
			}
			last->next = nullptr;
			double t1 = Time();
			icp->ProcessCommands(first->next);
			double t2 = Time();
			times[0] += (t2 - t0) * 1000;
			times[1] += (t1 - t0) * 1000;
			times[2] += (t2 - t1) * 1000;
			ASSERT(ref == cp.api.GetResult());
			stackAlloc.RemoveAll();
		}
		printf("%20s: %6.3f (lst:%6.3f, exec:%6.3f)\n",
			"virtual node",
			times[0] / 100,
			times[1] / 100,
			times[2] / 100);
	}
}


