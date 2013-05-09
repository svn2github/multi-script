#pragma once

class LuaScriptContext;

struct LuaFunctionInfo
{
	LuaScriptContext* m_context;
	FunctionDesc* m_desc;
};

struct LuaClassInfo
{
	LuaScriptContext* m_context;
	ClassDesc* m_desc;
};

class LuaScriptObject : public ScriptObject
{
public:
	bool m_lockedByScript;

	LuaScriptObject();
	void Release();
};

/**
 *	Lua script context implementation.
 */
class LuaScriptContext : public ScriptContext
{
	friend class LuaScriptCall;
	friend class LuaScriptStack;
private:
	lua_State* L;
	std::vector<LuaFunctionInfo*> m_functions;
	std::vector<LuaClassInfo*> m_classes;

	LuaScriptContext();
	~LuaScriptContext();

public:
	static LuaScriptContext* CreateContext(int scriptStack);

	void SetLogger(ScriptLogger* logger);
	void CollectGarbage(bool full);
	bool ExecuteString(const char* string);
	ScriptStack* BeginCall(FunctionDesc* desc);
	ScriptStack* BeginCall(const char* name);
	bool RegisterFunction(FunctionDesc* desc);
	bool RegisterUserClass(ClassDesc* desc);

protected:
	LuaClassInfo* FindClassInfo(ClassDesc* classDesc);
	bool LuaCall(int numArgs, int numResults);

	static void LuaPrintCallback(lua_State* L, const char* text);
	static int LuaErrorHandlerCallback(lua_State* L);
	static LuaScriptObject* PopObjectData(lua_State* L);

	static int LuaClassConstructorCallback(lua_State* L);
	static int LuaClassDestructorCallback(lua_State* L);
	static int LuaClassGCCallback(lua_State* L);
	static int LuaClassToStringMethodCallback(lua_State* L);
	static int LuaClassMethodCallback(lua_State* L);
	static int LuaFunctionCallback(lua_State* L);
};