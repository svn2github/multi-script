#include "ScriptInterface.h"

#include "gmMachine.h"
#include "gmCall.h"

#include "GMScriptContext.h"
#include "GMScriptStack.h"

struct GMClassInfo;

//-----------------------------------------------------------

GMScriptContext::GMScriptContext() :
	m_machine(NULL)
{}

GMScriptContext::~GMScriptContext()
{
	m_machine->CollectGarbage(true);
	delete m_machine;
}

GMScriptContext* GMScriptContext::CreateContext(int stackSize)
{
	gmMachine::s_printCallback = GMPrintCallback;

	GMScriptContext* context = new GMScriptContext();
	context->m_machine = new gmMachine();
	context->m_machine->SetUserData(context);

	return context;
}

void GMScriptContext::SetLogger(ScriptLogger* logger)
{
	m_logger = logger;
}

void GMScriptContext::CollectGarbage(bool full)
{
	m_machine->CollectGarbage(full);
}

bool GMScriptContext::ExecuteString(const char* string)
{
	const int errors = m_machine->ExecuteString(string);
	if (errors)
	{
		bool first = true;
		const char* message;
		while (message = m_machine->GetLog().GetEntry(first))
		{
			char buffer[1024];
			MultiScriptSprintf(buffer, 1024, "%s\n", message);
			m_logger->Output(buffer);
		}
		m_machine->GetLog().Reset();
		return false;
	}

	return true;
}

ScriptStack* GMScriptContext::BeginCall(FunctionDesc* desc)
{
	return BeginCall(desc->m_name);
}

ScriptStack* GMScriptContext::BeginCall(const char* name)
{
	gmCall* call = new gmCall();
	if (call->BeginGlobalFunction(m_machine, name))
		return new GMScriptStack(NULL, call);

	delete call;
	return NULL;
}

bool GMScriptContext::RegisterFunction(FunctionDesc* desc)
{
	GMFunctionInfo* info = new GMFunctionInfo();
	info->m_desc = desc;
	info->m_context = this;

	m_machine->RegisterLibraryFunction(desc->m_name, GMFunctionCallback, NULL, info);

	m_functions.push_back(info);
	return true;
}

bool GMScriptContext::RegisterUserClass(ClassDesc* desc)
{
	MultiScriptAssert(desc->m_constructor);
	MultiScriptAssert(desc->m_destructor);

	// Create class info
	GMClassInfo* classInfo = new GMClassInfo();
	classInfo->m_desc = desc;
	classInfo->m_context = this;
	classInfo->m_gmTypeId = m_machine->CreateUserType(desc->m_name);

	// Create info for each method
	for (unsigned int i = 0; i < desc->m_methods.size(); ++i)
	{
		GMMethodInfo methodInfo;
		methodInfo.m_classInfo = classInfo;
		methodInfo.m_methodIndex = i;
		classInfo->m_methodInfos.push_back(methodInfo);
	}

	// Create an array of methods
	for (unsigned int i = 0; i < desc->m_methods.size(); ++i)
	{
		gmFunctionEntry functionEntry;
		functionEntry.m_name = desc->m_methods[i].m_name;
		functionEntry.m_function = GMClassMethodCallback;
		functionEntry.m_userData = &classInfo->m_methodInfos[i];
		classInfo->m_gmMethods.push_back(functionEntry);
	}

	gmFunctionEntry toStringMethod;
	toStringMethod.m_name = "AsString";
	toStringMethod.m_userData = classInfo;
	toStringMethod.m_function = GMClassToStringMethodCallback;
	classInfo->m_gmMethods.push_back(toStringMethod);

	if (!desc->m_garbageCollect)
	{
		gmFunctionEntry toStringMethod;
		toStringMethod.m_name = "Destroy";
		toStringMethod.m_userData = classInfo;
		toStringMethod.m_function = GMClassDestructorCallback;
		classInfo->m_gmMethods.push_back(toStringMethod);
	}

	// Register function to create objects of this class
	m_machine->RegisterLibraryFunction(desc->m_name, GMClassConstructorCallback, NULL, classInfo);

	// Register methods
	m_machine->RegisterTypeLibrary(
		classInfo->m_gmTypeId,
		&classInfo->m_gmMethods[0],
		(int) classInfo->m_gmMethods.size());

	// Register GC callbacks and to-string method
	m_machine->RegisterUserCallbacks(
		classInfo->m_gmTypeId,
		GMClassTraceCallback,
		GMClassGCCallback,
		GMClassToStringCallback);

	// Add class info
	m_classes.push_back(classInfo);
	return true;
}

GMClassInfo* GMScriptContext::FindClassInfo(ClassDesc* classDesc)
{
	for (unsigned int i = 0; i < m_classes.size(); ++i)
		if (m_classes[i]->m_desc == classDesc)
			return m_classes[i];
	return NULL;
}

void GMScriptContext::GMPrintCallback(gmMachine* machine, const char* string)
{
	GMScriptContext* context = (GMScriptContext*) machine->GetUserData();
	if (context->m_logger)
		context->m_logger->Output(string);
}

bool GMScriptContext::GMClassTraceCallback(gmMachine* machine, gmUserObject* object, gmGarbageCollector* gc, const int workLeftToGo, int& workDone)
{
	// TODO: Is this fine?
	workDone++;
	return true;
}

int GMScriptContext::GMClassConstructorCallback(gmThread* thread)
{
	GMClassInfo* classInfo = (GMClassInfo*) thread->GetFunctionObject()->m_cUserData;

	GMScriptObject* scriptObject = new GMScriptObject();
	scriptObject->m_classInfo = classInfo;
	scriptObject->m_gmTable = thread->GetMachine()->AllocTableObject();
	scriptObject->m_gmUserObject = thread->PushNewUser(scriptObject, classInfo->m_gmTypeId);

	GMScriptStack stack(thread);
	classInfo->m_desc->m_constructor(&stack, scriptObject);
	MultiScriptAssert( scriptObject->m_objectPtr );

	return GM_OK;
}

int GMScriptContext::GMClassDestructorCallback(gmThread* thread)
{
	GMScriptObject* scriptObject = (GMScriptObject*) thread->ThisUser_NoChecks();
	MultiScriptAssert(scriptObject);

	scriptObject->m_lockedByScript = true;
	scriptObject->m_classInfo->m_desc->m_destructor(scriptObject);
	scriptObject->m_lockedByScript = false;
	scriptObject->m_objectPtr = NULL;

	return GM_OK;
}

void GMScriptContext::GMClassGCCallback(gmMachine* machine, gmUserObject* object)
{
	GMScriptObject* scriptObject = (GMScriptObject*) object->m_user;
	MultiScriptAssert(scriptObject);

	if (!scriptObject->m_objectPtr)
	{
		delete scriptObject;
		return;
	}

	if (scriptObject->m_classInfo->m_desc->m_garbageCollect)
	{
		scriptObject->m_lockedByScript = true;
		scriptObject->m_classInfo->m_desc->m_destructor(scriptObject);
	}

	delete scriptObject;
}

int GMScriptContext::GMClassToStringMethodCallback(gmThread* thread)
{
	GMScriptObject* scriptObject = (GMScriptObject*) thread->ThisUser_NoChecks();
	MultiScriptAssert( scriptObject->m_objectPtr );

	GMClassInfo* classInfo = scriptObject->m_classInfo;

	char buffer[1 << 8];
	if (classInfo->m_desc->m_toStringMethod)
	{
		GMScriptStack stack(thread);
		const bool result = classInfo->m_desc->m_toStringMethod(scriptObject, buffer, 1 << 8);
		MultiScriptAssert( result );
	}
	else
		MultiScriptSprintf(buffer, 1 << 8, "[%s]", classInfo->m_desc->m_name);

	thread->PushNewString(buffer, 1 << 8);

	return GM_OK;
}

void GMScriptContext::GMClassToStringCallback(gmUserObject* object, char* buffer, int bufferSize)
{
	GMScriptObject* scriptObject = (GMScriptObject*) object->m_user;
	MultiScriptAssert( scriptObject->m_objectPtr );

	GMClassInfo* classInfo = scriptObject->m_classInfo;

	if (classInfo->m_desc->m_toStringMethod)
	{
		const bool result = classInfo->m_desc->m_toStringMethod(scriptObject, buffer, bufferSize);
		MultiScriptAssert( result );
	}
	else
		MultiScriptSprintf(buffer, 1 << 8, "[%s]", classInfo->m_desc->m_name);
}

int GMScriptContext::GMClassMethodCallback(gmThread* thread)
{
	GMMethodInfo* methodInfo = (GMMethodInfo*) thread->GetFunctionObject()->m_cUserData;

	GMScriptObject* scriptObject = (GMScriptObject*) thread->ThisUser_NoChecks();
	MultiScriptAssert( scriptObject->m_objectPtr );

	GMScriptStack stack(thread);
	const bool result = methodInfo->m_classInfo->m_desc->m_methods[methodInfo->m_methodIndex].m_method(scriptObject, &stack);
	MultiScriptAssert( result );

	return GM_OK;
}

int GMScriptContext::GMFunctionCallback(gmThread* thread)
{
	GMFunctionInfo* info = (GMFunctionInfo*) thread->GetFunctionObject()->m_cUserData;

	GMScriptStack stack(thread);
	const bool result = info->m_desc->m_function(&stack);
	MultiScriptAssert( result );

	return GM_OK;
}

ScriptContext* CreateGMScriptContext(int scriptStack)
{
	return GMScriptContext::CreateContext(scriptStack);
}