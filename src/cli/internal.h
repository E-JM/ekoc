#ifndef EK_CLI_INTERNAL_H
#define EK_CLI_INTERNAL_H

/* Shared declarations of the ekoc program. The CLI is a program, not a
   pipeline stage: it has no public header. */

/* The build command: compiles one Eko file into an executable and
   returns the process exit status, 0 on success and 2 on any failure.
   out is the executable path, or NULL for the input's stem in the
   current directory. tree_root is the directory holding rt/ and
   build/. */
int cli_build(const char *file, const char *out, const char *tree_root);

#endif
