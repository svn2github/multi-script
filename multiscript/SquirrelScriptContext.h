#pragma once

class SquirrelScriptContext;

struct SquirrelFunctionInfo
{
	SquirrelScriptContext* m_context;
	FunctionDesc* m_desc;
};

struct SquirrelClassInfo
{
	SquirrelScriptContext* m_context;
	ClassDesc* m_desc;
};

class SquirrelScriptObject : public ScriptObject
{
public:
	SquirrelClassInfo* m_classInfo;
	bool m_lockedByScript;

	SquirrelScriptObject();
	void Release();
};

/**
 *	Squirrel script context implementation.
 */
class SquirrelScriptContext : public ScriptContext
{
	friend class SquirrelScriptCall;
	friend class SquirrelScriptStack;
private:
	HSQUIRRELVM m_vm;
	std::vector<SquirrelFunctionInfo*> m_functions;
	std::vector<SquirrelClassInfo*> m_classes;

	const char* m_currentStringProgram;

	SquirrelScriptContext();
	~SquirrelScriptContext();

public:
	static SquirrelScriptContext* CreateContext(int stackSize);

	void SetLogger(ScriptLogger* logger);
	void CollectGarbage(bool full);
	bool ExecuteString(const char* string);
	ScriptStack* BeginCall(FunctionDesc* desc);
	ScriptStack* BeginCall(const char* name);
	bool RegisterFunction(FunctionDesc* desc);
	bool RegisterUserClass(ClassDesc* desc);

protected:
	SquirrelClassInfo* FindClassInfo(ClassDesc* classDesc);

	static void SquirrelPrintCallback(HSQUIRRELVM vm, const SQChar* text, ...);
	static SQInteger SquirrelReadCallback(SQUserPointer userdata);

	static int SquirrelClassConstructorCallback(HSQUIRRELVM vm);
	static int SquirrelClassDestructorCallback(HSQUIRRELVM vm);
	static int SquirrelClassGCCallback(SQUserPointer userPtr, SQInteger size);
	static int SquirrelClassGCCallback(HSQUIRRELVM vm);
	static int SquirrelClassToStringMethodCallback(HSQUIRRELVM vm);
	static int SquirrelClassMethodCallback(HSQUIRRELVM vm);
	static int SquirrelFunctionCallback(HSQUIRRELVM vm);
};