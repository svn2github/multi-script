#pragma once

#include "OcamlScriptContext.h"

class OcamlScriptStack : public ScriptStack
{
	friend class OcamlScriptContext;
private:
	OcamlScriptContext* m_context;
	int m_numPushed;
	int m_numParams;
	bool m_isCall;

	std::vector<value> m_values;

	value m_result;

	value* m_closure;

public:
	OcamlScriptStack(OcamlScriptContext* context, value* closure, vector<value>& values = vector<value>());

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
