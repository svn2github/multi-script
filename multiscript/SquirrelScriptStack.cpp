#include "ScriptInterface.h"

#include "windows.h"
#define assert MultiScriptAssert

#include "squirrel.h"
#include "sqstdblob.h"
#include "sqstdsystem.h"
#include "sqstdio.h"
#include "sqstdmath.h"	
#include "sqstdstring.h"
#include "sqstdaux.h"
#include "sqvm.h"

#include "windows.h"

#include "SquirrelScriptContext.h"
#include "SquirrelScriptStack.h"

SquirrelScriptStack::SquirrelScriptStack(SquirrelScriptContext* context, bool isCall) :
	m_context(context),
	m_numPushed(0),
	m_isCall(isCall)
{
	m_numParams = isCall ? 0 : (sq_gettop(m_context->m_vm) - 1 /* root table */);
}

int SquirrelScriptStack::GetNumParams()
{
	return m_numParams;
}

bool SquirrelScriptStack::PopInt(int& value)
{
	SQRESULT result = sq_getinteger(m_context->m_vm, -1, &value);
	if (result != SQ_OK) return false;
	sq_pop(m_context->m_vm, 1);
	m_numParams--;
	return true;
}

ScriptObject* SquirrelScriptStack::PopScriptObject()
{
	MultiScriptAssert( sq_gettype(m_context->m_vm, -1) == OT_INSTANCE );

	void* userPtr = NULL;
	sq_getinstanceup(m_context->m_vm, -1, &userPtr, 0);
	sq_pop(m_context->m_vm, 1);

	m_numParams--;
	return (SquirrelScriptObject*) userPtr;
}

bool SquirrelScriptStack::PushInt(int value)
{
	sq_pushinteger(m_context->m_vm, value);
	m_numPushed++;
	return true;
}

bool SquirrelScriptStack::PushString(const char* string, int length)
{
	sq_pushstring(m_context->m_vm, string, length);
	m_numPushed++;
	return true;
}

bool SquirrelScriptStack::PushScriptObject(ScriptObject* object)
{
	MultiScriptAssert(false);
	// TODO: Implement me!!!

	m_numPushed++;
	return false;
}

ScriptObject* SquirrelScriptStack::PushNewScriptObject(ClassDesc* classDesc, void* objectPtr)
{
	SquirrelClassInfo* classInfo = m_context->FindClassInfo(classDesc);
	if (!classInfo)
	{
		m_context->RegisterUserClass(classDesc);
		classInfo = m_context->FindClassInfo(classDesc);
	}

	SquirrelScriptObject* scriptObject = new SquirrelScriptObject();
	scriptObject->m_objectPtr = objectPtr;

	// Find the class
	sq_pushstring(m_context->m_vm, classDesc->m_name, -1);
	sq_get(m_context->m_vm, -2);
	MultiScriptAssert( sq_gettype(m_context->m_vm, -1) == OT_CLASS );

	// Create instance
	sq_createinstance(m_context->m_vm, -1);
	sq_setinstanceup(m_context->m_vm, -1, scriptObject);

	// Remove class from the stack
	sq_remove(m_context->m_vm, -2);

	m_numPushed++;
	return scriptObject;
}

bool SquirrelScriptStack::EndCall()
{
	MultiScriptAssert(m_isCall);

	const int topBefore = sq_gettop(m_context->m_vm);
	SQRESULT result = sq_call(m_context->m_vm, m_numPushed + 1 /* root table */, SQTrue, SQTrue);
	if (result != SQ_OK)
	{
		sq_pop(m_context->m_vm, 2); // Pop root table and function
		return false;
	}
	const int topAfter = sq_gettop(m_context->m_vm);

	// Determine number of outputs
	m_numParams = topAfter - (topBefore - (m_numPushed + 1));
	return true;
}

void SquirrelScriptStack::ReleaseAfterCall()
{
	MultiScriptAssert(m_isCall);
	sq_pop(m_context->m_vm, 2); // Pop function and root table
	delete this;
}