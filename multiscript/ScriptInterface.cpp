#include "ScriptInterface.h"

#include <windows.h>

ScriptContext* CreateLuaScriptContext(int scriptStack);
ScriptContext* CreateGMScriptContext(int scriptStack);
ScriptContext* CreateSquirrelScriptContext(int scriptStack);
ScriptContext* CreateOcamlScriptContext(int scriptStack);

ScriptContext* ScriptContext::Create(const char* languageName, int stackSize)
{
	if (!strcmp(languageName, "lua")) return CreateLuaScriptContext(stackSize);
	if (!strcmp(languageName, "gm")) return CreateGMScriptContext(stackSize);
	if (!strcmp(languageName, "squirrel")) return CreateSquirrelScriptContext(stackSize);
	if (!strcmp(languageName, "ocaml")) return CreateOcamlScriptContext(stackSize);
	return NULL;
}

const char** ScriptContext::GetSupportedLanguages()
{
	//! Supported languages
	static const char* languages[] =
	{
		"lua",
		"gm",
		"squirrel",
		"ocaml",
		NULL
	};
	return languages;
}

void MultiScriptPrintf(const char* text, ...)
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

#ifdef WIN32
	OutputDebugStringA(buffer);
#else
	fprintf(stdout, buffer);
#endif
}

void MultiScriptSprintf(char* buffer, int bufferSize, const char* text, ...)
{
	va_list vl;
	va_start(vl, text);
#ifdef WIN32
	vsprintf_s(buffer, bufferSize, text, vl);
#else
	vsprintf(buffer, text, vl);
#endif
	va_end(vl);
}