#pragma once

class gmMachine;
struct GMFunctionInfo;
struct GMClassInfo;

class GMScriptObject : public ScriptObject
{
public:
	GMClassInfo* m_classInfo;

	gmTableObject* m_gmTable;
	gmUserObject* m_gmUserObject;

	bool m_lockedByScript;

	GMScriptObject() :
	m_lockedByScript(false)
	{}

	void Release()
	{
		if (!m_lockedByScript)
			delete this;
	}
};

struct GMFunctionInfo
{
	ScriptContext* m_context;
	FunctionDesc* m_desc;
};

struct GMMethodInfo
{
	GMClassInfo* m_classInfo;
	int m_methodIndex;
};

struct GMClassInfo
{
	ScriptContext* m_context;
	ClassDesc* m_desc;

	vector<GMMethodInfo> m_methodInfos;

	gmType m_gmTypeId;
	vector<gmFunctionEntry> m_gmMethods;
};

//! GM Script Context implementation
class GMScriptContext : public ScriptContext
{
	friend class GMScriptStack;
private:
	gmMachine* m_machine;
	std::vector<GMFunctionInfo*> m_functions;
	std::vector<GMClassInfo*> m_classes;

	GMScriptContext();
	~GMScriptContext();

public:
	static GMScriptContext* CreateContext(int scriptStack);

	void SetLogger(ScriptLogger* logger);
	void CollectGarbage(bool full);
	bool ExecuteString(const char* string);
	ScriptStack* BeginCall(FunctionDesc* desc);
	ScriptStack* BeginCall(const char* name);
	bool RegisterFunction(FunctionDesc* desc);
	bool RegisterUserClass(ClassDesc* desc);

protected:
	GMClassInfo* FindClassInfo(ClassDesc* classDesc);

	static void GMPrintCallback(gmMachine* machine, const char* string);
	static bool GMClassTraceCallback(gmMachine* machine, gmUserObject* object, gmGarbageCollector* gc, const int workLeftToGo, int& workDone);
	static int GMClassConstructorCallback(gmThread* thread);
	static int GMClassDestructorCallback(gmThread* thread);
	static void GMClassGCCallback(gmMachine* machine, gmUserObject* object);
	static int GMClassToStringMethodCallback(gmThread* thread);
	static void GMClassToStringCallback(gmUserObject* object, char* buffer, int bufferSize);
	static int GMClassMethodCallback(gmThread* thread);
	static int GMFunctionCallback(gmThread* thread);
};