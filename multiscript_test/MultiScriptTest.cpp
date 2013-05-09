#include "ScriptInterface.h"

#include "windows.h"

//---------------------------------------------------------
// Sample classes
//---------------------------------------------------------

/**
 *	Sample garbage collected class
 */
class SampleClass
{
public:
	int m_value; //!< Just some value

	SampleClass(int value = 17) :
		m_value(value)
	{
	}

	~SampleClass()
	{
		MultiScriptPrintf("CPP: destroyed SampleClass\n");
	}

	static void Generic_Constructor(ScriptStack* stack, ScriptObject* scriptObject)
	{
		SampleClass* object = new SampleClass();
		stack->PopInt(object->m_value); // Optional parameter; may fail

		scriptObject->m_objectPtr = object;
	}

	static void Generic_Destructor(ScriptObject* scriptObject)
	{
		MultiScriptPrintf("CPP: destructor\n");
		if (!scriptObject->m_objectPtr)
		{
			MultiScriptPrintf("CPP (destructor): object already destroyed.\n");
			return;
		}

		SampleClass* object = (SampleClass*) scriptObject->m_objectPtr;
		delete object;
	}

	static bool Generic_ToString(ScriptObject* scriptObject, char* buffer, int bufferSize)
	{
		SampleClass* object = (SampleClass*) scriptObject->m_objectPtr;
		MultiScriptSprintf(buffer, bufferSize, "[SampleClass{%d}]", object->m_value);
		return true;
	}

	static bool Generic_SetInt(ScriptObject* scriptObject, ScriptStack* stack)
	{
		SampleClass* object = (SampleClass*) scriptObject->m_objectPtr;
		return stack->PopInt(object->m_value);
	}

	static bool Generic_GetInt(ScriptObject* scriptObject, ScriptStack* stack)
	{
		SampleClass* object = (SampleClass*) scriptObject->m_objectPtr;
		stack->PushInt(object->m_value);
		return true;
	}

	static ClassDesc* GetClassDesc_Static()
	{
		static ClassDesc classDesc;

		static bool isInitialized = false;
		if (!isInitialized)
		{
			isInitialized = true;

			classDesc.m_garbageCollect = true;
			classDesc.m_name = "SampleClass";
			classDesc.m_constructor = SampleClass::Generic_Constructor;
			classDesc.m_destructor = SampleClass::Generic_Destructor;
			classDesc.m_toStringMethod = SampleClass::Generic_ToString;
			classDesc.m_methods.push_back( ClassMethodDesc("SetInt", SampleClass::Generic_SetInt) );
			classDesc.m_methods.push_back( ClassMethodDesc("GetInt", SampleClass::Generic_GetInt) );
		}

		return &classDesc;
	}
};

/**
 *	Sample base class that can be used as a game entity - used from both C++ and script side.
 *	It's main purpose is to hold ScriptObject inside so it can pass itself to scripts via PushScriptObject() method.
 *
 *	Note: It would be possible not to embed ScriptObject inside this class, but then every time it was passed to the script
 *	new Script Object would be created which is somewhat less efficient than storing single reference to the script object
 *	throughout whole object's life time.
 */
class BaseScriptableClass
{
private:
	ScriptObjectPtr m_scriptObject; //!< Pointer to a script object; hidden from user

protected:
	void SetScriptObject(ScriptObject* scriptObject)
	{
		m_scriptObject = scriptObject;
	}

public:
	//! Retrieves class description
	virtual ClassDesc* GetClassDesc() = 0;

	//! Pushes itself onto given stack
	bool PushScriptObject(ScriptStack* stack)
	{
		if (!m_scriptObject)
		{
			m_scriptObject = stack->PushNewScriptObject(GetClassDesc(), this);
			return m_scriptObject != NULL;
		}

		return stack->PushScriptObject(m_scriptObject);
	}
};

/**
 *	Sample non-garbage collected class that can pass itself to the script.
 */
class NewSampleClass : public BaseScriptableClass
{
public:
	int m_value; //!< Just some value
	int m_secondValue; //!< Just some another value

	NewSampleClass(int secondValue = 175, int value = 17) :
		m_value(value),
		m_secondValue(secondValue)
	{
	}

	~NewSampleClass()
	{
		MultiScriptPrintf("CPP: destroyed NewSampleClass\n");
	}

	ClassDesc* GetClassDesc()
	{
		return NewSampleClass::GetClassDesc_Static();
	}

	static void Generic_Constructor(ScriptStack* stack, ScriptObject* scriptObject)
	{
		NewSampleClass* object = new NewSampleClass();
		object->SetScriptObject(scriptObject);

		stack->PopInt(object->m_secondValue); // Optional parameter; may fail
		stack->PopInt(object->m_value); // Optional parameter; may fail

		scriptObject->m_objectPtr = object;
	}

	static void Generic_Destructor(ScriptObject* scriptObject)
	{
		MultiScriptPrintf("CPP: destructor\n");
		if (!scriptObject->m_objectPtr)
		{
			MultiScriptPrintf("CPP (destructor): object already destroyed.\n");
			return;
		}
		NewSampleClass* object = (NewSampleClass*) scriptObject->m_objectPtr;
		delete object;
	}

	static bool Generic_ToString(ScriptObject* scriptObject, char* buffer, int bufferSize)
	{
		NewSampleClass* object = (NewSampleClass*) scriptObject->m_objectPtr;
		MultiScriptSprintf(buffer, bufferSize, "[NewSampleClass{%d, %d}]", object->m_value, object->m_secondValue);
		return true;
	}

	static bool Generic_SetSecondInt(ScriptObject* scriptObject, ScriptStack* stack)
	{
		NewSampleClass* object = (NewSampleClass*) scriptObject->m_objectPtr;
		return stack->PopInt(object->m_secondValue);
	}

	static bool Generic_GetSecondInt(ScriptObject* scriptObject, ScriptStack* stack)
	{
		NewSampleClass* object = (NewSampleClass*) scriptObject->m_objectPtr;
		stack->PushInt(object->m_secondValue);
		return true;
	}

	static ClassDesc* GetClassDesc_Static()
	{
		static ClassDesc classDesc;

		static bool isInitialized = false;
		if (!isInitialized)
		{
			isInitialized = true;

			classDesc.m_garbageCollect = false;
			classDesc.m_name = "NewSampleClass";
			classDesc.m_constructor = NewSampleClass::Generic_Constructor;
			classDesc.m_destructor = NewSampleClass::Generic_Destructor;
			classDesc.m_toStringMethod = NewSampleClass::Generic_ToString;
			classDesc.m_methods.push_back( ClassMethodDesc("SetSecondInt", NewSampleClass::Generic_SetSecondInt) );
			classDesc.m_methods.push_back( ClassMethodDesc("GetSecondInt", NewSampleClass::Generic_GetSecondInt) );
		}
		
		return &classDesc;
	}
};

//---------------------------------------------------------
// Functions registered in script
//---------------------------------------------------------

static bool PrintInt(ScriptStack* stack)
{
	int value;
	if (!stack->PopInt(value)) return false;

	MultiScriptPrintf("CPP: value = %d\n", value);

	return true;
}

static bool MyAdd(ScriptStack* stack)
{
	int a, b;
	if (!stack->PopInt(a)) return false;
	if (!stack->PopInt(b)) return false;
	if (!stack->PushInt(a + b)) return false;
	return true;
}

static bool ReplaceCPPObject(ScriptStack* stack)
{
	// Pop an object to replace
	ScriptObject* oldScriptObject = stack->PopScriptObject();
	if (!oldScriptObject) return false;

	NewSampleClass* oldObject = (NewSampleClass*) oldScriptObject->m_objectPtr;

	// Create new object
	NewSampleClass* newObject = new NewSampleClass(oldObject->m_secondValue + 1, oldObject->m_value + 1);

	// Return the object
	if (!newObject->PushScriptObject(stack)) return false;

	return true;
}

//---------------------------------------------------------
// Script call example
//---------------------------------------------------------

bool ExecuteScriptFunction_ScriptAdd(ScriptContext* context, bool checkResult = true)
{
	ScriptCallPtr call = context->BeginCall("script_add");
	if (call)
	{
		int a = 17;
		int b = 14;
		if (!call->PushInt(a)) return false;
		if (!call->PushInt(b)) return false;
		if (!call->EndCall()) return false;

		int result = 0;
		if (!call->PopInt(result)) return false;

		if (checkResult)
		{
			//MultiScriptAssert( result == a + b );
			MultiScriptPrintf("script_add: Result OK\n");
		}
		return true;
	}

	return false;
}

//---------------------------------------------------------
// Test scripts - "same" for each supported language (as much as syntax allows)
//---------------------------------------------------------

struct ScriptText
{
	const char* m_language;
	const char* m_script;
};

const ScriptText scripts[] =
{

	// Test Program 0
	{"lua",			"print('Hello from lua\\n')"},

	{"gm",			"print(\"Hello from game monkey\\n\");"},

	{"squirrel",	"print(\"Hello from squirrel\\n\");"},

	{"ocaml",		"Printf.printf \"Hello from ocaml\\n\";;"},

	// Test Program 1
	{"lua",			"PrintInt( MyAdd(3, 4) )"},

	{"gm",			"PrintInt( MyAdd(3, 4) );"},

	{"squirrel",	"PrintInt( MyAdd(3, 4) )"},

		// Note 1: In Ocaml we need to explicitly tell the ocamlc compiler which functions are external
		// Note 2: External names can't start with big letters; this is why I changed Ocaml's internal naming convention; from C++ point of view these are still functions of the original name
	{"ocaml",		"external my_print_int : int -> unit = \"PrintInt\"\n"
					"external my_add : int -> int -> int = \"MyAdd\";;\n"
					"my_print_int (my_add 3 4);;"}, 

	// Test Program 2 (no Ocaml coz Ocaml doesn't support binding of the classes)
	{"lua",			"function SampleClass:Show()\n"
					"	print('passed value = ', self:GetInt(), '\\n')\n"
					"end\n"
					""
					"a = SampleClass(100)\n"
					"a:Show()\n"
					"print( 'a = ', a, '\\n' )\n"
					"a:SetInt( MyAdd(MyAdd(4, 5), 6) )\n"
					""
					"print('a = ', a:GetInt(), '\\n')\n"
					""
					"b = NewSampleClass(2, 5)\n"
					"print('b = ', b, '\\n')\n"
					"b:Destroy()"
					},

	{"gm",			"Show = function()\n"
					"{\n"
					"	print(\"passed value = \" + .GetInt() + \"\\n\");\n"
					"};\n"
					""
					"a = SampleClass(100);\n"
					"a:Show();\n"
					"print( \"a = \" + a.AsString()); print(\"\\n\");\n"
					"a.SetInt( MyAdd(MyAdd(4, 5), 6) );\n"
					""
					"print(\"a = \" + a.GetInt() + \"\\n\");"
					""
					"b = NewSampleClass(2, 5);\n"
					"print(\"b = \" + b.AsString()); print(\"\\n\");\n"
					"b.Destroy();"
					},

	{"squirrel",	"function SampleClass::Show()\n"
					"{\n"
					"	print(\"passed value = \" + GetInt() + \"\\n\");\n"
					"}\n"
					""
					"local a = SampleClass(100);\n"
					"a.Show();\n"
					"print( \"a = \" + a.AsString() + \"\\n\");\n"
					"a.SetInt( MyAdd(MyAdd(4, 5), 6) );\n"
					""
					"print(\"a = \" + a.GetInt() + \"\\n\");"
					""
					"local b = NewSampleClass(2, 5);\n"
					"print(\"b = \" + b.AsString() + \"\\n\");\n"
					"b.Destroy();"
					},

	// Test Program 3 (no Ocaml coz Ocaml doesn't support binding of the classes)
	{"lua",			"y = NewSampleClass(44, 55)\n"
					"print('y = ', y, '\\n' )\n"
					"x = ReplaceCPPObject(y)\n"
					"print('x = ', x, '\\n' )\n"
					"x:Destroy()\n"
					"y:Destroy()"},

	{"gm",			"y = NewSampleClass(44, 55);\n"
					"print(\"y = \" + y.AsString()); print(\"\\n\");\n"
					"x = ReplaceCPPObject(y);\n"
					"print(\"x = \" + x.AsString()); print(\"\\n\");\n"
					"x.Destroy();\n"
					"y.Destroy();"},

	{"squirrel",	"local y = NewSampleClass(44, 55);\n"
					"print(\"y = \" + y.AsString() + \"\\n\");\n"
					"local x = ReplaceCPPObject(y);\n"
					"print(\"x = \" + x.AsString() + \"\\n\");\n"
					"x.Destroy();\n"
					"y.Destroy();"},

	{NULL, NULL}
};

//! A 'script_add' function that adds two numbers in each scripting language
const ScriptText script_function[] =
{
	{"lua",			"function script_add(a, b) return a + b end"},
	{"gm",			"global script_add = function(a, b) { return a + b; };"},
	{"squirrel",	"function script_add(a, b) { return a + b };"},
		// Note: In Ocaml we have to explicitly say which functions should be visible from C side; we can do it via Callback.register function
	{"ocaml",		"let script_add a b = a + b\n"
					"let _ = Callback.register \"script_add\" script_add"},
	{NULL, NULL}
};

//---------------------------------------------------------
// Logger registered to the script context
//---------------------------------------------------------

class MyScriptLogger : public ScriptLogger
{
public:
	void Output(const char* text, ...)
	{
		char buffer[1024];

		va_list vl;
		va_start(vl, text);
#ifdef WIN32
		vsprintf_s(buffer, 1024, text, vl);
#else
		vsprintf(buffer, text, vl);
#endif
		va_end(vl);

		MultiScriptPrintf(buffer);
	}
};

//---------------------------------------------------------
// Test
//---------------------------------------------------------

void main()
{
	ScriptLogger* logger = new MyScriptLogger();

	// Create function descriptions
	vector<FunctionDesc*> funcs;
	funcs.push_back( new FunctionDesc("PrintInt", PrintInt, 1) );
	FunctionDesc* myAddFunction = NULL;
	funcs.push_back( myAddFunction = new FunctionDesc("MyAdd", MyAdd, 2) );
	funcs.push_back( new FunctionDesc("ReplaceCPPObject", ReplaceCPPObject, 1) );

	// Create classes description
	vector<ClassDesc*> classes;
	classes.push_back( SampleClass::GetClassDesc_Static() );
	classes.push_back( NewSampleClass::GetClassDesc_Static() );

	// ---------------------------------------------------------------
	// Execute all test scripts for all supported languages
	// ---------------------------------------------------------------
	for (int i = 0; scripts[i].m_language; ++i)
	{
		// Create context for given language
		ScriptContext* context = ScriptContext::Create(scripts[i].m_language);
		if (!context)
			continue;

		// Some info
		MultiScriptPrintf(
			"====================================\n"
			"Executing script in '%s' language:\n", scripts[i].m_language);

		// Set up context
		context->SetLogger(logger);

		// Register functions and classes
		for (unsigned int j = 0; j < funcs.size(); j++)
			context->RegisterFunction( funcs[j] );
		for (unsigned int j = 0; j < classes.size(); j++)
			context->RegisterUserClass( classes[j] );

		// Execute script
		const bool result = context->ExecuteString( scripts[i].m_script );
		if (!result)
			MultiScriptPrintf("FAILED\n");

		// Destroy context
		context->CollectGarbage(true);
		delete context;
	}

	// ---------------------------------------------------------------
	// Execute MyAdd script function for all languages
	// ---------------------------------------------------------------
	for (const char** language = ScriptContext::GetSupportedLanguages(); *language; ++language)
	{
		// Create context for given language
		ScriptContext* context = ScriptContext::Create(*language);
		if (!context)
			continue;

		// Some info
		MultiScriptPrintf(
			"====================================\n"
			"Executing script function in '%s' language:\n", *language);

		// Set up context
		context->SetLogger(logger);

		// Register functions and classes
		for (unsigned int j = 0; j < funcs.size(); j++)
			context->RegisterFunction( funcs[j] );
		for (unsigned int j = 0; j < classes.size(); j++)
			context->RegisterUserClass( classes[j] );

		// Register/create script function to be used afterwards
		const ScriptText* scriptFunction = NULL;
		for (int i = 0; script_function[i].m_language; ++i)
			if (!strcmp(script_function[i].m_language, *language))
			{
				scriptFunction = &script_function[i];
				break;
			}
		MultiScriptAssert(scriptFunction);
		const bool result = context->ExecuteString(scriptFunction->m_script);
		if (!result)
			MultiScriptPrintf("FAILED\n");

		// Execute script function
		ExecuteScriptFunction_ScriptAdd(context);

		// Destroy context
		delete context;
	}
}