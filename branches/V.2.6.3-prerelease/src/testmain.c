/*
 * testmain.c: A sample source to show how to use mcpp as a subroutine.
 * 2006/11  Contributed by Juergen Mueller.
 * Refer to mcpp-porting.html section 3.12 for compiling mcpp as a subroutine.
 *
 * 2007/02  Updated to enable testing of memory buffer output.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MCPP_LIB    1
#include "mcpp_lib.h"

extern int  mcpp_lib_main(int argc, char **argv);

int main(int argc, char *argv[])
{
    int     i, j;
    char ** tmp_argv;

    tmp_argv = (char **) malloc(sizeof(char *) * (argc + 1));

    if (tmp_argv == NULL) {
        return(0);
    }

    /*
     * assume strict order: <options, ...> <files, ...> and
     * each option and its argument are specified without intervening spaces
     * such as '-I/dir' or '-otmp.i' (not '-I /dir' nor '-o tmp.i').
     */
    for (i = 0; i < argc; ++i) {
        tmp_argv[i] = argv[i];              /* copy options */

        if (       (*argv[i] != '-')
                && (*argv[i] != '/')
                && (i > 0)) {
            break;
        }
    }

    /* this works only if called function does not manipulate pointer array! */
    for (j = i; i < argc; ++i) {
        int     retval;
        char *  result;
        clock_t start, finish;

        tmp_argv[j] = argv[i];              /* process each file    */
        fprintf(stderr, "\n%s\n", argv[i]);

        start = clock();                    /* get start time       */
#if OUT2MEM         /* Use memory buffer    */
        mcpp_use_mem_buffers( 1);           /* enable memory output */
        retval = mcpp_lib_main(j + 1, tmp_argv);    /* call MCPP    */
        result = mcpp_get_mem_buffer( OUT); /* get the output       */
        if (result)
            fputs( result, stdout);
        result = mcpp_get_mem_buffer( ERR); /* get the diagnostics  */
        if (result)
            fputs( result, stderr);
        result = mcpp_get_mem_buffer( DBG); /* get the debug output */
        if (result)
            fputs( result, stdout);         /* appended to output   */
#else               /* Normal output to file    */
        retval = mcpp_lib_main(j + 1, tmp_argv);
#endif
        finish = clock();                   /* get finish time      */

        fprintf(stderr, "\nReturned status:%d, Elapsed time: %.3f seconds.\n",
                retval, (double)(finish - start) / (double)CLOCKS_PER_SEC);
    }

    free(tmp_argv);

    return(0);
}
