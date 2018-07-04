
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <conio.h>


#ifdef DLL

struct incrementor
{
	incrementor() : value(0)
	{
		puts("[dll] inc ctor");
	}
	~incrementor()
	{
		puts("[dll] inc dtor");
	}
	int inc()
	{
		value += 1; // change between loads to see effects
		return value;
	}
	int value;
};

extern "C" __declspec(dllexport) int inc()
{
	static incrementor i;
	return i.inc();
}

#else

HMODULE dll;
int (*pinc)();

void cmd(const char* cmd)
{
	printf("> %s\n", cmd);
	system(cmd);
}

void compile()
{
	puts("[app] compiling DLL...");

	cmd("g++ -shared -DDLL -o " __FILE__ ".dll " __FILE__);

	puts("[app] compile done");
}

void copy()
{
	char bfr[300];
	sprintf(bfr, "%s.%d.dll", __FILE__, GetCurrentProcessId());

	printf("[app] copying file %s.dll to %s\n", __FILE__, bfr);

	CopyFile(__FILE__ ".dll", bfr, FALSE);
}

void delcopy()
{
	char bfr[300];
	sprintf(bfr, "%s.%d.dll", __FILE__, GetCurrentProcessId());

	printf("[app] deleting %s...\n", bfr);

	DeleteFileA(bfr);
}

void load()
{
	puts("[app] loading DLL...");

	char bfr[300];
	sprintf(bfr, "%s.%d.dll", __FILE__, GetCurrentProcessId());
	dll = LoadLibraryA(bfr);

	printf("[app] DLL loading %s\n", dll ? "done" : "failed");

	*(void**)&pinc = (void*) GetProcAddress(dll, "inc");

	printf("[app] function address: %p\n", pinc);
}

void unload()
{
	puts("[app] unloading DLL...");

	FreeLibrary(dll);
}

int main()
{
	compile();
	copy();
	load();

	char ch;
	while ((ch = _getch()) != EOF)
	{
		if (ch == 'r')
		{
			compile();
			unload();
			copy();
			load();
		}
		else if (ch == 'i')
		{
			int v = (*pinc)();
			printf("[app] inc value=%d\n", v);
		}
		else if (ch == 'q')
			break;
	}

	unload();
	delcopy();
}

#endif
