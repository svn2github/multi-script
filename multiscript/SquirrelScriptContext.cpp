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

#include "SquirrelScriptStack.h"
#include "SquirrelScriptContext.h"

SquirrelScriptObject::SquirrelScriptObject() :
	m_lockedByScript(false)
{}

void SquirrelScriptObject::Release()
{
	if (!m_lockedByScript)
		delete this;
}

SquirrelScriptContext::SquirrelScriptContext() :
	m_vm(NULL)
{}

SquirrelScriptContext::~SquirrelScriptContext()
{
	sq_close(m_vm);
}

SquirrelScriptContext* SquirrelScriptContext::CreateContext(int stackSize)
{
	SquirrelScriptContext* context = new SquirrelScriptContext();

	HSQUIRRELVM vm = sq_open(stackSize);
	vm->user_data = context;

	sq_setprintfunc(vm, SquirrelPrintCallback);
	sqstd_seterrorhandlers(vm);

	// Push root table so stdlibs can register themselves to it
	sq_pushroottable(vm);

	// Register stdlibs
	sqstd_register_iolib(vm);
	sqstd_register_mathlib(vm);
	sqstd_register_stringlib(vm);
	sqstd_register_systemlib(vm);

	context->m_vm = vm;

	return context;
}

void SquirrelScriptContext::SetLogger(ScriptLogger* logger)
{
	m_logger = logger;
}

void SquirrelScriptContext::CollectGarbage(bool full)
{
	sq_collectgarbage(m_vm);
}

bool SquirrelScriptContext::ExecuteString(const char* string)
{
	SQRESULT result = 0;

	// Compile from string and push function onto the stack
	m_currentStringProgram = string;
	result = sq_compile(
		m_vm,
		SquirrelReadCallback,
		this,
		"some script",
		SQTrue);
	if (result != SQ_OK) return false;

	// Push root table (environment)
	sq_pushroottable(m_vm);

	// Execute function
	result = sq_call(m_vm, 1 /* root table */, SQFalse, SQTrue);
	sq_pop(m_vm, 1); // Pop function

	return result == SQ_OK;
}

ScriptStack* SquirrelScriptContext::BeginCall(FunctionDesc* desc)
{
	return BeginCall(desc->m_name);
}

ScriptStack* SquirrelScriptContext::BeginCall(const char* name)
{
	// Retrieve function
	sq_pushroottable(m_vm);
	sq_pushstring(m_vm, name, -1);
	sq_get(m_vm, -2);

	// Verify the function exists
	const SQObjectType type = sq_gettype(m_vm, -1);
	if (type != OT_CLOSURE && type != OT_NATIVECLOSURE)
	{
		sq_pop(m_vm, 1);
		return NULL;
	}

	// Push 1st argument - function environment
	sq_pushroottable(m_vm);

	return new SquirrelScriptStack(this, true);
}

bool SquirrelScriptContext::RegisterFunction(FunctionDesc* desc)
{
	SquirrelFunctionInfo* info = new SquirrelFunctionInfo();
	info->m_desc = desc;
	info->m_context = this;

	sq_pushroottable(m_vm);
	sq_pushstring(m_vm, desc->m_name, -1);
	sq_pushuserpointer(m_vm, info);
	sq_newclosure(m_vm, SquirrelFunctionCallback, 1);
	sq_createslot(m_vm, -3);
	sq_pop(m_vm, 1);

	m_functions.push_back(info);
	return true;
}

bool SquirrelScriptContext::RegisterUserClass(ClassDesc* desc)
{
	MultiScriptAssert(desc->m_constructor);
	MultiScriptAssert(desc->m_destructor);

	SquirrelClassInfo* classInfo = new SquirrelClassInfo();
	classInfo->m_desc = desc;
	classInfo->m_context = this;

	// Create class
	sq_pushstring(m_vm, desc->m_name, -1);

	// TODO: Handle subclass via 2nd parameter
	sq_newclass(m_vm, SQFalse);

	// Add methods
	for (unsigned int i = 0; i < desc->m_methods.size(); ++i)
	{
		sq_pushstring(m_vm, desc->m_methods[i].m_name, -1);
		sq_pushuserpointer(m_vm, classInfo);
		sq_pushinteger(m_vm, i);
		sq_newclosure(m_vm, SquirrelClassMethodCallback, 2);
		sq_createslot(m_vm, -3);
	}

	// Add constructor
	sq_pushstring(m_vm, "constructor", -1);
	sq_pushuserpointer(m_vm, classInfo);
	sq_newclosure(m_vm, SquirrelClassConstructorCallback, 1);
	sq_createslot(m_vm, -3);

	// Add to-string method
	sq_pushstring(m_vm, "AsString", -1);
	sq_pushuserpointer(m_vm, classInfo);
	sq_newclosure(m_vm, SquirrelClassToStringMethodCallback, 1);
	sq_createslot(m_vm, -3);

	// Add optional destructor
	if (!desc->m_garbageCollect)
	{
		// Add destructor
		sq_pushstring(m_vm, "Destroy", -1);
		sq_pushuserpointer(m_vm, classInfo);
		sq_newclosure(m_vm, SquirrelClassDestructorCallback, 1);
		sq_createslot(m_vm, -3);
	}
	else
	{
		// TODO: Add garbage collection callback here
	}

	// Add class to root table
	sq_createslot(m_vm, -3);

	// Add class info
	m_classes.push_back(classInfo);
	return true;
}

SquirrelClassInfo* SquirrelScriptContext::FindClassInfo(ClassDesc* classDesc)
{
	for (unsigned int i = 0; i < m_classes.size(); ++i)
		if (m_classes[i]->m_desc == classDesc)
			return m_classes[i];
	return NULL;
}

void SquirrelScriptContext::SquirrelPrintCallback(HSQUIRRELVM vm, const SQChar* text, ...)
{
	SquirrelScriptContext* context = (SquirrelScriptContext*) vm->user_data;
	if (!context->m_logger)
		return;

	char buffer[1024];

	va_list vl;
	va_start(vl, text);
	vsprintf_s(buffer, 1024, text, vl);
	va_end(vl);

	context->m_logger->Output(buffer);
}

SQInteger SquirrelScriptContext::SquirrelReadCallback(SQUserPointer userdata)
{
	SquirrelScriptContext* context = (SquirrelScriptContext*) userdata;

	const char currentChar = *context->m_currentStringProgram;
	if (currentChar)
		context->m_currentStringProgram++;
	else
		context->m_currentStringProgram = NULL;
	return currentChar;
}

int SquirrelScriptContext::SquirrelClassConstructorCallback(HSQUIRRELVM vm)
{
	MultiScriptAssert( sq_gettype(vm, -1) == OT_USERPOINTER );

	// Get class info
	void* userPtr = NULL;
	sq_getuserpointer(vm, -1, &userPtr);
	sq_pop(vm, 1);

	SquirrelClassInfo* classInfo = (SquirrelClassInfo*) userPtr;

	// Construct object
	SquirrelScriptObject* scriptObject = new SquirrelScriptObject();
	scriptObject->m_classInfo = classInfo;

	SquirrelScriptStack stack(classInfo->m_context, false);
	classInfo->m_desc->m_constructor(&stack, scriptObject);
	MultiScriptAssert( scriptObject->m_objectPtr );
	MultiScriptAssert( stack.m_numPushed == 0 );

	// There is already an instance on the stack (pushed automatically when constructor was invoked from script)
	// Attach our object to created instance
	sq_setinstanceup(vm, -1, scriptObject);

	// Set garbage collection callback for this instance
	sq_setreleasehook(vm, -1, SquirrelClassGCCallback);

	return 0;
}

int SquirrelScriptContext::SquirrelClassDestructorCallback(HSQUIRRELVM vm)
{
	// Get class info
	MultiScriptAssert( sq_gettype(vm, -1) == OT_USERPOINTER );
	void* userPtr = NULL;
	sq_getuserpointer(vm, -1, &userPtr);
	sq_pop(vm, 1);

	SquirrelClassInfo* classInfo = (SquirrelClassInfo*) userPtr;

	// Get the instance
	MultiScriptAssert( sq_gettype(vm, 1) == OT_INSTANCE );
	sq_getinstanceup(vm, 1, &userPtr, 0);
	SquirrelScriptObject* scriptObject = (SquirrelScriptObject*) userPtr;
	MultiScriptAssert(scriptObject->m_objectPtr);

	// Invoke user supplied destructor
	scriptObject->m_lockedByScript = true;
	classInfo->m_desc->m_destructor(scriptObject);
	scriptObject->m_lockedByScript = false;
	scriptObject->m_objectPtr = NULL;

	return 0;
}

int SquirrelScriptContext::SquirrelClassGCCallback(SQUserPointer userPtr, SQInteger size)
{
	SquirrelScriptObject* scriptObject = (SquirrelScriptObject*) userPtr;

	if (!scriptObject->m_objectPtr)
	{
		delete scriptObject;
		return 0;
	}

	if (scriptObject->m_classInfo->m_desc->m_garbageCollect)
	{
		scriptObject->m_lockedByScript = true;
		scriptObject->m_classInfo->m_desc->m_destructor(scriptObject);
	}

	delete scriptObject;
	return 0;
}

int SquirrelScriptContext::SquirrelClassToStringMethodCallback(HSQUIRRELVM vm)
{
	// Get class info
	MultiScriptAssert( sq_gettype(vm, -1) == OT_USERPOINTER );
	void* userPtr = NULL;
	sq_getuserpointer(vm, -1, &userPtr);
	sq_pop(vm, 1);

	SquirrelClassInfo* classInfo = (SquirrelClassInfo*) userPtr;

	// Get the instance
	MultiScriptAssert( sq_gettype(vm, 1) == OT_INSTANCE );
	sq_getinstanceup(vm, 1, &userPtr, 0);
	SquirrelScriptObject* scriptObject = (SquirrelScriptObject*) userPtr;
	MultiScriptAssert(scriptObject->m_objectPtr);

	// Invoke to-string method
	char buffer[1 << 8];
	if (classInfo->m_desc->m_toStringMethod)
	{
		SquirrelScriptStack stack(classInfo->m_context, false);
		const bool result = classInfo->m_desc->m_toStringMethod(scriptObject, buffer, 1 << 8);
		MultiScriptAssert( result );
		MultiScriptAssert( stack.m_numPushed == 0 );
	}
	else
		MultiScriptSprintf(buffer, 1 << 8, "[%s]", classInfo->m_desc->m_name);

	sq_pushstring(vm, buffer, -1);
	return 1;
}

int SquirrelScriptContext::SquirrelClassMethodCallback(HSQUIRRELVM vm)
{
	// Get class info
	MultiScriptAssert( sq_gettype(vm, -1) == OT_USERPOINTER );
	void* userPtr = NULL;
	sq_getuserpointer(vm, -1, &userPtr);
	sq_pop(vm, 1);

	SquirrelClassInfo* classInfo = (SquirrelClassInfo*) userPtr;

	// Get method index
	MultiScriptAssert( sq_gettype(vm, -1) == OT_INTEGER );

	int methodIndex;
	sq_getinteger(vm, -1, &methodIndex);
	sq_pop(vm, 1);

	// Get the instance
	MultiScriptAssert( sq_gettype(vm, 1) == OT_INSTANCE );
	sq_getinstanceup(vm, 1, &userPtr, 0);
	SquirrelScriptObject* scriptObject = (SquirrelScriptObject*) userPtr;
	MultiScriptAssert(scriptObject->m_objectPtr);

	SquirrelScriptStack stack(classInfo->m_context, false);
	const bool result = classInfo->m_desc->m_methods[methodIndex].m_method(scriptObject, &stack);
	MultiScriptAssert( result );

	return stack.m_numPushed;
}

int SquirrelScriptContext::SquirrelFunctionCallback(HSQUIRRELVM vm)
{
	MultiScriptAssert( sq_gettype(vm, -1) == OT_USERPOINTER );

	void* userPtr = NULL;
	sq_getuserpointer(vm, -1, &userPtr);
	sq_pop(vm, 1);

	SquirrelFunctionInfo* info = (SquirrelFunctionInfo*) userPtr;

	SquirrelScriptStack stack(info->m_context, false);
	const bool result = info->m_desc->m_function(&stack);
	MultiScriptAssert( result );

	return stack.m_numPushed;
}


ScriptContext* CreateSquirrelScriptContext(int scriptStack)
{
	return SquirrelScriptContext::CreateContext(scriptStack);
}