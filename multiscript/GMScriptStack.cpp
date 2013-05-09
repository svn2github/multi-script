#include "ScriptInterface.h"

#include "gmMachine.h"
#include "gmCall.h"

#include "GMScriptContext.h"
#include "GMScriptStack.h"

GMScriptStack::GMScriptStack(gmThread* thread, gmCall* call) :
	m_thread(thread),
	m_numPopped(0),
	m_numPushed(0),
	m_call(call)
{
	m_numParams = m_call ? 0 : m_thread->GetNumParams();
}

GMScriptStack::~GMScriptStack()
{
	if (m_call)
		delete m_call;
}

int GMScriptStack::GetNumParams()
{
	return m_numParams;
}

bool GMScriptStack::PopInt(int& value)
{
	if (m_numPopped == m_numParams) return false;

	if (m_call)
	{
		if (!m_call->GetReturnedInt(value))
			return false;
	}
	else if (!m_thread->ParamInt(m_numParams - m_numPopped - 1, value, 0))
		return false;
	
	m_numPopped++;
	return true;
}

ScriptObject* GMScriptStack::PopScriptObject()
{
	if (m_numPopped == m_numParams) return false;

	gmUserObject* userObject = NULL;
	if (m_call)
	{
		const gmVariable& variable = m_call->GetReturnedVariable();
		if (variable.m_type != GM_USER) return false;
		userObject = (gmUserObject*) variable.m_value.m_ref;
	}
	else if (!m_thread->ParamUserObject(m_numParams - m_numPopped - 1, userObject))
		return false;

	m_numPopped++;
	return (ScriptObject*) userObject->m_user;
}

bool GMScriptStack::PushInt(int value)
{
	if (m_call)
		m_call->AddParamInt(value);
	else
		m_thread->PushInt(value);
	return true;
}

bool GMScriptStack::PushString(const char* string, int length)
{
	if (m_call)
		m_call->AddParamString(string, length);
	else
		m_thread->PushNewString(string, length);
	return true;
}

bool GMScriptStack::PushScriptObject(ScriptObject* abstractScriptObject)
{
	MultiScriptAssert(!m_call);

	GMScriptObject* scriptObject = (GMScriptObject*) abstractScriptObject;
	MultiScriptAssert( scriptObject->m_gmUserObject );
	m_thread->PushUser(scriptObject->m_gmUserObject);
	return true;
}

ScriptObject* GMScriptStack::PushNewScriptObject(ClassDesc* classDesc, void* objectPtr)
{
	MultiScriptAssert(!m_call);

	gmMachine* machine = m_thread->GetMachine();
	GMScriptContext* context = (GMScriptContext*) machine->GetUserData();

	GMClassInfo* classInfo = context->FindClassInfo(classDesc);
	if (!classInfo)
	{
		context->RegisterUserClass(classDesc);
		classInfo = context->FindClassInfo(classDesc);
	}

	GMScriptObject* scriptObject = new GMScriptObject();
	scriptObject->m_classInfo = classInfo;
	scriptObject->m_gmTable = m_thread->GetMachine()->AllocTableObject();
	scriptObject->m_gmUserObject = m_thread->PushNewUser(scriptObject, classInfo->m_gmTypeId);
	scriptObject->m_objectPtr = objectPtr;

	return scriptObject;
}

bool GMScriptStack::EndCall()
{
	MultiScriptAssert(m_call);

	m_call->End();
	m_numParams = m_call->DidReturnVariable() ? 1 : 0;
	m_numPopped = 0;

	return true;
}

void GMScriptStack::ReleaseAfterCall()
{
	MultiScriptAssert(m_call);
	delete this;
}