#include "LuaScriptCommon.h"
#include "LuaScriptContext.h"
#include "LuaScriptStack.h"

LuaScriptStack::LuaScriptStack(LuaScriptContext* context, bool isCall) :
	m_context(context),
	m_numPushed(0),
	m_isCall(isCall)
{
	m_numParams = isCall ? 0 : lua_gettop(m_context->L);
}

int LuaScriptStack::GetNumParams()
{
	return m_numParams;
}

bool LuaScriptStack::PopInt(int& value)
{
	if (!lua_isnumber(m_context->L, -1)) return false;
	value = (int) lua_tointeger(m_context->L, -1);
	lua_pop(m_context->L, 1);
	m_numParams--;
	return true;
}

ScriptObject* LuaScriptStack::PopScriptObject()
{
	if (!lua_isuserdata(m_context->L, -1)) return false;
	void** scriptObjectPtr = (void**) lua_touserdata(m_context->L, -1);
	MultiScriptAssert(scriptObjectPtr && *scriptObjectPtr);
	lua_pop(m_context->L, 1);
	m_numParams--;

	return (LuaScriptObject*) *scriptObjectPtr;
}

bool LuaScriptStack::PushInt(int value)
{
	lua_pushinteger(m_context->L, value);
	m_numPushed++;
	return true;
}

bool LuaScriptStack::PushString(const char* string, int length)
{
	if (length == -1) lua_pushstring(m_context->L, string);
	else lua_pushlstring(m_context->L, string, length);
	m_numPushed++;
	return true;
}

bool LuaScriptStack::PushScriptObject(ScriptObject* object)
{
	MultiScriptAssert(false);
	// TODO: Implement me!!!

	m_numPushed++;
	return false;
}

ScriptObject* LuaScriptStack::PushNewScriptObject(ClassDesc* classDesc, void* objectPtr)
{
	LuaClassInfo* classInfo = m_context->FindClassInfo(classDesc);
	if (!classInfo)
	{
		m_context->RegisterUserClass(classDesc);
		classInfo = m_context->FindClassInfo(classDesc);
	}

	LuaScriptObject* scriptObject = new LuaScriptObject();
	scriptObject->m_objectPtr = objectPtr;

	void** scriptObjectData = (void**) lua_newuserdata(m_context->L, sizeof(void*));
	*scriptObjectData = scriptObject;
	luaL_getmetatable(m_context->L, classInfo->m_desc->m_name);
	MultiScriptAssert( lua_type(m_context->L, -1) == LUA_TTABLE );
	lua_setmetatable(m_context->L, -2);

	m_numPushed++;

	return scriptObject;
}

bool LuaScriptStack::EndCall()
{
	MultiScriptAssert(m_isCall);

	if (!m_context->LuaCall(m_numPushed, LUA_MULTRET))
		return false;

	m_numParams = lua_gettop(m_context->L);
	return true;
}

void LuaScriptStack::ReleaseAfterCall()
{
	MultiScriptAssert(m_isCall);
	delete this;
}