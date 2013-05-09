#include "ScriptInterface.h"

#include <windows.h>
#include <process.h>

#include "OcamlScriptStack.h"
#include "OcamlScriptContext.h"

OcamlScriptContext* OcamlScriptContext::s_instance = NULL;

OcamlScriptObject::OcamlScriptObject() :
	m_lockedByScript(false)
{
}

void OcamlScriptObject::Release()
{
	if (!m_lockedByScript)
		delete this;
}

OcamlScriptContext::OcamlScriptContext() :
	m_dirtyFakedDLL(true)
{}

OcamlScriptContext::~OcamlScriptContext()
{
}

OcamlScriptContext* OcamlScriptContext::CreateContext(int stackSize)
{
	if (!custom_caml_init())
		return NULL;

	OcamlScriptContext* context = new OcamlScriptContext();

	custom_ocaml_set_stdout_func(OcamlPrintCallback);
	custom_ocaml_set_fatal_error_func(OcamlFatalErrorCallback);

	s_instance = context;
	return context;
}

void OcamlScriptContext::SetLogger(ScriptLogger* logger)
{
	m_logger = logger;
}

void OcamlScriptContext::CollectGarbage(bool full)
{
	
}

static string m_ocamlc_path = "C:/Program Files/Objective Caml/bin/ocamlc.exe";
static string m_ocamlc_param_path = "\"C:/Program Files/Objective Caml/bin/ocamlc.exe\"";
static string m_tempSourceFile = "temp_ocaml.ml";
static string m_tempBytecodeFile = "temp_ocaml.bytecode";
static string m_fakeDLLName = "FakeOcamlDLL.dll";

bool OcamlScriptContext::ExecuteString(const char* string)
{
	// Rebuild faked DLL containing registered functions
	if (m_dirtyFakedDLL)
	{
		RebuildFakeDLL();

		FILE* dllFile = fopen(m_fakeDLLName.c_str(), "rb");
		if (!dllFile)
		{
			if (m_logger)
				m_logger->Output("ERROR: Faked Ocaml DLL is not built while it's necessary to run the ocaml scripts. Build FakeOcamlDLL and run this app again (sorry).\n");
			return false;
		}
		fclose(dllFile);
	}

	// Save the script in some temp file
	FILE* f = fopen(m_tempSourceFile.c_str(), "w");
	if (!f) return false;
	fprintf(f, string);
	fclose(f);

	// Invoke ocamlc to generate bytecode: *.ml + FakeOcamlDLL.dll -> *.bytecode
	// Assumes ocamlc.exe (Ocaml Compiler) to be present at certain location
	const int result = _spawnl(
		_P_WAIT,
		m_ocamlc_path.c_str(), m_ocamlc_param_path.c_str(),
		"-o", m_tempBytecodeFile.c_str(), // Compile & link and output result to given file
		m_tempSourceFile.c_str(), // ML source file
		"-dllib", m_fakeDLLName.c_str(), // Faked auto-generated DLL to link with; by supplying this faked DLL to ocaml compiler we make ocaml compiler happy in that it's able to find external functions
		NULL);
	if (result)
		return false;

	// Load bytecode file generated in previous step (*.bytecode)
	caml_code code;
	if (!custom_caml_load_code(m_tempBytecodeFile.c_str(), &code))
		return false;

	// Execute bytecode
	if (!custom_caml_run_code(&code))
		return false;
	
	return true;
}

ScriptStack* OcamlScriptContext::BeginCall(FunctionDesc* desc)
{
	return BeginCall(desc->m_name);
}

ScriptStack* OcamlScriptContext::BeginCall(const char* name)
{
	// Surely, this is as inefficient as it can be, but this is just a test...
	value* closure = caml_named_value(name);
	if (!closure)
		return NULL;

	return new OcamlScriptStack(this, closure);
}

bool OcamlScriptContext::RegisterFunction(FunctionDesc* desc)
{
	OcamlFunctionInfo* info = new OcamlFunctionInfo();
	info->m_desc = desc;
	info->m_context = this;

	int result;
	switch (desc->m_numParams)
	{
		case 0: result = custom_caml_register_c_function(desc->m_name, (c_primitive) OcamlFunctionCallback0Args, info); break;
		case 1: result = custom_caml_register_c_function(desc->m_name, (c_primitive) OcamlFunctionCallback1Arg, info); break;
		case 2: result = custom_caml_register_c_function(desc->m_name, (c_primitive) OcamlFunctionCallback2Args, info); break;
		default:
			return false;
	}
	MultiScriptAssert(result);

	m_functions.push_back(info);

	m_dirtyFakedDLL = true;
	return true;
}

void OcamlScriptContext::RebuildFakeDLL()
{
	// Rebuild FakeDll.h header file in the FakeDLL project (under Ocaml solution directory)
	FILE* f = fopen("../multiscript/FakeDLL.h", "w");
	MultiScriptAssert(f);

	fprintf(f,
		"// This is auto-generated file (by OcamlScriptContext)\n"
		"// Don't modify manually!\n");

	// Output all registered functions as DLL exported ones
	for (unsigned int i = 0; i < m_functions.size(); ++i)
		fprintf(f, "extern \"C\" __declspec(dllexport) int %s() { return 0; }\n", m_functions[i]->m_desc->m_name);

	fclose(f);

	// TODO: Recompile FakeDLL.dll using C++ compiler
	// NOTE: For now you have to manually recompile Ocaml->FakeDLL project after running this function

	m_dirtyFakedDLL = false;
}

bool OcamlScriptContext::RegisterUserClass(ClassDesc* desc)
{
	MultiScriptAssert(desc->m_constructor);
	MultiScriptAssert(desc->m_destructor);

	OcamlClassInfo* classInfo = new OcamlClassInfo();
	classInfo->m_desc = desc;
	classInfo->m_context = this;

	// Add class info
	m_classes.push_back(classInfo);
	return true;
}

OcamlClassInfo* OcamlScriptContext::FindClassInfo(ClassDesc* classDesc)
{
	for (unsigned int i = 0; i < m_classes.size(); ++i)
		if (m_classes[i]->m_desc == classDesc)
			return m_classes[i];
	return NULL;
}

void OcamlScriptContext::OcamlPrintCallback(int fd, char* p, int n)
{
	OcamlScriptContext* context = (OcamlScriptContext*) s_instance;
	if (!context->m_logger)
		return;

	char buffer[1 << 10];
	memcpy(buffer, p, n);
	buffer[n] = '\0';

	context->m_logger->Output(buffer);
}

void OcamlScriptContext::OcamlFatalErrorCallback(char* fmt, ...)
{
	OcamlScriptContext* context = (OcamlScriptContext*) s_instance;
	if (!context->m_logger)
		return;

	MultiScriptAssert(false)
}

int OcamlScriptContext::OcamlClassConstructorCallback()
{
	return 0;
}

int OcamlScriptContext::OcamlClassDestructorCallback()
{
	return 0;
}

int OcamlScriptContext::OcamlClassToStringMethodCallback()
{
	return 0;
}

int OcamlScriptContext::OcamlClassMethodCallback()
{
	return 0;
}

value OcamlScriptContext::OcamlFunctionCallback0Args()
{
	CAMLparam0();

	OcamlFunctionInfo* info = (OcamlFunctionInfo*) custom_caml_get_current_c_function_user_data();
	MultiScriptAssert(info->m_desc->m_numParams == 0);

	OcamlScriptStack stack(info->m_context, NULL);
	const bool result = info->m_desc->m_function(&stack);
	MultiScriptAssert(result);

	CAMLreturn(stack.m_result);
}

value OcamlScriptContext::OcamlFunctionCallback1Arg(value a0)
{
	CAMLparam1(a0);

	OcamlFunctionInfo* info = (OcamlFunctionInfo*) custom_caml_get_current_c_function_user_data();
	MultiScriptAssert(info->m_desc->m_numParams == 1);

	vector<value> values;
	values.push_back(a0);

	OcamlScriptStack stack(info->m_context, NULL, values);
	const bool result = info->m_desc->m_function(&stack);
	MultiScriptAssert(result);

	CAMLreturn(stack.m_result);
}

value OcamlScriptContext::OcamlFunctionCallback2Args(value a0, value a1)
{
	CAMLparam2(a0, a1);

	OcamlFunctionInfo* info = (OcamlFunctionInfo*) custom_caml_get_current_c_function_user_data();
	MultiScriptAssert(info->m_desc->m_numParams == 2);

	vector<value> values;
	values.push_back(a0);
	values.push_back(a1);

	OcamlScriptStack stack(info->m_context, NULL, values);
	const bool result = info->m_desc->m_function(&stack);
	MultiScriptAssert(result);

	CAMLreturn(stack.m_result);
}

ScriptContext* CreateOcamlScriptContext(int scriptStack)
{
	return OcamlScriptContext::CreateContext(scriptStack);
}