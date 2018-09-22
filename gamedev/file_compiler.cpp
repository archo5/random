
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <cinttypes>

using namespace std;


uint32_t fnv1(const char* s)
{
	uint32_t hash = 0x811c9dc5;
	while (*s)
	{
		hash *= 16777619U;
		hash ^= *s++;
	}
	return hash;
}

void DumpBinaryData(const char* data, size_t size)
{
	enum
	{
		NUM_COLS = 16,
		ROW_STRIDE = NUM_COLS * 8,
	};

	puts("");

	// header
	printf("         "); // address space
	for (int i = 0; i < NUM_COLS; i++)
		printf(" %02X", i);
	puts("  [---- text ----]");
	puts("");

	// data
	for (size_t off = 0; off < size; off += 16)
	{
		// row offset
		printf("%08X ", int(off));

		// data bytes
		for (int i = 0; i < NUM_COLS; i++)
		{
			if (off + i < size)
			{
				unsigned char byte = data[off + i];
				printf(" %02X", byte);
			}
			else
				printf("   ");
		}
		printf("  ");
		for (int i = 0; i < NUM_COLS; i++)
		{
			if (off + i < size)
			{
				char ch = data[off + i];
				if (isprint(ch))
					putchar(ch);
				else
					putchar('.');
			}
			else
				putchar(' ');
		}

		puts("");
	}

	puts("");
}


struct FileCompiler
{
	struct RefEntry
	{
		uint32_t* offset;
		const void* mem;
	};
	struct PtrRefEntry
	{
		uintptr_t* pointer;
	};
	struct RefStrEntry
	{
		uint32_t* offset;
	};
	struct DataEntry
	{
		const void* mem;
		uint32_t offset;
		uint32_t size;
	};

	// stage 1: referencing
	void Ref(uint32_t& offset, const void* mem)
	{
		refs.push_back({ &offset, mem });
	}
	template <class T> void RefPtr(T*& ptr, const void* mem)
	{
		memcpy(&ptr, &mem, sizeof(void*));
		ptrRefs.push_back({ reinterpret_cast<uintptr_t*>(&ptr) });
	}
	void RefLPStr1(uint32_t& offset, const char* str)
	{
		size_t len = strlen(str);
		assert(len <= 255);
		offset = strData.size();
		strData.push_back(len);
		strData.append(str, len + 1);
		strRefs.push_back({ &offset });
	}
	void PtrRefStr(const char*& ptr, const char* str)
	{
		reinterpret_cast<uintptr_t&>(ptr) = strData.size();
		strData.append(str);
		strData.push_back(0);
		ptrStrRefs.push_back({ reinterpret_cast<uintptr_t*>(&ptr) });
	}

	// stage 2: layout
	uint32_t AddData(const void* mem, uint32_t size, uint32_t align)
	{
		if (curOffset % align)
			curOffset += align - (curOffset % align);
		dataEntries.push_back({ mem, curOffset, size });
		uint32_t ret = curOffset;
		curOffset += size;
		return ret;
	}
	template <class T> uint32_t AddDataStruct(const T& s)
	{
		return AddData(&s, sizeof(s), alignof(s));
	}
	template <class T> uint32_t AddDataArray(const T* a, size_t s)
	{
		return AddData(a, sizeof(*a) * s, alignof(*a));
	}
	void AddStringTable()
	{
		// at least one string => at least one byte in strData
		if (strData.size())
			strTableOff = AddData(strData.data(), strData.size(), 1);
	}

	// stage 3: linking
	uint32_t _FindOffset(const void* mem)
	{
		// TODO optimize (may need to pre-sort a copy of data entries by memory pointer to support binary search)
		for (const auto& entry : dataEntries)
		{
			if (uintptr_t(entry.mem) <= uintptr_t(mem) && uintptr_t(mem) < uintptr_t(entry.mem) + entry.size)
				return uintptr_t(mem) - uintptr_t(entry.mem) + entry.offset;
		}
		assert(!"Offset not found");
		return 0;
	}
	void LinkData()
	{
		for (const auto& ref : refs)
		{
			*ref.offset = _FindOffset(ref.mem);
		}
		for (const auto& ref : ptrRefs)
		{
			*ref.pointer = _FindOffset(reinterpret_cast<const void*>(*ref.pointer));
		}
		for (const auto& ref : strRefs)
		{
			*ref.offset += strTableOff;
		}
		for (const auto& ref : ptrStrRefs)
		{
			*ref.pointer += strTableOff;
		}
	}

	// stage 4: file generation
	void GenerateFile(vector<char>& out)
	{
		out.clear();
		out.resize(curOffset, 0);
		for (const auto& entry : dataEntries)
		{
			memcpy(out.data() + entry.offset, entry.mem, entry.size);
		}
	}

	vector<RefEntry> refs;
	vector<PtrRefEntry> ptrRefs;
	vector<RefStrEntry> strRefs;
	vector<PtrRefEntry> ptrStrRefs;
	// TODO pooling
	string strData;

	vector<DataEntry> dataEntries;
	uint32_t strTableOff = 0;
	uint32_t curOffset = 0;
};


template <class T> void FixPtr(T*& ptr, const void* base)
{
	reinterpret_cast<uintptr_t&>(ptr) += reinterpret_cast<uintptr_t>(base);
}


// format:
struct Header
{
	template <class T> T* GetPtr(uint32_t off)
	{
		return reinterpret_cast<T*>(reinterpret_cast<char*>(this) + off);
	}
	const char* GetCStr(uint32_t off)
	{
		return reinterpret_cast<char*>(this) + off + 1;
	}

	uint32_t magic;
	uint32_t offStartOfHashes; // +*Header => Hash*
	uint32_t numHashes;
};
struct Hash
{
	uint32_t offName; // +*Header => uint8 size, char[size] text, char NUL = '\0';
	uint32_t hash;
};
// format 2:
struct Hash2
{
	const char* name;
	uint32_t hash;
};
struct Header2
{
	void FixPointers()
	{
		FixPtr(hashes, this);
		for (uint32_t i = 0; i < numHashes; i++)
			FixPtr(hashes[i].name, this);
	}

	uint32_t magic;
	Hash2* hashes;
	uint32_t numHashes;
};


int main()
{
	puts("\n File compiler test 1 - compiling a list of strings and hashes (offset refs)");
	{
		FileCompiler C;

		Header hdr;
		vector<Hash> hashes;

		// generate data & references
		hashes.reserve(3);
		for (auto* str : { "apple", "banana", "carrot" })
		{
			hashes.push_back({ 0, fnv1(str) });
			C.RefLPStr1(hashes.back().offName, str);
		}

		hdr.magic = 12345;
		C.Ref(hdr.offStartOfHashes, hashes.data());
		hdr.numHashes = hashes.size();

		// generate layout
		C.AddDataStruct(hdr);
		C.AddDataArray(hashes.data(), hashes.size());
		C.AddStringTable();

		// link
		C.LinkData();

		// export file
		vector<char> genData;
		C.GenerateFile(genData);
		DumpBinaryData(genData.data(), genData.size());

		// read the file as if it was regular structs
		puts("dumping exported data...");
		auto* xhdr = reinterpret_cast<Header*>(genData.data());
		printf("magic: %u, offset: %u, numHashes: %u\n", xhdr->magic, xhdr->offStartOfHashes, xhdr->numHashes);
		Hash* xhashes = xhdr->GetPtr<Hash>(xhdr->offStartOfHashes);
		for (uint32_t i = 0; i < xhdr->numHashes; i++)
		{
			printf("#%u: \"%s\" = %08X\n", i, xhdr->GetCStr(xhashes[i].offName), xhashes[i].hash);
		}
	}

	puts("\n File compiler test 2 - compiling a list of strings and hashes (ptr refs)");
	{
		FileCompiler C;

		Header2 hdr;
		vector<Hash2> hashes;

		// generate data & references
		hashes.reserve(3);
		for (auto* str : { "airplane", "boat", "car" })
		{
			hashes.push_back({ 0, fnv1(str) });
			C.PtrRefStr(hashes.back().name, str);
		}

		hdr.magic = 12345;
		C.RefPtr(hdr.hashes, hashes.data());
		hdr.numHashes = hashes.size();

		// generate layout
		C.AddDataStruct(hdr);
		C.AddDataArray(hashes.data(), hashes.size());
		C.AddStringTable();

		// link
		C.LinkData();

		// export file
		vector<char> genData;
		C.GenerateFile(genData);
		DumpBinaryData(genData.data(), genData.size());

		// read the file as if it was regular structs
		puts("dumping exported data...");
		auto* xhdr = reinterpret_cast<Header2*>(genData.data());
		xhdr->FixPointers();
		printf("magic: %u, hashes: %p, numHashes: %u\n", xhdr->magic, xhdr->hashes, xhdr->numHashes);
		for (uint32_t i = 0; i < xhdr->numHashes; i++)
		{
			printf("#%u: \"%s\" = %08X\n", i, xhdr->hashes[i].name, xhdr->hashes[i].hash);
		}
	}
}

