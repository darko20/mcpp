/*-
 * Copyright (c) 1998, 2002-2007 Kiyoshi Matsui <kmatsui@t3.rim.or.jp>
 * All rights reserved.
 *
 * Some parts of this code are derived from the public domain software
 * DECUS cpp (1984,1985) written by Martin Minow.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 *                              M A I N . C
 *                  M C P P   M a i n   P r o g r a m
 *
 * The main routine and it's supplementary routines are placed here.
 * The post-preprocessing routines are also placed here.
 */

#if PREPROCESSED                /* Use "pre-preprocessed" header    */
#include    "mcpp.H"
#else
#include    "system.H"
#include    "internal.H"
#endif

    /* Function pointer to expand_macro() functions.    */
    char *   (*expand_macro)( DEFBUF * defp, char * out, char * out_end);

    int     mcpp_mode = STD;        /* Mode of preprocessing        */

    int     cflag = FALSE;          /* -C option (keep comments)    */
    int     zflag = FALSE;      /* -i option (no output of included file)   */
    int     pflag = FALSE;          /* -P option (no #line output)  */
    int     qflag = FALSE;      /* -Q option (diagnostics to "mcpp.err")    */
    int     trig_flag = TRIGRAPHS_INIT;     /* -3 option (trigraphs)*/
    int     dig_flag = DIGRAPHS_INIT;       /* -2 option (digraphs) */
    long    cplus_val = 0L;         /* Value of __cplusplus for C++ */
    long    stdc_ver = 0L;          /* Value of __STDC_VERSION__    */
    int     stdc_val = 0;           /* Value of __STDC__            */
    int     stdc2;              /* cplus_val || stdc_ver >= 199901L */
    int     stdc3;              /* cplus_val >= 199901L || stdc_ver >= 199901L.
        (cplus_val >= 199901L) specifies compatible mode to C99 (extended
        feature of this preprocessor)   */
    int     standard = TRUE;    /* TRUE, if mcpp_mode is STD or POST_STD    */

/*
 * lang_asm allows the following non-standard features.
 * 1. #non-directive.
 * 2. <newline> in a string-literal.
 * 3. invalid pp-token generated by ## operator.
 * lang_asm is not available in POST_STD mode.
 */
    int     lang_asm = FALSE;       /* -a option (assembler source) */
    int     std_line_prefix = STD_LINE_PREFIX;
            /* Output line and file information in C source style   */

/*
 * Translation limits specified C90, C99 or C++.
 */
    /* The following three values are temporarily set for do_options()      */
    long    str_len_min = NBUFF;    /* Least maxmum of string len.  */
    size_t  id_len_min = IDMAX;     /* Least maximum of ident len.  */
    int     n_mac_pars_min = NMACPARS;  /* Least maximum of num of params.  */
    int     exp_nest_min;           /* Least maximum of expr nest   */
    int     blk_nest_min;           /* Least maximum of block nest  */
    int     inc_nest_min;           /* Least maximum of include nest*/
    long    n_macro_min;        /* Least maximum of num of macros   */

    long    line_limit;             /* Maximum source line number   */

/*
 * Commonly used global variables:
 * src_line     is the current input line number.
 * wrong_line   is set in many places when the actual output line is out of
 *              sync with the numbering, e.g, when expanding a macro with an
 *              embedded newline.
 * identifier   holds the last identifier scanned (which might be a candidate
 *              for macro expansion).
 * errors       is the running mcpp error counter.
 * infile       is the head of a linked list of input files (extended by
 *              #include and macros being expanded).  'infile' always points
 *              to the current file/macro.  'infile->parent' to the includer,
 *              etc.  'infile->fp' is NULL if this input stream is a macro.
 * inc_dirp     Directory of #includer with trailing PATH_DELIM.  This points
 *              to one of incdir[] or to the current directory (represented as
 *              "".  This should not be NULL.
 */
    long        src_line;           /* Current line number          */
    int         wrong_line;         /* Force #line to compiler      */
    int         newlines;           /* Count of blank lines         */
    int         errors = 0;         /* Cpp error counter            */
    int         warn_level = -1;    /* Level of warning (have to initialize)*/
    FILEINFO *  infile = NULL;      /* Current input file           */
    int         include_nest;       /* Nesting level of #include    */
    const char *    null = "";      /* "" string for convenience    */
    const char **   inc_dirp;       /* Directory of #includer       */
    const char *    cur_fname;      /* Current source file name     */
                /* cur_fname is not rewritten by #line directive    */
    char        cur_fullname[ FILENAMEMAX + 1];
        /* Full path of current source file (i.e. *inc_dirp/cur_fname)      */
    int         no_source_line;     /* Do not output line in diag.  */
    char        identifier[ IDMAX + IDMAX/8];       /* Current identifier   */
    int         mcpp_debug = 0;     /* != 0 if debugging now        */

/*
 *   in_directive is set TRUE while a directive line is scanned by directive().
 * It modifies the behavior of squeeze_ws() in expand.c so that newline is
 * not skipped even if getting macro arguments.
 */
    int     in_directive = FALSE;   /* TRUE scanning directive line */
    int     in_define = FALSE;      /* TRUE scanning #define line   */
    int     in_getarg = FALSE;      /* TRUE collecting macro arguments      */
    int     in_include = FALSE;     /* TRUE scanning #include line  */
    long    in_asm = 0L;    /* Starting line of #asm - #endasm block*/

/*
 *   macro_line is set to the line number of start of a macro call while
 * expanding the macro, else set to 0.  Line number is remembered for
 * diagnostics of unterminated macro call.  On unterminated macro call
 * macro_line is set to MACRO_ERROR.
 */
    long    macro_line = 0L;
/*
 *   macro_name is the currently expanding macro.
 */
    char *  macro_name;

/*
 *   compat_mode is set to TRUE, if recursive macro call is expanded more
 * than Standard's specification.  This mode is compatible to GCC and
 * some other implementations.
 */
    int     compat_mode = FALSE;

/*
 * openum is the return value of scan_op() in support.c.
 */
    int     openum;

/*
 *   mkdep means to output source file dependency line, specified by -M*
 * option.  The OR of the following values is used.
 *      MD_MKDEP    (1) :   Output dependency line.
 *      MD_SYSHEADER(2) :   Print also system headers or headers with
 *          absolute path not only user headers.
 *      MD_FILE     (4) :   Output to the file named *.d instead of fp_out.
 *          Normal output is done to fp_out as usual.
 */
    int     mkdep = 0;

/*
 *   If zflag is TRUE, no_output is incremented when a file is #included,
 * and decremented when the file is finished.
 * If no_output is larger than 0, processed files are not output, meanwhile
 * the macros in the files are defined.
 *   If mkdep != 0 && (mkdep & MD_FILE) == 0, no_output is set to 1 initially.
 */
    int     no_output = 0;

/*
 * keep_comments is set TRUE by the -C option.  If TRUE, comments are written
 * directly to the output stream.  This is needed if the output from cpp is
 * to be passed to lint (which uses commands embedded in comments).  cflag
 * contains the permanent state of the -C flag.  keep_comments is always
 * falsified when compilation is supressed by a false #if or when no_output
 * is TRUE.
 */
    int     keep_comments = 0;          /* Write out comments flag  */

/*
 * ifstack[] holds information about nested #if's.  It is always accessed via
 * ifptr->stat.  The information is as follows:
 *      WAS_COMPILING   state of compiling flag at outer level.
 *      ELSE_SEEN       set TRUE when #else seen to prevent 2nd #else.
 *      TRUE_SEEN       set TRUE when #if or #elif succeeds
 * ifstack[0].stat holds the compiling flag.  It is WAS_COMPILING if compila-
 * tion is currently enabled.  Note that this must be initialized to
 * WAS_COMPILING.
 */
    IFINFO      ifstack[ BLK_NEST + 1] = { {WAS_COMPILING, 0L, 0L}, };
                /* Note: '+1' is necessary for the initial state.   */
    IFINFO *    ifptr = ifstack;        /* -> current ifstack[]     */

/*
 * In POST_STD mode, insert_sep is set to INSERT_SEP when :
 *  1. the next get_ch() shall insert a token separator.
 *  2. unget_ch() has been called when insert_sep == INSERTED_SEP.
 * set to INSERTED_SEP when :
 *  get_ch() has been called when insert_sep == INSERT_SEP.
 * set to NO_SEP when :
 *  get_ch() has been called when insert_sep == INSERTED_SEP.
 */
    int     insert_sep = NO_SEP;

/*
 * has_pragma is set to TRUE so as to execute _Pragma() operator when the
 * psuedo macro _Pragma() is found.
 */
    int     has_pragma = FALSE;

/* File pointers for input and output.  */
    FILE *  fp_in;                  /* Input stream to preprocess   */
    FILE *  fp_out;                 /* Output stream preprocessed   */
    FILE *  fp_err;                 /* Diagnostics stream           */
    FILE *  fp_debug;               /* Debugging information stream */

/* Variables on multi-byte character encodings. */
    int     mbchar = MBCHAR;        /* Encoding of multi-byte char  */
    int     mbmask;                 /* Char type other than mbchar  */
    int     mbstart;                /* 1st byte of mbchar (or shift)*/
    int     bsl_in_mbchar;          /* 2nd byte of mbchar has '\\'  */
    int     bsl_need_escape;    /* '\\' in MBCHAR should be escaped */
    /* Function pointer to mb_read_*() functions.   */
    size_t  (*mb_read)( int c1, char ** in_pp, char ** out_pp);

    jmp_buf error_exit;             /* Exit on fatal error          */

/*
 * work_buf[] and workp are used to store one piece of text in a temporary
 * buffer.
 * To initialize storage, set workp = work_buf.  Note that the work buffer is
 * used by several subroutines -- be sure that your data won't be overwritten.
 * work_buf[] is used for:
 *      1. temporary buffer in macro expansion (exp_special(), expand_macro(),
 *         catenate())
 *      2. temporary buffer in processing directive line.
 */
    char        work_buf[ NWORK + IDMAX];       /* Work buffer      */
    char *      workp;              /* Pointer into work_buf[]      */
    char * const     work_end = & work_buf[ NWORK];
                                    /* End of buffer of work_buf[]  */

#define MBCHAR_IS_ESCAPE_FREE   (SJIS_IS_ESCAPE_FREE && \
            BIGFIVE_IS_ESCAPE_FREE && ISO2022_JP_IS_ESCAPE_FREE)

#if MCPP_LIB
static void     init_main( void);
                /* Initialize static variables      */
#endif
static void     init_defines( void);
                /* Predefine macros                 */
static void     mcpp_main( void);
                /* Main loop to process input lines */
static void     do_pragma_op( void);
                /* Execute the _Pragma() operator   */
static void     put_seq( char * begin, char * seq);
                /* Put out the failed sequence      */
static char *   de_stringize( char * in, char * out);
                /* "De-stringize" for _Pragma() op. */
static void     putout( char * out);
                /* May concatenate adjacent string  */
static void     devide_line( char * out);
                /* Devide long line for compiler    */
static void     put_a_line( char * out);
                /* Put out the processed line       */
#if ! HAVE_DIGRAPHS || ! MBCHAR_IS_ESCAPE_FREE
static int      post_preproc( char * out);
                /* Post-preprocess for older comps  */
#if ! HAVE_DIGRAPHS
static char *   conv_a_digraph( char * cp);
                /* Convert a digraph in place       */
#endif
#if ! MBCHAR_IS_ESCAPE_FREE
static char *   esc_mbchar( char * str, char * str_end);
                /* Insert \ before 2nd byte of SJIS */
#endif
#endif


#if MCPP_LIB
static void     init_main( void)
/* Initialize global variables on re-entering.  */
{
    mcpp_mode = STD;
    cflag = zflag = pflag = qflag = FALSE;
    trig_flag = TRIGRAPHS_INIT;
    dig_flag = DIGRAPHS_INIT;
    cplus_val = stdc_ver = 0L;
    stdc_val = 0;
    standard = TRUE;
    lang_asm = FALSE;
    std_line_prefix = STD_LINE_PREFIX;
    str_len_min = NBUFF;
    id_len_min = IDMAX;
    n_mac_pars_min = NMACPARS;
    errors = 0;
    warn_level = -1;
    infile = NULL;
    in_directive = in_define = in_getarg = in_include = FALSE;
    in_asm = 0L;
    macro_line = 0L;
    compat_mode = FALSE;
    mcpp_debug = mkdep = no_output = keep_comments = 0;
    ifstack[0].stat = WAS_COMPILING;
    ifstack[0].ifline = ifstack[0].elseline = 0L;
    ifptr = ifstack;
    insert_sep = NO_SEP;
    has_pragma = FALSE;
    mbchar = MBCHAR;
}

int     mcpp_lib_main
#else
int     main
#endif
(
    int argc,
    char ** argv
)
{
    char *  in_file = NULL;
    char *  out_file = NULL;

    if (setjmp( error_exit) == -1)
        goto  fatal_error_exit;

#if MCPP_LIB
    /* Initialize static variables. */
    init_main();
    init_directive();
    init_eval();
    init_support();
    init_system();
#if NEED_GETOPT
    init_lib();
#endif
#endif

    fp_in = stdin;
    fp_out = stdout;
    fp_err = stderr;
    fp_debug = stdout;
        /*
         * Debugging information is output to stdout in order to
         *      synchronize with preprocessed output.
         */

    inc_dirp = &null;   /* Initialize to current (null) directory   */
    cur_fname = "(predefined)";     /* For predefined macros        */
    init_defines();                         /* Predefine macros     */
    mb_init();      /* Should be initialized prior to get options   */
    do_options( argc, argv, &in_file, &out_file);   /* Command line options */

    /* Open input file, "-" means stdin.    */
    if (in_file != NULL && ! str_eq( in_file, "-")) {
        if (freopen( in_file, "r", fp_in) == NULL) {
            mcpp_fprintf( ERR, "Can't open input file \"%s\".\n", in_file);
#if MCPP_LIB
            goto  fatal_error_exit;
#else
            return( IO_ERROR);
#endif
        }
        strcpy( work_buf, in_file); /* Remember input filename      */
    } else {
        strcpy( work_buf, "<stdin>");
    }
    /* Open output file, "-" means stdout.  */
    if (out_file != NULL && ! str_eq( out_file, "-")) {
        if (freopen( out_file, "w", fp_out) == NULL) {
            mcpp_fprintf( ERR, "Can't open output file \"%s\".\n", out_file);
#if MCPP_LIB
            goto  fatal_error_exit;
#else
            return( IO_ERROR);
#endif
        }
    }
    if (qflag) {                            /* Redirect diagnostics */
        if (freopen( "mcpp.err", "a", fp_err) == NULL) {
            mcpp_fprintf( OUT, "Can't open \"mcpp.err\"\n");
#if MCPP_LIB
            goto  fatal_error_exit;
#else
            return( IO_ERROR);
#endif
        }
    }
    add_file( fp_in, work_buf);     /* "open" main input file       */
    infile->dirp = inc_dirp;
    strcpy( cur_fullname, work_buf);
    if (mkdep && str_eq( infile->real_fname, "<stdin>") == FALSE)
        put_depend( work_buf);      /* Putout target file name      */
    at_start();                     /* Do the pre-main commands     */

    mcpp_main();                    /* Process main file            */

    if (mkdep)
        put_depend( NULL);      /* Append '\n' to dependency line   */
    at_end();                       /* Do the final commands        */

fatal_error_exit:
#if MCPP_LIB
    /* Free malloced memory */
    clear_filelist();
    clear_symtable();
#endif

    if (mcpp_debug & MEMORY)
        print_heap();
    if (errors > 0 && no_source_line == FALSE) {
        mcpp_fprintf( ERR, "%d error%s in preprocessor.\n",
                errors, (errors == 1) ? "" : "s");
        return  IO_ERROR;
    }
    return  IO_SUCCESS;             /* No errors or -E option set   */
}

void    sharp( void)
/*
 * Output a line number line.
 */
{
    if (no_output || pflag || infile == NULL)
        goto  sharp_exit;
    if (keep_comments)
        mcpp_fputc( '\n', OUT);         /* Ensure to be on line top */
    if (std_line_prefix)
        mcpp_fprintf( OUT, "#line %ld", src_line);
    else
        mcpp_fprintf( OUT, "%s%ld", LINE_PREFIX, src_line);
    cur_file();
    mcpp_fputc( '\n', OUT);
sharp_exit:
    wrong_line = FALSE;
}

/*
 * This is the table used to predefine target machine, operating system and
 * compiler designators.  It may need hacking for specific circumstances.
 * The -N option supresses these definitions.
 */
typedef struct pre_set {
    const char *    name;
    const char *    val;
} PRESET;

static PRESET   preset[] = {

#ifdef  CPU_OLD
        { CPU_OLD, "1"},
#endif
#ifdef  CPU_SP_OLD
        { CPU_SP_OLD, "1"},
#endif
#ifdef  SYSTEM_OLD
        { SYSTEM_OLD, "1"},
#endif
#ifdef  SYSTEM_SP_OLD
        { SYSTEM_SP_OLD, "1"},
#endif
#ifdef  COMPILER_OLD
        { COMPILER_OLD, "1"},
#endif
#ifdef  COMPILER_SP_OLD
        { COMPILER_SP_OLD, "1"},
#endif

        { NULL, NULL},  /* End of macros beginning with alphabet    */

#ifdef  CPU_STD
        { CPU_STD, "1"},
#endif
#ifdef  CPU_STD1
        { CPU_STD1, "1"},
#endif
#ifdef  CPU_STD2
        { CPU_STD2, "1"},
#endif
#ifdef  SYSTEM_STD
        { SYSTEM_STD, "1"},
#endif
#ifdef  SYSTEM_STD1
        { SYSTEM_STD1, "1"},
#endif
#ifdef  SYSTEM_STD2
        { SYSTEM_STD2, "1"},
#endif

#ifdef  SYSTEM_EXT
        { SYSTEM_EXT, SYSTEM_EXT_VAL},
#endif
#ifdef  SYSTEM_EXT2
        { SYSTEM_EXT2, SYSTEM_EXT2_VAL},
#endif
#ifdef  SYSTEM_SP_STD
        { SYSTEM_SP_STD, SYSTEM_SP_STD_VAL},
#endif
#ifdef  COMPILER_STD
        { COMPILER_STD, COMPILER_STD_VAL},
#endif
#ifdef  COMPILER_STD1
        { COMPILER_STD1, COMPILER_STD1_VAL},
#endif
#ifdef  COMPILER_STD2
        { COMPILER_STD2, COMPILER_STD2_VAL},
#endif
#ifdef  COMPILER_EXT
        { COMPILER_EXT, COMPILER_EXT_VAL},
#endif
#ifdef  COMPILER_EXT2
        { COMPILER_EXT2, COMPILER_EXT2_VAL},
#endif
#ifdef  COMPILER_SP_STD
        { COMPILER_SP_STD, COMPILER_SP_STD_VAL},
#endif
#ifdef  COMPILER_SP1
        { COMPILER_SP1, COMPILER_SP1_VAL},
#endif
#ifdef  COMPILER_SP2
        { COMPILER_SP2, COMPILER_SP2_VAL},
#endif
#ifdef  COMPILER_SP3
        { COMPILER_SP3, COMPILER_SP3_VAL},
#endif
#ifdef  COMPILER_SP4
        { COMPILER_SP4, COMPILER_SP4_VAL},
#endif
#ifdef  COMPILER_CPLUS
        { COMPILER_CPLUS, COMPILER_CPLUS_VAL},
#endif
        { NULL, NULL},  /* End of macros with value of any integer  */
};

static void init_defines( void)
/*
 * Initialize the built-in #define's.
 * Called only on cpp startup prior to do_options().
 *
 * Note: the built-in static definitions are removed by the -N option,
 * definitions beginning with alphabet are removed by the -S1 option,
 */
{
    int     n = sizeof preset / sizeof (PRESET);
    PRESET *    pp;

    /* Predefine the built-in symbols.  */
    for (pp = preset; pp < preset + n; pp++) {
        if (pp->name && *(pp->name))
            look_and_install( pp->name, DEF_NOARGS - 1, null, pp->val);
    }

    look_and_install( "__MCPP", DEF_NOARGS - 1, null, "2");
    /* MCPP V.2.x   */
    /* This macro is predefined yet can be undefined by -U or #undef.   */
}

void    un_predefine(
    int clearall                            /* TRUE for -N option   */
)
/*
 * Remove predefined symbols from the symbol table.
 */
{
    PRESET *    pp;
    DEFBUF *    defp;
    int     n = sizeof preset / sizeof (PRESET);

    for (pp = preset; pp < preset + n; pp++) {
        if (pp->name) {
            if (*(pp->name) && (defp = look_id( pp->name)) != NULL
                    && defp->nargs == DEF_NOARGS - 1)
                undefine( pp->name);
        } else if (clearall == FALSE) {     /* -S<n> option         */
            break;
        }
    }
}

void    undef_a_predef(
    const char * name
)
/*
 * Remove a predefined name from the preset[] table so that the name can be
 * redefined by -D option.
 * The strange ordering (insert, command-line-scan, remove)
 * is needed to avoid interaction with -D arguments.
 */
{
    PRESET *    pp;
    int     n = sizeof preset / sizeof (PRESET);

    for (pp = preset; pp < preset + n; pp++) {
        if (pp->name && *(pp->name) && str_eq( pp->name, name)) {
            pp->name = "";
            break;
        }
    }
}

/*
 * output[] and out_ptr are used for:
 *      buffer to store preprocessed line (this line is put out or handed to
 *      post_preproc() via putout() in some cases)
 */
static char     output[ NMACWORK];  /* Buffer for preprocessed line */
static char * const out_end = & output[ NWORK - 2];
                                    /* Limit of output line         */
static char * const out_wend = & output[ NMACWORK - 2];
                                    /* Buffer end of output line    */
static char *       out_ptr;        /* Current pointer into output[]*/

static void mcpp_main( void)
/*
 * Main process for mcpp -- copies tokens from the current input stream
 * (main file or included file) to the output file.
 */
{
    int     c;                      /* Current character            */
    char *  wp;                     /* Temporary pointer            */
    DEFBUF *    defp;               /* Macro definition             */

    if (! no_output) {  /* Explicitly output a #line at the start of cpp    */
        src_line++;
        sharp();
        put_info();                         /* -fworking-directory  */
        src_line--;
    }
    keep_comments = cflag && !no_output;

    /*
     * This loop is started "from the top" at the beginning of each line.
     * 'wrong_line' is set TRUE in many places if it is necessary to write
     * a #line record.  (But we don't write them when expanding macros.)
     *
     * 'newlines' variable counts the number of blank lines that have been
     * skipped over.  These are then either output via #line records or
     * by outputting explicit blank lines.
     * 'newlines' will be cleared on end of an included file by get_ch().
     */
    while (1) {                             /* For the whole input  */
        newlines = 0;                       /* Count empty lines    */

        while (1) {                         /* For each line, ...   */
            c = get_ch();                   /* First of the line    */
            out_ptr = output;               /* Top of the line buf  */
            if (c == ' ') {         /* Dosen't occur in POST_STD    */
                *out_ptr++ = ' ';           /* Retain a space       */
                c = get_ch();       /* First of token (else '\n')   */
            }
            if (mcpp_mode == OLD_PREP && c == COM_SEP)
                 c = get_ch();              /* Skip 0-length comment*/
            if (c == '#') {                 /* Is 1st non-space '#' */
                directive();                /* Do a #directive      */
            } else if (mcpp_mode == STD && dig_flag && c == '%') {
                    /* In POST_STD digraphs are already converted   */
                if (get_ch() == ':') {      /* '%:' i.e. '#'        */
                    directive();            /* Do a #directive      */
                } else {
                    unget_ch();
                    if (! compiling) {
                        skip_nl();
                        newlines++;
                    } else {
                        break;
                    }
                }
            } else if (c == CHAR_EOF) {     /* End of input         */
                break;
            } else if (! compiling) {       /* #ifdef false?        */
                skip_nl();                  /* Skip to newline      */
                newlines++;                 /* Count it, too.       */
            } else if (in_asm && ! no_output) { /* In #asm block    */
                put_asm();                  /* Put out as it is     */
            } else if (c == '\n') {         /* Blank line           */
                if (keep_comments)
                    mcpp_fputc( '\n', OUT); /* May flush comments   */
                else
                    newlines++;             /* Wait for a token     */
            } else {
                break;                      /* Actual token         */
            }
        }

        if (c == CHAR_EOF)                  /* Exit process at      */
            break;                          /*   end of input       */

        /*
         * If the loop didn't terminate because of end of file, we
         * know there is a token to compile.  First, clean up after
         * absorbing newlines.  newlines has the number we skipped.
         */
        if (no_output) {
            wrong_line = FALSE;
        } else {
            if (wrong_line || newlines > 10) {
                sharp();                /* Output # line number */
            } else {                    /* If just a few, stuff */
                while (newlines-- > 0)  /* them out ourselves   */
                    mcpp_fputc('\n', OUT);
            }
        }

        /*
         * Process each token on this line.
         */
        while (c != '\n' && c != CHAR_EOF) {    /* For the whole line   */
            if (scan_token( c, (wp = out_ptr, &wp), out_wend) == NAM
                    && (defp = is_macro( &wp)) != NULL) {   /* A macro  */
                wp = expand_macro( defp, out_ptr, out_wend);
                                            /* Expand it completely */
                if (has_pragma) {           /* Found _Pramga()      */
                    do_pragma_op();         /* Do _Pragma() operator*/
                    has_pragma = FALSE;     /* Reset signal         */
                    out_ptr = output;       /* Do the rest of line  */
                    wrong_line = TRUE;      /* Line-num out of sync */
                } else {
                    out_ptr = wp;
                }
            } else {                        /* Not a macro call     */
                out_ptr = wp;               /* Advance the place    */
                if (wrong_line)             /* is_macro() swallowed */
                    break;                  /*      the newline     */
            }
            if ((c = get_ch()) == ' ') {    /* Token separator      */
                *out_ptr++ = ' ';
                c = get_ch();               /* First of token       */
            }
            if (mcpp_mode == OLD_PREP && c == COM_SEP)
                c = get_ch();               /* Skip 0-length comment*/
        }                                   /* Line for loop        */

        putout( output);                    /* Output the line      */
    }                                       /* Continue until EOF   */
}

static void do_pragma_op( void)
/*
 * Execute the _Pragma() operator contained in an expanded macro.
 * Note: _Pragma() operator is also implemented as a special macro.  Therefore
 *      it is always searched as a macro.
 * There might be more than one _Pragma() in a expanded macro and those may be
 *      surrounded by other token sequences.
 * Since all the macros have been expanded completely, any name identical to
 *      macro should not be re-expanded.
 */
{
    FILEINFO *  file;
    DEFBUF *    defp;
    int     prev = output < out_ptr;        /* There is a previous sequence */
    int     token_type;
    char *  cp1, * cp2;
    int     c;

    file = unget_string( out_ptr, NULL);
    while (c = get_ch(), file == infile) {
        if (c == ' ') {
            *out_ptr++ = ' ';
            continue;
        }
        if (scan_token( c, (cp1 = out_ptr, &cp1), out_wend)
                    == NAM && (defp = is_macro( &cp1)) != NULL
                && defp->nargs == DEF_PRAGMA) {     /* _Pragma() operator   */
            if (prev) {
                putout( output);    /* Putout the previous sequence */
                cp1 = stpcpy( output, "pragma ");   /* From top of buffer   */
            }
            *cp1++ = get_ch();                              /* '('  */
            while ((c = get_ch()) == ' ')
                *cp1++ = ' ';
            if (((token_type = scan_token( c, (cp2 = cp1, &cp1), out_wend))
                    != STR && token_type != WSTR)) {
                /* Not a string literal */
                put_seq( output, cp1);
                return;
            }
            workp = de_stringize( cp2, work_buf);
            while ((c = get_ch()) == ' ')
                *cp1++ = ' ';
            if (c != ')') {         /* More than a string literal   */
                unget_ch();
                put_seq( output, cp1);
                return;
            }
            strcpy( workp, "\n");       /* Terminate with <newline> */
            unget_string( work_buf, NULL);
            do_pragma();                /* Do the #pragma "line"    */
            infile->bptr += strlen( infile->bptr);      /* Clear sequence   */
            cp1 = out_ptr = output;     /* From the top of buffer   */
            prev = FALSE;
        } else {                        /* Not pragma sequence      */
            out_ptr = cp1;
            prev = TRUE;
        }
    }
    unget_ch();
    if (prev)
        putout( output);
}

static void put_seq(
    char *  begin,                  /* Sequence already in buffer   */
    char *  seq                     /* Sequence to be read          */
)
/*
 * Put out the failed sequence as it is.
 */
{
    FILEINFO *  file = infile;
    int     c;

    cerror( "Operand of _Pragma() is not a string literal"  /* _E_  */
            , NULL, 0L, NULL);
    while (c = get_ch(), file == infile)
        *seq++ = c;
    unget_ch();
    out_ptr = seq;
    putout( begin);
}

static char *   de_stringize(
    char *  in,                 /* Null terminated string literal   */
    char *  out                             /* Output buffer        */
)
/*
 * Make token sequence from a string literal for _Pragma() operator.
 */
{
    char *  in_p;
    int     c1;
    int     c;

    in_p = in;
    if (*in_p == 'L')
        in_p++;                             /* Skip 'L' prefix      */
    while ((c = *++in_p) != EOS) {
        if (c == '\\' && ((c1 = *(in_p + 1), c1 == '\\') || c1 == '"'))
            c = *++in_p;            /* "De-escape" escape sequence  */
        *out++ = c;
    }
    *--out = EOS;                   /* Remove the closing '"'       */
    return  out;
}

static void putout(
    char *  out     /* Output line (line-end is always 'out_ptr')   */
)
/*
 * Put out a line with or without "post-preprocessing".
 */
{
    size_t  len;

    *out_ptr++ = '\n';                      /* Put out a newline    */
    *out_ptr = EOS;

#if ! MBCHAR_IS_ESCAPE_FREE
    post_preproc( out);
#elif   ! HAVE_DIGRAPHS
    if (mcpp_mode == STD && dig_flag)
        post_preproc( out);
#endif
    /* Else no post-preprocess  */
    len = strlen( out);
    if (len > NWORK - 1)
        devide_line( out);
    else
        put_a_line( out);
}

static void devide_line(
    char * out                      /* 'out' is 'output' in actual  */
)
/*
 * Devide a too long line into output lines shorter than NWORK.
 * This routine is called from putout().
 */
{
    FILEINFO *  file;
    char *  save;
    char *  wp;
    int     c;

    file = unget_string( out, NULL);        /* To re-read the line  */
    wp = out_ptr = out;

    while ((c = get_ch()), file == infile) {
        if (c == ' ') {
            if (out == out_ptr || *(out_ptr - 1) != ' ') {
                *out_ptr++ = ' ';
                wp++;
            }
            continue;
        }
        scan_token( c, &wp, out_wend);          /* Read a token     */
        if (NWORK-2 < wp - out_ptr) {           /* Too long a token */
            cfatal( "Too long token %s", out_ptr, 0L, NULL);        /* _F_  */
        } else if (out_end <= wp) {             /* Too long line    */
            save = save_string( out_ptr);       /* Save the token   */
            *out_ptr++ = '\n';                  /* Append newline   */
            *out_ptr = EOS;
            put_a_line( out);           /* Putout the former tokens */
            wp = out_ptr = stpcpy( out, save);      /* Restore the token    */
            free( save);
        } else {                            /* Still in size        */
            out_ptr = wp;                   /* Advance the pointer  */
        }
    }

    unget_ch();                 /* Push back the source character   */
    put_a_line( out);                   /* Putout the last tokens   */
    sharp();                                /* Correct line number  */
}

static void put_a_line(
    char * out
)
/*
 * Finally put out the preprocessed line.
 */
{
    size_t  len;
    char *  out_p;
    char *  tp;

    if (no_output)
        return;
    len = strlen( out);
    tp = out_p = out + len - 2;             /* Just before '\n'     */
    while (char_type[ *out_p & UCHARMAX] & SPA)
        out_p--;                    /* Remove trailing white spaces */
    if (out_p < tp) {
        *++out_p = '\n';
        *++out_p = EOS;
    }
    if (mcpp_fputs( out, OUT) == EOF)
        cfatal( "File write error", NULL, 0L, NULL);        /* _F_  */
}


/*
 *      Routines to  P O S T - P R E P R O C E S S
 *
 * 1998/08      created     kmatsui     (revised 1998/09, 2004/02, 2006/07)
 *    Supplementary phase for the older compiler-propers.
 *      1. Convert digraphs to usual tokens.
 *      2. Double '\\' of the second byte of multi-byte characters.
 *    These conversions are done selectively according to the macros defined
 *  in system.H.
 *      1. Digraphs are converted if ! HAVE_DIGRAPHS and digraph recoginition
 *  is enabled by DIGRAPHS_INIT and/or -2 option on execution.
 *      2. '\\' of the second byte of SJIS (BIGFIVE or ISO2022_JP) is doubled
 *  if bsl_need_escape == TRUE.
 */

#if HAVE_DIGRAPHS && MBCHAR_IS_ESCAPE_FREE
    /* No post_preproc()    */
#else

static int  post_preproc(
    char * out
)
/*
 * Convert digraphs and double '\\' of the second byte of SJIS (BIGFIVE or
 * ISO2022_JP).
 */
{
#if ! HAVE_DIGRAPHS
    int     di_count = 0;
#endif
    int     token_type;
    int     c;
    char *  str;
    char *  cp = out;

    unget_string( out, NULL);
    while ((c = get_ch()) != '\n') {    /* Not to read over to next line    */
        if (c == ' ') {
            *cp++ = ' ';
            continue;
        }
        str = cp;
        token_type = scan_token( c, &cp, out_wend);
        switch (token_type) {
#if ! MBCHAR_IS_ESCAPE_FREE
        case WSTR   :
        case WCHR   :
            str++;                          /* Skip prefix 'L'      */
            /* Fall through */
        case STR    :
        case CHR    :
            if (bsl_need_escape)
                cp = esc_mbchar( str, cp);
            break;
#endif  /* ! MBCHAR_IS_ESCAPE_FREE  */
#if ! HAVE_DIGRAPHS
        case OPE    :
            if (mcpp_mode == STD && (openum & OP_DIGRAPH)) {
                cp = conv_a_digraph( cp);   /* Convert a digraph    */
                di_count++;
            }
            break;
#endif
        }
    }
    *cp++ = '\n';
    *cp = EOS;
#if ! HAVE_DIGRAPHS
    if (mcpp_mode == STD && di_count && (warn_level & 16))
        cwarn( "%.0s%ld digraph(s) converted"           /* _W16_    */
                , NULL, (long) di_count, NULL);
#endif
    return  0;
}

#endif  /* ! HAVE_DIGRAPHS || ! MBCHAR_IS_ESCAPE_FREE   */

#if ! HAVE_DIGRAPHS
static char *   conv_a_digraph(
    char *  cp                      /* The end of the digraph token */
)
/*
 * Convert a digraph to usual token in place.
 * This routine is never called in POST_STD mode.
 */
{
    cp -= 2;
    switch (openum) {
    case OP_LBRACE_D    :
        *cp++ = '{';
        break;
    case OP_RBRACE_D    :
        *cp++ = '}';
        break;
    case OP_LBRCK_D     :
        *cp++ = '[';
        break;
    case OP_RBRCK_D     :
        *cp++ = ']';
        break;
    case OP_SHARP_D     :                       /* Error of source  */
        *cp++ = '#';
        break;
    case OP_DSHARP_D    :                       /* Error of source  */
        cp -= 2;
        *cp++ = '#';
        *cp++ = '#';
        break;
    }
    return  cp;
}
#endif  /* ! HAVE_DIGRAPHS  */

#if ! MBCHAR_IS_ESCAPE_FREE
static char *   esc_mbchar(
    char *  str,        /* String literal or character constant without 'L' */
    char *  str_end     /* The end of the token */
)
/*
 * Insert \ before the byte of 0x5c('\\') of the SJIS, BIGFIVE or ISO2022_JP
 * multi-byte character code in string literal or character constant.
 * Insert \ also before the byte of 0x22('"') and 0x27('\'') of ISO2022_JP.
 * esc_mbchar() does in-place insertion.
 */
{
    char *  cp;
    int     delim;
    int     c;

    if (! bsl_need_escape)
        return  str_end;
    if ((delim = *str++) == 'L')
        delim = *str++;                         /* The quote character  */
    while ((c = *str++ & UCHARMAX) != delim) {
        if (char_type[ c] & mbstart) {               /* MBCHAR   */
            cp = str;
            mb_read( c, &str, (workp = work_buf, &workp));
            while (cp++ < str) {
                c = *(cp - 1);
                if (c == '\\' || c == '"' || c == '\'') {
                                    /* Insert \ before 0x5c, 0x22, 0x27 */
                    memmove( cp, cp - 1, (size_t) (str_end - cp) + 2);
                    *(cp++ - 1) = '\\';
                    str++;
                    str_end++;
                }
            }
        } else if (c == '\\' && ! (char_type[ *str & UCHARMAX] & mbstart)) {
            str++;                              /* Escape sequence      */
        }
    }
    return  str_end;
}
#endif  /* ! MBCHAR_IS_ESCAPE_FREE  */

