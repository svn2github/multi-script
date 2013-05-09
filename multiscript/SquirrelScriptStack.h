#pragma once

class SquirrelScriptStack : public ScriptStack
{
	friend class SquirrelScriptContext;
private:
	SquirrelScriptContext* m_context;
	int m_numPushed;
	int m_numParams;
	bool m_isCall;

public:
	SquirrelScriptStack(SquirrelScriptContext* context, bool isCall);

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
