#include "LuaScriptCommon.h"
#include "LuaScriptContext.h"
#include "LuaScriptStack.h"

extern "C"
{
	lua_print_callback_func lua_print_callback;
}

LuaScriptObject::LuaScriptObject() :
	m_lockedByScript(false)
{}

void LuaScriptObject::Release()
{
	if (!m_lockedByScript)
		delete this;
}

LuaScriptContext::LuaScriptContext() :
	L(NULL)
{}

LuaScriptContext::~LuaScriptContext()
{
	lua_close(L);
}

LuaScriptContext* LuaScriptContext::CreateContext(int stackSize)
{
	lua_print_callback = LuaPrintCallback;

	LuaScriptContext* context = new LuaScriptContext();

	lua_State* L = lua_open();
	L->user_data = context;
	luaL_openlibs(L);

	context->L = L;

	return context;
}

void LuaScriptContext::SetLogger(ScriptLogger* logger)
{
	m_logger = logger;
}

void LuaScriptContext::CollectGarbage(bool full)
{
	lua_gc(L, full ? LUA_GCCOLLECT : LUA_GCSTEP, 0);
}

bool LuaScriptContext::ExecuteString(const char* string)
{
	//lua_pushcfunction(L, LuaErrorHandlerCallback);
	luaL_loadstring(L, string);

	return LuaCall(0, LUA_MULTRET);
}

ScriptStack* LuaScriptContext::BeginCall(FunctionDesc* desc)
{
	return BeginCall(desc->m_name);
}

ScriptStack* LuaScriptContext::BeginCall(const char* name)
{
	if (lua_gettop(L))
	{
		const char* s = lua_tolstring(L, -1, NULL);
		char s2;
		s2 = *s;
	}

	MultiScriptAssert( lua_gettop(L) == 0 );
	lua_getfield(L, LUA_GLOBALSINDEX, name);
	if (!lua_isfunction(L, -1))
	{
		lua_pop(L, 1);
		return NULL;
	}

	return new LuaScriptStack(this, true);
}

bool LuaScriptContext::RegisterFunction(FunctionDesc* desc)
{
	LuaFunctionInfo* info = new LuaFunctionInfo();
	info->m_desc = desc;
	info->m_context = this;

	lua_pushlightuserdata(L, info);
	lua_pushcclosure(L, LuaFunctionCallback, 1);
	lua_setfield(L, LUA_GLOBALSINDEX, desc->m_name);

	m_functions.push_back(info);
	return true;
}

bool LuaScriptContext::RegisterUserClass(ClassDesc* desc)
{
	MultiScriptAssert(desc->m_constructor);
	MultiScriptAssert(desc->m_destructor);

	LuaClassInfo* info = new LuaClassInfo();
	info->m_desc = desc;
	info->m_context = this;

	lua_newtable(L);
	int methods = lua_gettop(L);

	luaL_newmetatable(L, desc->m_name);
	int metatable = lua_gettop(L);

	// store method table in globals so that
	// scripts can add functions written in Lua.
	lua_pushstring(L, desc->m_name);
	lua_pushvalue(L, methods);
	lua_settable(L, LUA_GLOBALSINDEX);

	lua_pushliteral(L, "__metatable");
	lua_pushvalue(L, methods);
	lua_settable(L, metatable);  // hide metatable from Lua getmetatable()

	lua_pushliteral(L, "__index");
	lua_pushvalue(L, methods);
	lua_settable(L, metatable);

	lua_pushliteral(L, "__tostring");
	lua_pushlightuserdata(L, info);
	lua_pushcclosure(L, LuaClassToStringMethodCallback, 1);
	lua_settable(L, metatable);

	// Garbage collecting
	lua_pushliteral(L, "__gc");
	lua_pushlightuserdata(L, info);
	lua_pushcclosure(L, LuaClassGCCallback, 1);
	lua_settable(L, metatable);
		
	// Optional destruction callback
	if (!desc->m_garbageCollect)
	{
		lua_pushliteral(L, "Destroy");
		lua_pushlightuserdata(L, info);
		lua_pushcclosure(L, LuaClassDestructorCallback, 1);
		lua_settable(L, methods);
	}

	// mt for method table
	lua_newtable(L);
	int mt = lua_gettop(L);

	lua_pushliteral(L, "__call");
	lua_pushlightuserdata(L, info);
	lua_pushcclosure(L, LuaClassConstructorCallback, 1);

	// Handle call to class name as a 'new'
	lua_settable(L, mt);            // mt.__call = constructor

	// Set mt as a metatable for methods table
	lua_setmetatable(L, methods);

	// fill method table with methods from class T
	for (unsigned int i = 0; i < desc->m_methods.size(); ++i)
	{
		lua_pushstring(L, desc->m_methods[i].m_name);
		lua_pushlightuserdata(L, info);
		lua_pushinteger(L, i);
		lua_pushcclosure(L, LuaClassMethodCallback, 2);
		lua_settable(L, methods);
	}

	lua_pop(L, 2);  // drop metatable and method table

	MultiScriptAssert( lua_gettop(L) == 0 );

	// Add class info
	m_classes.push_back(info);
	return true;
}

LuaClassInfo* LuaScriptContext::FindClassInfo(ClassDesc* classDesc)
{
	for (unsigned int i = 0; i < m_classes.size(); ++i)
		if (m_classes[i]->m_desc == classDesc)
			return m_classes[i];
	return NULL;
}

bool LuaScriptContext::LuaCall(int numArgs, int numResults)
{
	const int statusCode = lua_pcall(L, numArgs, numResults, 0);
	const char* errorMessage = (statusCode != 0) ? lua_tostring(L, -1) : NULL;
	switch (statusCode)
	{
		case LUA_ERRRUN:
		{
			m_logger->Output("EXECUTION ERROR:\n%s\n", errorMessage);
			return false;
		}

		case LUA_ERRSYNTAX:
		{
			m_logger->Output("SYNTAX ERROR: %s\n", errorMessage);
			return false;
		}

		case LUA_ERRMEM:
		{
			m_logger->Output("MEMORY ERROR: %s\n", errorMessage);
			return false;
		}

		case LUA_ERRERR:
		{
			m_logger->Output("ERROR in ERROR HANDLER: %s\n", errorMessage);
			return false;
		}
	}
	return true;
}

void LuaScriptContext::LuaPrintCallback(lua_State* L, const char* text)
{
	LuaScriptContext* context = (LuaScriptContext*) L->user_data;
	if (context->m_logger)
		context->m_logger->Output(text);
}

int LuaScriptContext::LuaErrorHandlerCallback(lua_State* L)
{
	LuaScriptContext* context = (LuaScriptContext*) L->user_data;
	if (!context->m_logger)
		return 0;

	lua_Debug debug;
	int i = 0;
	while (lua_getstack(L, i, &debug) == 1)
	{
		lua_getinfo(L, ">n", &debug);
		lua_getinfo(L, ">S", &debug);
		lua_getinfo(L, ">l", &debug);
		lua_getinfo(L, ">u", &debug);
		
		char buffer[1024];
		MultiScriptSprintf(buffer, 1024, "Stack %d: %s line:%d\n", i, debug.source, debug.currentline);
		context->m_logger->Output(buffer);

		i++;
	}
	return 0;
}

LuaScriptObject* LuaScriptContext::PopObjectData(lua_State* L)
{
	MultiScriptAssert( lua_isuserdata(L, 1) );
	void** scriptObjectData = (void**) lua_touserdata(L, 1);
	MultiScriptAssert(scriptObjectData && *scriptObjectData);
	lua_remove(L, 1);
	return (LuaScriptObject*) *scriptObjectData;
}

int LuaScriptContext::LuaClassConstructorCallback(lua_State* L)
{
	MultiScriptAssert( lua_isuserdata(L, lua_upvalueindex(1)) );
	LuaClassInfo* classInfo = (LuaClassInfo*) lua_touserdata(L, lua_upvalueindex(1));

	// Ignore metatable
	MultiScriptAssert( lua_type(L, 1) == LUA_TTABLE );
	lua_remove(L, 1);

	LuaScriptObject* scriptObject = new LuaScriptObject();

	LuaScriptStack stack(classInfo->m_context, false);
	classInfo->m_desc->m_constructor(&stack, scriptObject);
	MultiScriptAssert( scriptObject->m_objectPtr );
	MultiScriptAssert( stack.m_numPushed == 0 );

	void** scriptObjectData = (void**) lua_newuserdata(L, sizeof(void*));
	*scriptObjectData = scriptObject;
	luaL_getmetatable(L, classInfo->m_desc->m_name);
	MultiScriptAssert( lua_type(L, -1) == LUA_TTABLE );
	lua_setmetatable(L, -2);

	return 1;
}

int LuaScriptContext::LuaClassDestructorCallback(lua_State* L)
{
	MultiScriptAssert( lua_isuserdata(L, lua_upvalueindex(1)) );
	LuaClassInfo* classInfo = (LuaClassInfo*) lua_touserdata(L, lua_upvalueindex(1));

	LuaScriptObject* scriptObject = PopObjectData(L);
	MultiScriptAssert(scriptObject);

	scriptObject->m_lockedByScript = true;
	classInfo->m_desc->m_destructor(scriptObject);
	scriptObject->m_lockedByScript = false;
	scriptObject->m_objectPtr = NULL;

	return 0;
}

int LuaScriptContext::LuaClassGCCallback(lua_State* L)
{
	LuaScriptObject* scriptObject = PopObjectData(L);
	MultiScriptAssert(scriptObject);

	if (!scriptObject->m_objectPtr)
	{
		delete scriptObject;
		return 0;
	}

	MultiScriptAssert( lua_isuserdata(L, lua_upvalueindex(1)) );
	LuaClassInfo* classInfo = (LuaClassInfo*) lua_touserdata(L, lua_upvalueindex(1));

	if (classInfo->m_desc->m_garbageCollect)
	{
		scriptObject->m_lockedByScript = true;
		classInfo->m_desc->m_destructor(scriptObject);
	}

	delete scriptObject;
	return 0;
}

int LuaScriptContext::LuaClassToStringMethodCallback(lua_State* L)
{
	MultiScriptAssert( lua_isuserdata(L, lua_upvalueindex(1)) );
	LuaClassInfo* info = (LuaClassInfo*) lua_touserdata(L, lua_upvalueindex(1));

	LuaScriptObject* scriptObject = PopObjectData(L);

	char buffer[1 << 8];
	if (info->m_desc->m_toStringMethod)
	{
		LuaScriptStack stack(info->m_context, false);
		const bool result = info->m_desc->m_toStringMethod(scriptObject, buffer, 1 << 8);
		MultiScriptAssert( result );
		MultiScriptAssert( stack.m_numPushed == 0 );
	}
	else
		MultiScriptSprintf(buffer, 1 << 8, "[%s]", info->m_desc->m_name);

	lua_pushstring(L, buffer);
	return 1;
}

int LuaScriptContext::LuaClassMethodCallback(lua_State* L)
{
	MultiScriptAssert( lua_isuserdata(L, lua_upvalueindex(1)) );
	LuaClassInfo* info = (LuaClassInfo*) lua_touserdata(L, lua_upvalueindex(1));

	MultiScriptAssert( lua_isnumber(L, lua_upvalueindex(2)) );
	const int methodIndex = (int) lua_tointeger(L, lua_upvalueindex(2));

	LuaScriptObject* scriptObject = PopObjectData(L);

	LuaScriptStack stack(info->m_context, false);
	const bool result = info->m_desc->m_methods[methodIndex].m_method(scriptObject, &stack);
	MultiScriptAssert( result );

	return stack.m_numPushed;
}

int LuaScriptContext::LuaFunctionCallback(lua_State* L)
{
	MultiScriptAssert( lua_isuserdata(L, lua_upvalueindex(1)) );
	LuaFunctionInfo* info = (LuaFunctionInfo*) lua_touserdata(L, lua_upvalueindex(1));

	LuaScriptStack stack(info->m_context, false);
	const bool result = info->m_desc->m_function(&stack);
	MultiScriptAssert( result );

	return stack.m_numPushed;
}


ScriptContext* CreateLuaScriptContext(int scriptStack)
{
	return LuaScriptContext::CreateContext(scriptStack);
}