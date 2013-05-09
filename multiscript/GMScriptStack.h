#pragma once

class gmThread;
class gmCall;

//! GM Script Stack implementation
class GMScriptStack : public ScriptStack
{
	friend class GMScriptContext;
private:
	gmThread* m_thread;
	int m_numParams;
	int m_numPushed;
	int m_numPopped;

	gmCall* m_call;

public:
	GMScriptStack(gmThread* thread, gmCall* call = NULL);
	~GMScriptStack();

	int GetNumParams();

	bool PopInt(int& value);
	ScriptObject* PopScriptObject();

	bool PushInt(int value);
	bool PushString(const char* string, int length);
	bool PushScriptObject(ScriptObject* abstractScriptObject);
	ScriptObject* PushNewScriptObject(ClassDesc* classDesc, void* objectPtr);

	bool EndCall();
	void ReleaseAfterCall();
};