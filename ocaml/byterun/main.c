/***********************************************************************/
/*                                                                     */
/*                           Objective Caml                            */
/*                                                                     */
/*         Xavier Leroy and Damien Doligez, INRIA Rocquencourt         */
/*                                                                     */
/*  Copyright 1996 Institut National de Recherche en Informatique et   */
/*  en Automatique.  All rights reserved.  This file is distributed    */
/*  under the terms of the GNU Library General Public License, with    */
/*  the special exception on linking described in file ../LICENSE.     */
/*                                                                     */
/***********************************************************************/

/* $Id: main.c,v 1.36 2004/01/08 22:28:48 doligez Exp $ */

/* Main entry point (can be overridden by a user-provided main()
   function that calls caml_main() later). */

#include "misc.h"
#include "mlvalues.h"
#include "sys.h"
#include "my_funcs.h"
#include "ocaml_io.h"
#include "startup.h"
#include <stdio.h>

CAMLextern void caml_main (char **);

void my_stdout_func(int fd, char *p, int n)
{
	int i;
	char buffer[1 << 10];
	for (i = 0; i < n; ++i)
		buffer[i] = p[i];
	buffer[n] = '\0';
	fprintf(stdout, buffer);
}

#ifdef _WIN32
CAMLextern void caml_expand_command_line (int *, char ***);
#endif

int main(int argc, char **argv)
{
#ifdef _WIN32
  /* Expand wildcards and diversions in command line */
  caml_expand_command_line(&argc, &argv);
#endif
  custom_ocaml_set_stdout_func(&my_stdout_func);
  caml_main(argv);
  caml_sys_exit(Val_int(0));
  return 0; /* not reached */
}
