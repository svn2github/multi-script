#pragma once

class LuaScriptStack : public ScriptStack
{
	friend class LuaScriptContext;
private:
	LuaScriptContext* m_context;
	int m_numPushed;
	int m_numParams;
	bool m_isCall;

public:
	LuaScriptStack(LuaScriptContext* context, bool isCall);

	int GetNumParams();

	bool PopInt(int& value);
	ScriptObject* PopScriptObject();

	bool PushInt(int value);
	bool PushString(const char* string, int length);
	bool PushScriptObject(ScriptObject* object);
	ScriptObject* PushNewScriptObject(ClassDesc* classDesc, void* objectPtr);

	bool EndCall();
	void ReleaseAfterCall();
};
