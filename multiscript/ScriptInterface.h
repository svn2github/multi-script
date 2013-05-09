#pragma once

//-----------------------------------------
// Some common utilities
//-----------------------------------------

#include <vector>
using namespace std;

#define MultiScriptAssert(condition) if (!(condition)) { printf("!!! ASSERTION !!!\nCondition: %s\n", #condition); while (true) {} }
void MultiScriptPrintf(const char* text, ...);
void MultiScriptSprintf(char* buffer, int bufferSize, const char* text, ...);

//-----------------------------------------
// MultiScript interfaces and function types
//-----------------------------------------

class ScriptStack;
class ScriptObject;

//! Generic function type; first pops parameters from the stack, then pushes results onto the stack
typedef bool (*GenericFunction)(ScriptStack* stack);

//! Function description
struct FunctionDesc
{
	const char* m_name; //!< Function name
	GenericFunction m_function; //!< The actual function
	int m_numParams; //!< Number of parameters this function takes

	FunctionDesc() :
		m_name(NULL),
		m_function(NULL)
	{}

	FunctionDesc(const char* name, GenericFunction function, int numParams) :
		m_name(name),
		m_function(function),
		m_numParams(numParams)
	{}
};

//! Class constructor; required to set scriptObject->m_objectPtr to non-NULL value
typedef void (*GenericClassConstructor)(ScriptStack* stack, ScriptObject* scriptObject);
//! Class destructor; should destroy object at scriptObject->m_objectPtr
typedef void (*GenericClassDestructor)(ScriptObject* scriptObject);
//! Should return object's as-string representation via passed buffer and its size; should returns true on success, false otherwise
typedef bool (*GenericClassToStringMethod)(ScriptObject* scriptObject, char* buffer, int bufferSize);
//! Class method invoked on object at scriptObject->m_objectPtr; the method should pop all parameters from the stack and push all results onto the stack
typedef bool (*GenericClassMethod)(ScriptObject* scriptObject, ScriptStack* stack);

//! Description of the class method
struct ClassMethodDesc
{
	const char* m_name; //!< Method name
	GenericClassMethod m_method; //!< The actual function

	ClassMethodDesc() :
		m_name(NULL),
		m_method(NULL)
	{}

	ClassMethodDesc(const char* name, GenericClassMethod method) :
		m_name(name),
		m_method(method)
	{}
};

//! Description of a class
struct ClassDesc
{
	bool m_garbageCollect; //!< Indicates whether the class is garbage collected or not; if yes, the destructor is called when garbage collector destroys it, otherwise the only way to destroy object from the script is to call special method Destroy() on it
	ClassDesc* m_superClass; //!< Optional super class 
	const char* m_name; //!< Name of the class
	GenericClassConstructor m_constructor; //!< Mandatory constructor; it's required that passed script object's internal pointer is set to non-NULL value
	GenericClassDestructor m_destructor; //!< Mandatory destructor (if garbage collected, invoked on garbage collection event; otherwise invoked only when manually called special method Destroy() on this from script); it's not required to NULL-ify the script object's internal pointer 
	GenericClassToStringMethod m_toStringMethod; //!< Optional to-string method; accessible from script as ToString() method
	vector<ClassMethodDesc> m_methods; //!< Class methods

	ClassDesc() :
		m_garbageCollect(true),
		m_superClass(NULL),
		m_name(NULL),
		m_constructor(NULL),
		m_destructor(NULL),
		m_toStringMethod(NULL)
	{}
};

/**
 *	Language independent script stack interface.
 *	Used in both callbacks to C/C++ functions registered in script and in calls made script functions.
 */
class ScriptStack
{
public:
	virtual ~ScriptStack() {}

	//! Retrieves number of available parameters to pop; within generic function indicates number of input parameters left to pop; after call to EndCall() indicates number of results left to pop
	virtual int GetNumParams() = 0;

	// Popping values from the stack
	virtual bool PopInt(int& value) = 0;
	virtual ScriptObject* PopScriptObject() = 0;

	// Pushing values to the stack
	virtual bool PushInt(int value) = 0;
	virtual bool PushString(const char* string, int length = -1) = 0;
	virtual bool PushScriptObject(ScriptObject* scriptObject) = 0;
	virtual ScriptObject* PushNewScriptObject(ClassDesc* classDesc, void* objectPtr) = 0;

	// Script call interface
	virtual bool EndCall() = 0;
	virtual void ReleaseAfterCall() = 0;
};

/**
 *	Language independent script object representation.
 *
 *	Note: Use ScriptObjectPtr instead of ScriptObject directly.
 */
class ScriptObject
{
public:
	void* m_objectPtr; //!< Pointer to the actual user's object (or any user data)

	ScriptObject() : m_objectPtr(NULL) {}
	virtual ~ScriptObject() {}
	//! Releases script object; use this when you remove C++ side object to notify script that the object doesn't exist
	virtual void Release() = 0;
};

//! Helper class to manage script object
class ScriptObjectPtr
{
private:
	ScriptObject* m_object; //!< Handled object
public:
	ScriptObjectPtr(ScriptObject* object = NULL) :
		m_object(object)
	{}

	~ScriptObjectPtr()
	{
		if (m_object)
			m_object->Release();
	}

	inline void operator = (ScriptObject* object)
	{
		if (m_object)
			m_object->Release();
		m_object = object;
	}

	inline operator ScriptObject* () const { return m_object; }

	inline ScriptObject* operator -> ()
	{
		return m_object;
	}
};

//! Helper class to manage script call
class ScriptCallPtr
{
private:
	ScriptStack* m_stack; //!< Used stack
public:
	ScriptCallPtr(ScriptStack* stack = NULL) :
		m_stack(stack)
	{}

	~ScriptCallPtr()
	{
		if (m_stack)
			m_stack->ReleaseAfterCall();
	}

	inline void operator = (ScriptStack* stack)
	{
		if (m_stack)
			m_stack->ReleaseAfterCall();
		m_stack = stack;
	}

	inline operator ScriptStack* () const { return m_stack; }

	inline ScriptStack* operator -> ()
	{
		return m_stack;
	}
};

/**
 *	An interface to logger registered for a context.
 */
class ScriptLogger
{
public:
	virtual ~ScriptLogger() {}
	//! Invoked any time script has got anything to output (script's output, errors, debug info)
	virtual void Output(const char* text, ...) = 0;
};

/**
 *	Language independent script context interface.
 *
 *	Note: To create the context use static ScriptContext::Create() method.
 */
class ScriptContext
{
protected:
	ScriptLogger* m_logger; //!< Logger used by this context

	ScriptContext() : m_logger(NULL) {}
public:
	virtual ~ScriptContext() {}

	//! Sets logger for the context; when set the whole script's output is forwarded to this logger
	virtual void SetLogger(ScriptLogger* logger)= 0;
	//! Collects garbage; by default performs single gc step (if supported)
	virtual void CollectGarbage(bool full = false) = 0;

	//! Executes the script given as string; returns true on success, false otherwise
	virtual bool ExecuteString(const char* string) = 0;

	//! Begins call to a script-registered function
	virtual ScriptStack* BeginCall(const char* name) = 0;
	//! Begins call to a script-registered function
	virtual ScriptStack* BeginCall(FunctionDesc* desc) = 0;

	//! Registers user supplied function
	virtual bool RegisterFunction(FunctionDesc* desc) = 0;
	//! Registers user supplied class
	virtual bool RegisterUserClass(ClassDesc* desc) = 0;

	//! Creates context for a given language name
	static ScriptContext* Create(const char* languageName, int stackSize = 1 << 16);

	//! Retrieves list of supported languages; the list is NULL terminated
	static const char** GetSupportedLanguages();
};