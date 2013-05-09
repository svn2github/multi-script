#pragma once

extern "C"
{
	#include "misc.h"
	#include "mlvalues.h"
	#include "callback.h"
	#include "sys.h"
	#include "startup.h"
	#include "ocaml_io.h"
	#include "misc.h"
	#include "memory.h"
};

class OcamlScriptContext;

struct OcamlFunctionInfo
{
	OcamlScriptContext* m_context;
	FunctionDesc* m_desc;
};

struct OcamlClassInfo
{
	OcamlScriptContext* m_context;
	ClassDesc* m_desc;
};

class OcamlScriptObject : public ScriptObject
{
public:
	OcamlClassInfo* m_classInfo;
	bool m_lockedByScript;

	OcamlScriptObject();
	void Release();
};

/**
 *	Ocaml script context implementation.
 */
class OcamlScriptContext : public ScriptContext
{
	friend class OcamlScriptCall;
	friend class OcamlScriptStack;
private:
	std::vector<OcamlFunctionInfo*> m_functions;
	std::vector<OcamlClassInfo*> m_classes;

	bool m_dirtyFakedDLL;

	static OcamlScriptContext* s_instance;

	OcamlScriptContext();
	~OcamlScriptContext();

public:
	static OcamlScriptContext* CreateContext(int stackSize);

	void SetLogger(ScriptLogger* logger);
	void CollectGarbage(bool full);
	bool ExecuteString(const char* string);
	ScriptStack* BeginCall(FunctionDesc* desc);
	ScriptStack* BeginCall(const char* name);
	bool RegisterFunction(FunctionDesc* desc);
	bool RegisterUserClass(ClassDesc* desc);

protected:
	void RebuildFakeDLL();

	OcamlClassInfo* FindClassInfo(ClassDesc* classDesc);

	static void OcamlPrintCallback(int fd, char* p, int n);
	static void OcamlFatalErrorCallback(char* fmt, ...);

	static int OcamlClassConstructorCallback();
	static int OcamlClassDestructorCallback();
	static int OcamlClassGCCallback();
	static int OcamlClassToStringMethodCallback();
	static int OcamlClassMethodCallback();

	static value OcamlFunctionCallback0Args();
	static value OcamlFunctionCallback1Arg(value a0);
	static value OcamlFunctionCallback2Args(value a1, value a2);
};