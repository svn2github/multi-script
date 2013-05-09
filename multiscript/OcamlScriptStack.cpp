#include "ScriptInterface.h"

#include "windows.h"
#define assert MultiScriptAssert

#include "windows.h"

#include "OcamlScriptContext.h"
#include "OcamlScriptStack.h"

OcamlScriptStack::OcamlScriptStack(OcamlScriptContext* context, value* closure, vector<value>& values) :
	m_context(context),
	m_numPushed(0),
	m_isCall(closure != NULL),
	m_values(values),
	m_result(Val_unit),
	m_closure(closure)
{
	m_numParams = (int) values.size();
}

int OcamlScriptStack::GetNumParams()
{
	return m_numParams;
}

bool OcamlScriptStack::PopInt(int& value)
{
	if (m_isCall)
	{
		if (m_numParams == 0) return false;
		value = Int_val(m_result);
		m_numParams--;
	}
	else
	{
		if (m_numParams == 0) return false;
		value = Int_val(m_values[ m_values.size() - m_numParams ]);
		m_numParams--;
	}

	return true;
}

ScriptObject* OcamlScriptStack::PopScriptObject()
{
	MultiScriptAssert(!"Will not implement - ocaml doesn't support class binding.");
	return NULL;
}

bool OcamlScriptStack::PushInt(int value)
{
	if (m_isCall)
	{
		m_values.push_back(Val_int(value));
		m_numPushed++;
	}
	else
	{
		if (m_numPushed == 1) return false;
		m_result = Val_int(value);
		m_numPushed++;
	}
	return true;
}

bool OcamlScriptStack::PushString(const char* string, int length)
{
	MultiScriptAssert(!"Not yet implemented.");
	if (m_numPushed == 1) return false;
	m_numPushed++;
	return true;
}

bool OcamlScriptStack::PushScriptObject(ScriptObject* object)
{
	MultiScriptAssert(!"Will not implement - ocaml doesn't support class binding.");
	return false;
}

ScriptObject* OcamlScriptStack::PushNewScriptObject(ClassDesc* classDesc, void* objectPtr)
{
	MultiScriptAssert(!"Will not implement - ocaml doesn't support class binding.");
	return false;
}

bool OcamlScriptStack::EndCall()
{
	MultiScriptAssert(m_isCall);

	switch (m_values.size())
	{
	case 0:
		MultiScriptAssert(!"Calling ocaml function that don't take any parameters not supported!");
		break;
	case 1:
		m_result = caml_callback(*m_closure, m_values[0]);
		break;
	case 2:
		m_result = caml_callback2(*m_closure, m_values[0], m_values[1]);
		break;
	case 3:
		m_result = caml_callback3(*m_closure, m_values[0], m_values[1], m_values[2]);
		break;
	default:
		{
			m_result = caml_callbackN(*m_closure, (int) m_values.size(), &m_values[0]);
			break;
		}
	}

	// Set number of outputs to 1; obviously this isn't quite right
	m_numParams = 1;
	return true;
}

void OcamlScriptStack::ReleaseAfterCall()
{
	MultiScriptAssert(m_isCall);
	delete this;
}