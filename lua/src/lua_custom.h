
#ifndef lua_custom_h
#define lua_custom_h

typedef void (*lua_print_callback_func)(lua_State* L, const char* text);
extern lua_print_callback_func lua_print_callback;

#endif // lua_custom_h