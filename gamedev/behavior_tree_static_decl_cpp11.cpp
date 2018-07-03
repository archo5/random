
#include <stdio.h>
#include <string.h>
#include <vector>
#include <typeinfo>
#include <sstream>
#include <initializer_list>


using namespace std;


void lev(int v)
{
	while (v --> 0)
		printf("  ");
}


struct BTSubItem
{
	virtual ~BTSubItem() {}
};

struct BTCond : BTSubItem
{
	BTCond(bool (*cond) ()) : _cond(cond) {}
	operator BTSubItem* () { return this; }
	
	bool (*_cond) ();
};

template <class T>
struct BTAttr
{
	BTAttr(const T& v) : _value(v) {}

	T _value;
	const char* _varName = nullptr;
};

struct BTParamAny : BTSubItem
{
	BTParamAny(const char* name) : _name(name) {}
	operator BTSubItem* () { return this; }
	virtual const char* GetTypeName() = 0;
	
	const char* _name;
};

struct BTParamVar : BTParamAny
{
	BTParamVar(const char* name, const char* varName) : BTParamAny(name), _varName(varName) {}
	const char* GetTypeName() { return "<var>"; }

	const char* _varName;
};

template<class T>
struct BTParamT : BTParamAny
{
	BTParamT(const char* name, const T& value) : BTParamAny(name), _value(value) {}
	const char* GetTypeName() { return typeid(T).name(); }
	
	T _value;
};
template<class T> BTParamT<T> BTParam(const char* name, const T& value) { return BTParamT<T>(name, value); }

struct BTNode : BTSubItem
{
	virtual ~BTNode() {}
	virtual const char* GetName() = 0;

	virtual void DumpAttribs() {}
	template <class T>
	static void _DumpAttrib(const char* name, const BTAttr<T>& attr)
	{
		printf(" [%s=", name);
		if (attr._varName)
			printf(":%s", attr._varName);
		else
		{
			stringstream ss;
			ss << attr._value;
			printf("%s", ss.str().c_str());
		}
		printf("]");
	}
#define BT_ADUMP(x) _DumpAttrib(#x, x)

	virtual void Dump(int level)
	{
		lev(level); printf("%s", GetName());
		if (_cond)
			printf(" [cond]");
		DumpAttribs();
		puts("");
		if (!children.empty())
		{
			lev(level++); puts("{");
			for (auto ch : children)
				ch->Dump(level);
			lev(--level); puts("}");
		}
	}
	virtual void ProcessSubItems(initializer_list<BTSubItem*> items)
	{
		for (auto& item : items)
			if (auto* node = dynamic_cast<BTNode*>(item))
				children.push_back((BTNode*) node);
	}
	
	vector<BTNode*> children;
	bool (*_cond) () = nullptr;
};


template <class T>
struct BTN
{
	BTN(initializer_list<BTSubItem*> items)
	{
		_node = new T();
		_node->ProcessSubItems(items);
	}
	operator T* ()
	{
		return _node;
	}
	operator BTSubItem* ()
	{
		return _node;
	}
	T* _node;
};


struct BTSelector : BTNode
{
	const char* GetName() { return "selector"; }
};

struct BTSequence : BTNode
{
	const char* GetName() { return "sequence"; }
};

struct BTTask : BTNode
{
	virtual void OnBegin() {}
	virtual void OnEnd() {}
	virtual void OnTick() {}
};

struct BTTaskEcho : BTTask
{
	BTTaskEcho(const char* msg) : _message(msg) {}
	const char* GetName() { return "task(echo)"; }
	void OnBegin() override { printf("[%s] begin\n", _message); }
	void OnEnd() override { printf("[%s] end\n", _message); }
	void OnTick() override { printf("[%s] tick\n", _message); }
	void Dump(int level) override
	{
		lev(level); printf("echo \"%s\"\n", _message);
	}
	
	const char* _message = "?";
};

template <class T> void TryParse(BTParamAny* p, const char* nm, BTAttr<T>& out)
{
	if (strcmp(p->_name, nm))
	{
		return;
	}
	if (auto* v = dynamic_cast<BTParamVar*>(p))
	{
		out._varName = v->_varName;
		return;
	}
	if (auto* tp = dynamic_cast<BTParamT<T>*>(p))
		out = tp->_value;
	else
		printf("failed to convert '%s' (%s -> %s)\n", nm, p->GetTypeName(), typeid(T).name());
}

struct BTTaskMoveTo : BTTask
{
	BTTaskMoveTo() {}
	const char* GetName() { return "task(MoveTo)"; }
	void ProcessSubItems(initializer_list<BTSubItem*> items) override
	{
		for (auto& item : items)
		{
			if (auto* p = dynamic_cast<BTParamAny*>(item))
			{
				TryParse(p, "someVal", someVal);
				TryParse(p, "someInt", someInt);
			}
		}
	}
	void DumpAttribs() override
	{
		BT_ADUMP(someVal);
		BT_ADUMP(someInt);
	}
	
	BTAttr<float> someVal = 1.0f;
	BTAttr<int> someInt = 5;
};

struct BTTree
{
	BTTree(BTNode* root) : _root(root) {}
	void Dump()
	{
		puts("BTTree\n{");
		_root->Dump(1);
		puts("}");
	}
	
	BTNode* _root;
};


BTTree testTree
{
	BTN<BTSelector>
	{
		BTN<BTSequence>
		{
			BTCond([](){ return true; }),
			new BTTaskEcho("moving..."),
		},
		BTN<BTSequence>
		{
			BTN<BTTaskMoveTo>
			{
				BTParam("someVal", 3.5f),
				BTParamVar("someInt", "thaVar"),
			},
			new BTTaskEcho("idle..."),
		},
	},
};


int main()
{
	puts("Behavior Tree static declaration test");
	testTree.Dump();
	return 0;
}

