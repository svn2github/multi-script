/***********************************************************************/
/*                                                                     */
/*                           Objective Caml                            */
/*                                                                     */
/*            Xavier Leroy, projet Cristal, INRIA Rocquencourt         */
/*                                                                     */
/*  Copyright 2001 Institut National de Recherche en Informatique et   */
/*  en Automatique.  All rights reserved.  This file is distributed    */
/*  under the terms of the GNU Library General Public License, with    */
/*  the special exception on linking described in file ../LICENSE.     */
/*                                                                     */
/***********************************************************************/

/* $Id: startup.h,v 1.5 2004/02/22 15:07:51 xleroy Exp $ */

#ifndef CAML_STARTUP_H
#define CAML_STARTUP_H

#include "mlvalues.h"
#include "prims.h"
#include "exec.h"

// ======================== Custom caml stuff (msawitus) ===========================

typedef struct caml_code caml_code;
typedef struct caml_c_function caml_c_function;
typedef struct caml_vm caml_vm;

//! Caml code representation
struct caml_code
{
	code_t start_code; //!< The beginning of the code
	asize_t code_size; //!< The size of the code
	value global_data; //!< Globals used by the code
	struct ext_table prim_table; //!< Table containing all C primitives (C functions) used by the code
	struct ext_table prim_table_user_data; //!< Table containing user data for each C primitive in prim_table; NULL for all built-in primitives
};

//! Custom, user supplied, C function representation
struct caml_c_function
{
	const char* name; //!< Function name
	c_primitive prim; //!< The actual C function pointer
	void* user_data; //!< Custom user data associated with the function
};

//! Ocaml virtual machine representation
struct caml_vm
{
	struct ext_table c_functions; //!< Set of dynamically registered functions
};

extern caml_vm vm; //!< Global Caml virtual machine

CAMLextern int custom_caml_init();
CAMLextern int custom_caml_register_c_function(const char* name, c_primitive prim, void* user_data);
CAMLextern int custom_caml_load_code(const char* path, caml_code* code);
CAMLextern int custom_caml_run_code(caml_code* code);
CAMLextern void* custom_caml_get_current_c_function_user_data();

// ===================================================================

CAMLextern void caml_main(char **argv);

CAMLextern void caml_startup_code(
           code_t code, asize_t code_size,
           char *data, asize_t data_size,
           char *section_table, asize_t section_table_size,
           char **argv);

enum { FILE_NOT_FOUND = -1, BAD_BYTECODE  = -2 };

extern int caml_attempt_open(char **name, struct exec_trailer *trail,
                             int do_open_script);
extern void caml_read_section_descriptors(int fd, struct exec_trailer *trail);
extern int32 caml_seek_optional_section(int fd, struct exec_trailer *trail,
                                        char *name);
extern int32 caml_seek_section(int fd, struct exec_trailer *trail, char *name);


#endif /* CAML_STARTUP_H */
