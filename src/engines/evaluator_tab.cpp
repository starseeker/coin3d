/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Substitute the type names.  */
#define YYSTYPE         SO_EVALSTYPE
/* Substitute the variable and function names.  */
#define yyparse         so_evalparse
#define yylex           so_evallex
#define yyerror         so_evalerror
#define yydebug         so_evaldebug
#define yynerrs         so_evalnerrs
#define yylval          so_evallval
#define yychar          so_evalchar

/* First part of user prologue.  */

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

/*
 * Syntax analyzer for SoCalculator expressions.
 *
 * Compile with (GNU Bison 3.x):
 *
 *         bison -o evaluator_tab.cpp -l evaluator.y
 *
 * The api.prefix is set via %define below - no command-line -D flag needed.
 * No post-generation patching is required with modern bison (3.x+).
 */

#include "config.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_IO_H
/* isatty() on windows */
#include <io.h>
#endif /* HAVE_IO_H */
#include <Inventor/basic.h>
#include "engines/evaluator.h"
#include <mutex>


# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif


/* Debug traces.  */
#ifndef SO_EVALDEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define SO_EVALDEBUG 1
#  else
#   define SO_EVALDEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define SO_EVALDEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined SO_EVALDEBUG */
#if SO_EVALDEBUG
extern int so_evaldebug;
#endif

/* Token kinds.  */
#ifndef SO_EVALTOKENTYPE
# define SO_EVALTOKENTYPE
  enum so_evaltokentype
  {
    SO_EVALEMPTY = -2,
    SO_EVALEOF = 0,                /* "end of file"  */
    SO_EVALerror = 256,            /* error  */
    SO_EVALUNDEF = 257,            /* "invalid token"  */
    LEX_VALUE = 258,               /* LEX_VALUE  */
    LEX_TMP_FLT_REG = 259,         /* LEX_TMP_FLT_REG  */
    LEX_IN_FLT_REG = 260,          /* LEX_IN_FLT_REG  */
    LEX_OUT_FLT_REG = 261,         /* LEX_OUT_FLT_REG  */
    LEX_TMP_VEC_REG = 262,         /* LEX_TMP_VEC_REG  */
    LEX_IN_VEC_REG = 263,          /* LEX_IN_VEC_REG  */
    LEX_OUT_VEC_REG = 264,         /* LEX_OUT_VEC_REG  */
    LEX_COMPARE = 265,             /* LEX_COMPARE  */
    LEX_FLTFUNC = 266,             /* LEX_FLTFUNC  */
    LEX_ATAN2 = 267,               /* LEX_ATAN2  */
    LEX_POW = 268,                 /* LEX_POW  */
    LEX_FMOD = 269,                /* LEX_FMOD  */
    LEX_LEN = 270,                 /* LEX_LEN  */
    LEX_CROSS = 271,               /* LEX_CROSS  */
    LEX_DOT = 272,                 /* LEX_DOT  */
    LEX_NORMALIZE = 273,           /* LEX_NORMALIZE  */
    LEX_VEC3F = 274,               /* LEX_VEC3F  */
    LEX_ERROR = 275,               /* LEX_ERROR  */
    LEX_OR = 276,                  /* LEX_OR  */
    LEX_AND = 277,                 /* LEX_AND  */
    LEX_EQ = 278,                  /* LEX_EQ  */
    LEX_NEQ = 279,                 /* LEX_NEQ  */
    UNARY = 280                    /* UNARY  */
  };
  typedef enum so_evaltokentype so_evaltoken_kind_t;
#endif

/* Value type.  */
#if ! defined SO_EVALSTYPE && ! defined SO_EVALSTYPE_IS_DECLARED
union SO_EVALSTYPE
{

  int id;
  float value;
  char  reg;
  so_eval_node *node;


};
typedef union SO_EVALSTYPE SO_EVALSTYPE;
# define SO_EVALSTYPE_IS_TRIVIAL 1
# define SO_EVALSTYPE_IS_DECLARED 1
#endif


extern SO_EVALSTYPE so_evallval;


int so_evalparse (void);



/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_LEX_VALUE = 3,                  /* LEX_VALUE  */
  YYSYMBOL_LEX_TMP_FLT_REG = 4,            /* LEX_TMP_FLT_REG  */
  YYSYMBOL_LEX_IN_FLT_REG = 5,             /* LEX_IN_FLT_REG  */
  YYSYMBOL_LEX_OUT_FLT_REG = 6,            /* LEX_OUT_FLT_REG  */
  YYSYMBOL_LEX_TMP_VEC_REG = 7,            /* LEX_TMP_VEC_REG  */
  YYSYMBOL_LEX_IN_VEC_REG = 8,             /* LEX_IN_VEC_REG  */
  YYSYMBOL_LEX_OUT_VEC_REG = 9,            /* LEX_OUT_VEC_REG  */
  YYSYMBOL_LEX_COMPARE = 10,               /* LEX_COMPARE  */
  YYSYMBOL_LEX_FLTFUNC = 11,               /* LEX_FLTFUNC  */
  YYSYMBOL_LEX_ATAN2 = 12,                 /* LEX_ATAN2  */
  YYSYMBOL_LEX_POW = 13,                   /* LEX_POW  */
  YYSYMBOL_LEX_FMOD = 14,                  /* LEX_FMOD  */
  YYSYMBOL_LEX_LEN = 15,                   /* LEX_LEN  */
  YYSYMBOL_LEX_CROSS = 16,                 /* LEX_CROSS  */
  YYSYMBOL_LEX_DOT = 17,                   /* LEX_DOT  */
  YYSYMBOL_LEX_NORMALIZE = 18,             /* LEX_NORMALIZE  */
  YYSYMBOL_LEX_VEC3F = 19,                 /* LEX_VEC3F  */
  YYSYMBOL_20_ = 20,                       /* ','  */
  YYSYMBOL_21_ = 21,                       /* '['  */
  YYSYMBOL_22_ = 22,                       /* ']'  */
  YYSYMBOL_23_ = 23,                       /* '('  */
  YYSYMBOL_24_ = 24,                       /* ')'  */
  YYSYMBOL_25_ = 25,                       /* ';'  */
  YYSYMBOL_LEX_ERROR = 26,                 /* LEX_ERROR  */
  YYSYMBOL_27_ = 27,                       /* '='  */
  YYSYMBOL_28_ = 28,                       /* '?'  */
  YYSYMBOL_29_ = 29,                       /* ':'  */
  YYSYMBOL_LEX_OR = 30,                    /* LEX_OR  */
  YYSYMBOL_LEX_AND = 31,                   /* LEX_AND  */
  YYSYMBOL_LEX_EQ = 32,                    /* LEX_EQ  */
  YYSYMBOL_LEX_NEQ = 33,                   /* LEX_NEQ  */
  YYSYMBOL_34_ = 34,                       /* '+'  */
  YYSYMBOL_35_ = 35,                       /* '-'  */
  YYSYMBOL_36_ = 36,                       /* '*'  */
  YYSYMBOL_37_ = 37,                       /* '/'  */
  YYSYMBOL_38_ = 38,                       /* '%'  */
  YYSYMBOL_39_ = 39,                       /* '!'  */
  YYSYMBOL_UNARY = 40,                     /* UNARY  */
  YYSYMBOL_YYACCEPT = 41,                  /* $accept  */
  YYSYMBOL_input = 42,                     /* input  */
  YYSYMBOL_expression = 43,                /* expression  */
  YYSYMBOL_subexpression = 44,             /* subexpression  */
  YYSYMBOL_fltlhs = 45,                    /* fltlhs  */
  YYSYMBOL_veclhs = 46,                    /* veclhs  */
  YYSYMBOL_fltstatement = 47,              /* fltstatement  */
  YYSYMBOL_vecstatement = 48,              /* vecstatement  */
  YYSYMBOL_boolstatement = 49              /* boolstatement  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;


/* Second part of user prologue.  */

  static char * get_regname(char reg, int regtype);
  enum { REGTYPE_IN, REGTYPE_OUT, REGTYPE_TMP };
  static so_eval_node *root_node;
  static int so_evalerror(const char *);
  static int so_evallex(void);



#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_uint8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined SO_EVALSTYPE_IS_TRIVIAL && SO_EVALSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  12
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   500

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  41
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  9
/* YYNRULES -- Number of rules.  */
#define YYNRULES  61
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  158

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   280


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    39,     2,     2,     2,    38,     2,     2,
      23,    24,    36,    34,    20,    35,     2,    37,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    29,    25,
       2,    27,     2,    28,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    21,     2,    22,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    26,    30,    31,    32,    33,
      40
};

#if SO_EVALDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint8 yyrline[] =
{
       0,   114,   114,   117,   119,   122,   123,   124,   127,   128,
     129,   131,   135,   136,   139,   141,   143,   146,   147,   148,
     149,   150,   151,   153,   155,   158,   159,   160,   161,   162,
     164,   165,   166,   167,   169,   171,   173,   177,   179,   181,
     184,   185,   186,   187,   188,   189,   192,   193,   194,   197,
     198,   199,   200,   204,   205,   206,   207,   208,   209,   210,
     211,   212
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if SO_EVALDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "LEX_VALUE",
  "LEX_TMP_FLT_REG", "LEX_IN_FLT_REG", "LEX_OUT_FLT_REG",
  "LEX_TMP_VEC_REG", "LEX_IN_VEC_REG", "LEX_OUT_VEC_REG", "LEX_COMPARE",
  "LEX_FLTFUNC", "LEX_ATAN2", "LEX_POW", "LEX_FMOD", "LEX_LEN",
  "LEX_CROSS", "LEX_DOT", "LEX_NORMALIZE", "LEX_VEC3F", "','", "'['",
  "']'", "'('", "')'", "';'", "LEX_ERROR", "'='", "'?'", "':'", "LEX_OR",
  "LEX_AND", "LEX_EQ", "LEX_NEQ", "'+'", "'-'", "'*'", "'/'", "'%'", "'!'",
  "UNARY", "$accept", "input", "expression", "subexpression", "fltlhs",
  "veclhs", "fltstatement", "vecstatement", "boolstatement", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-35)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      85,   -35,   -35,    -2,     2,    31,     8,   -35,    11,    13,
      38,    40,   -35,    85,   121,   121,    23,    26,   -35,   -35,
     -35,   -35,   -35,    60,    61,    62,    67,    78,    79,    90,
      91,    92,    93,    96,    98,   121,   121,   121,    34,   463,
     -13,    34,   463,   -35,   -35,    58,    82,   120,   121,   121,
     121,   121,   121,   121,   121,   121,   121,    -8,   377,    56,
     -35,   -35,   -35,   121,   121,   121,   121,   121,   121,   121,
     121,   121,   121,   121,   121,   121,   121,   121,   121,   121,
     121,   121,    73,   109,   123,   138,   157,   168,   187,   391,
     353,   363,   405,   198,   -35,   -35,   -35,   342,   217,   -23,
     342,   342,    37,    37,   -35,   -35,   -35,   -35,   228,   443,
      72,    72,   -29,   -29,    -6,    -6,   248,   453,   111,   -35,
     -35,   -35,   -35,   -35,   121,   121,   121,   -35,   121,   121,
     -35,   121,   121,   121,   121,   121,   121,   121,   263,   278,
     293,   419,   433,   312,    34,   463,    34,   463,    34,   463,
     -35,   -35,   -35,   -35,   -35,   121,   327,   -35
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] =
{
       5,     8,     9,    12,    13,     0,     2,     4,     0,     0,
       0,     0,     1,     5,     0,     0,     0,     0,     3,    36,
      30,    32,    31,    49,    51,    50,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     6,     0,
       0,     0,     7,    10,    11,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      25,    46,    60,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    26,    47,    61,    57,     0,     0,
      53,    54,    17,    18,    20,    44,    19,    21,     0,     0,
      55,    56,    40,    41,    42,    43,     0,     0,    59,    58,
      33,    34,    35,    27,     0,     0,     0,    28,     0,     0,
      48,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    15,    38,    16,    39,    14,    37,
      22,    23,    24,    45,    29,     0,     0,    52
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
     -35,   -35,   -35,   130,   -35,   -35,   -14,    24,   -34
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
       0,     5,     6,     7,     8,     9,    41,    39,    40
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint8 yytable[] =
{
      38,    59,    63,    62,    63,    72,   133,    77,    78,    73,
      74,    75,    76,    77,    78,    79,    94,    80,    81,    10,
      64,    57,    60,    11,    65,    66,    67,    68,    69,    70,
      71,    12,    71,    13,    85,    86,    87,    88,    14,    42,
      15,    16,    93,    17,    63,    43,   118,   119,    44,    97,
      98,   100,   101,   102,   103,   104,   106,   107,   108,    58,
      61,    82,    64,   114,   115,   116,    65,    66,    67,    68,
      69,    70,    71,    69,    70,    71,    89,    90,    91,    92,
      96,    45,    46,    47,    79,    83,    80,    81,    99,     1,
      48,     2,     3,   105,     4,   120,   109,   110,   111,   112,
     113,    49,    50,   117,    73,    74,    75,    76,    77,    78,
     138,   139,   140,    51,    52,    53,    54,   143,   144,    55,
     146,    56,   148,    84,    19,    20,    21,    22,    23,    24,
      25,   121,    26,    27,    28,    29,    30,    31,    32,    33,
      34,   156,    81,    18,    35,   122,     0,     0,    63,     0,
       0,     0,   141,   142,     0,     0,    36,   145,     0,   147,
      37,   149,   123,     0,     0,     0,    64,    63,     0,     0,
      65,    66,    67,    68,    69,    70,    71,   124,    63,     0,
       0,     0,     0,     0,     0,    64,     0,     0,   125,    65,
      66,    67,    68,    69,    70,    71,    64,    63,     0,     0,
      65,    66,    67,    68,    69,    70,    71,   126,    63,     0,
       0,     0,     0,     0,     0,    64,     0,     0,   131,    65,
      66,    67,    68,    69,    70,    71,    64,    63,     0,     0,
      65,    66,    67,    68,    69,    70,    71,     0,    63,     0,
       0,     0,     0,     0,     0,    64,   132,     0,     0,    65,
      66,    67,    68,    69,    70,    71,    64,   134,    63,     0,
      65,    66,    67,    68,    69,    70,    71,     0,     0,     0,
       0,     0,     0,    63,     0,     0,    64,   136,     0,     0,
      65,    66,    67,    68,    69,    70,    71,   150,    63,     0,
       0,    64,     0,     0,     0,    65,    66,    67,    68,    69,
      70,    71,   151,    63,     0,     0,    64,     0,     0,     0,
      65,    66,    67,    68,    69,    70,    71,   152,     0,     0,
       0,    64,    63,     0,     0,    65,    66,    67,    68,    69,
      70,    71,   155,     0,     0,     0,     0,    63,     0,     0,
      64,     0,     0,     0,    65,    66,    67,    68,    69,    70,
      71,   157,    63,     0,     0,    64,     0,     0,     0,    65,
      66,    67,    68,    69,    70,    71,     0,     0,     0,     0,
       0,     0,     0,   128,    65,    66,    67,    68,    69,    70,
      71,    72,     0,   129,     0,    73,    74,    75,    76,    77,
      78,    72,     0,     0,     0,    73,    74,    75,    76,    77,
      78,    95,     0,     0,     0,    72,     0,     0,     0,    73,
      74,    75,    76,    77,    78,   127,     0,     0,     0,    72,
       0,     0,     0,    73,    74,    75,    76,    77,    78,   130,
       0,     0,     0,    72,     0,     0,     0,    73,    74,    75,
      76,    77,    78,   153,     0,     0,     0,    72,     0,     0,
       0,    73,    74,    75,    76,    77,    78,   154,     0,     0,
       0,    72,     0,     0,     0,    73,    74,    75,    76,    77,
      78,    72,   135,     0,     0,    73,    74,    75,    76,    77,
      78,    72,   137,     0,     0,    73,    74,    75,    76,    77,
      78,    72,     0,     0,     0,    73,    74,    75,    76,    77,
      78
};

static const yytype_int16 yycheck[] =
{
      14,    35,    10,    37,    10,    28,    29,    36,    37,    32,
      33,    34,    35,    36,    37,    28,    24,    30,    31,    21,
      28,    35,    36,    21,    32,    33,    34,    35,    36,    37,
      38,     0,    38,    25,    48,    49,    50,    51,    27,    15,
      27,     3,    56,     3,    10,    22,    80,    81,    22,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    35,
      36,     3,    28,    77,    78,    79,    32,    33,    34,    35,
      36,    37,    38,    36,    37,    38,    52,    53,    54,    55,
      24,    21,    21,    21,    28,     3,    30,    31,    64,     4,
      23,     6,     7,    69,     9,    22,    72,    73,    74,    75,
      76,    23,    23,    79,    32,    33,    34,    35,    36,    37,
     124,   125,   126,    23,    23,    23,    23,   131,   132,    23,
     134,    23,   136,     3,     3,     4,     5,     6,     7,     8,
       9,    22,    11,    12,    13,    14,    15,    16,    17,    18,
      19,   155,    31,    13,    23,    22,    -1,    -1,    10,    -1,
      -1,    -1,   128,   129,    -1,    -1,    35,   133,    -1,   135,
      39,   137,    24,    -1,    -1,    -1,    28,    10,    -1,    -1,
      32,    33,    34,    35,    36,    37,    38,    20,    10,    -1,
      -1,    -1,    -1,    -1,    -1,    28,    -1,    -1,    20,    32,
      33,    34,    35,    36,    37,    38,    28,    10,    -1,    -1,
      32,    33,    34,    35,    36,    37,    38,    20,    10,    -1,
      -1,    -1,    -1,    -1,    -1,    28,    -1,    -1,    20,    32,
      33,    34,    35,    36,    37,    38,    28,    10,    -1,    -1,
      32,    33,    34,    35,    36,    37,    38,    -1,    10,    -1,
      -1,    -1,    -1,    -1,    -1,    28,    29,    -1,    -1,    32,
      33,    34,    35,    36,    37,    38,    28,    29,    10,    -1,
      32,    33,    34,    35,    36,    37,    38,    -1,    -1,    -1,
      -1,    -1,    -1,    10,    -1,    -1,    28,    29,    -1,    -1,
      32,    33,    34,    35,    36,    37,    38,    24,    10,    -1,
      -1,    28,    -1,    -1,    -1,    32,    33,    34,    35,    36,
      37,    38,    24,    10,    -1,    -1,    28,    -1,    -1,    -1,
      32,    33,    34,    35,    36,    37,    38,    24,    -1,    -1,
      -1,    28,    10,    -1,    -1,    32,    33,    34,    35,    36,
      37,    38,    20,    -1,    -1,    -1,    -1,    10,    -1,    -1,
      28,    -1,    -1,    -1,    32,    33,    34,    35,    36,    37,
      38,    24,    10,    -1,    -1,    28,    -1,    -1,    -1,    32,
      33,    34,    35,    36,    37,    38,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    20,    32,    33,    34,    35,    36,    37,
      38,    28,    -1,    20,    -1,    32,    33,    34,    35,    36,
      37,    28,    -1,    -1,    -1,    32,    33,    34,    35,    36,
      37,    24,    -1,    -1,    -1,    28,    -1,    -1,    -1,    32,
      33,    34,    35,    36,    37,    24,    -1,    -1,    -1,    28,
      -1,    -1,    -1,    32,    33,    34,    35,    36,    37,    24,
      -1,    -1,    -1,    28,    -1,    -1,    -1,    32,    33,    34,
      35,    36,    37,    24,    -1,    -1,    -1,    28,    -1,    -1,
      -1,    32,    33,    34,    35,    36,    37,    24,    -1,    -1,
      -1,    28,    -1,    -1,    -1,    32,    33,    34,    35,    36,
      37,    28,    29,    -1,    -1,    32,    33,    34,    35,    36,
      37,    28,    29,    -1,    -1,    32,    33,    34,    35,    36,
      37,    28,    -1,    -1,    -1,    32,    33,    34,    35,    36,
      37
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,     4,     6,     7,     9,    42,    43,    44,    45,    46,
      21,    21,     0,    25,    27,    27,     3,     3,    44,     3,
       4,     5,     6,     7,     8,     9,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    23,    35,    39,    47,    48,
      49,    47,    48,    22,    22,    21,    21,    21,    23,    23,
      23,    23,    23,    23,    23,    23,    23,    47,    48,    49,
      47,    48,    49,    10,    28,    32,    33,    34,    35,    36,
      37,    38,    28,    32,    33,    34,    35,    36,    37,    28,
      30,    31,     3,     3,     3,    47,    47,    47,    47,    48,
      48,    48,    48,    47,    24,    24,    24,    47,    47,    48,
      47,    47,    47,    47,    47,    48,    47,    47,    47,    48,
      48,    48,    48,    48,    47,    47,    47,    48,    49,    49,
      22,    22,    22,    24,    20,    20,    20,    24,    20,    20,
      24,    20,    29,    29,    29,    29,    29,    29,    47,    47,
      47,    48,    48,    47,    47,    48,    47,    48,    47,    48,
      24,    24,    24,    24,    24,    20,    47,    24
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    41,    42,    43,    43,    44,    44,    44,    45,    45,
      45,    45,    46,    46,    47,    47,    47,    47,    47,    47,
      47,    47,    47,    47,    47,    47,    47,    47,    47,    47,
      47,    47,    47,    47,    47,    47,    47,    48,    48,    48,
      48,    48,    48,    48,    48,    48,    48,    48,    48,    48,
      48,    48,    48,    49,    49,    49,    49,    49,    49,    49,
      49,    49
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     3,     1,     0,     3,     3,     1,     1,
       4,     4,     1,     1,     5,     5,     5,     3,     3,     3,
       3,     3,     6,     6,     6,     2,     3,     4,     4,     6,
       1,     1,     1,     4,     4,     4,     1,     5,     5,     5,
       3,     3,     3,     3,     3,     6,     2,     3,     4,     1,
       1,     1,     8,     3,     3,     3,     3,     3,     3,     3,
       2,     3
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = SO_EVALEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == SO_EVALEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use SO_EVALerror or SO_EVALUNDEF. */
#define YYERRCODE SO_EVALUNDEF


/* Enable debugging if requested.  */
#if SO_EVALDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)]);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !SO_EVALDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !SO_EVALDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YY_USE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  switch (yykind)
    {
    case YYSYMBOL_expression: /* expression  */
            { so_eval_delete(((*yyvaluep).node)); }
        break;

    case YYSYMBOL_subexpression: /* subexpression  */
            { so_eval_delete(((*yyvaluep).node)); }
        break;

    case YYSYMBOL_fltlhs: /* fltlhs  */
            { so_eval_delete(((*yyvaluep).node)); }
        break;

    case YYSYMBOL_veclhs: /* veclhs  */
            { so_eval_delete(((*yyvaluep).node)); }
        break;

    case YYSYMBOL_fltstatement: /* fltstatement  */
            { so_eval_delete(((*yyvaluep).node)); }
        break;

    case YYSYMBOL_vecstatement: /* vecstatement  */
            { so_eval_delete(((*yyvaluep).node)); }
        break;

    case YYSYMBOL_boolstatement: /* boolstatement  */
            { so_eval_delete(((*yyvaluep).node)); }
        break;

      default:
        break;
    }
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* Lookahead token kind.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;




/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = SO_EVALEMPTY; /* Cause a token to be read.  */

  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == SO_EVALEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= SO_EVALEOF)
    {
      yychar = SO_EVALEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == SO_EVALerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = SO_EVALUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = SO_EVALEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* input: expression  */
                           { root_node = (yyvsp[0].node); }
    break;

  case 3: /* expression: expression ';' subexpression  */
              { (yyval.node) = so_eval_create_binary(ID_SEPARATOR, (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 4: /* expression: subexpression  */
                              { (yyval.node) = (yyvsp[0].node); }
    break;

  case 5: /* subexpression: %empty  */
                { (yyval.node) = NULL; }
    break;

  case 6: /* subexpression: fltlhs '=' fltstatement  */
                                        { (yyval.node) = so_eval_create_binary(ID_ASSIGN_FLT, (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 7: /* subexpression: veclhs '=' vecstatement  */
                                        { (yyval.node) = so_eval_create_binary(ID_ASSIGN_VEC, (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 8: /* fltlhs: LEX_TMP_FLT_REG  */
                                { (yyval.node) = so_eval_create_reg(get_regname((yyvsp[0].reg), REGTYPE_TMP)); }
    break;

  case 9: /* fltlhs: LEX_OUT_FLT_REG  */
                                { (yyval.node) = so_eval_create_reg(get_regname((yyvsp[0].reg), REGTYPE_OUT)); }
    break;

  case 10: /* fltlhs: LEX_TMP_VEC_REG '[' LEX_VALUE ']'  */
              { (yyval.node) = so_eval_create_reg_comp(get_regname((yyvsp[-3].reg), REGTYPE_TMP), (int) (yyvsp[-1].value)); }
    break;

  case 11: /* fltlhs: LEX_OUT_VEC_REG '[' LEX_VALUE ']'  */
              { (yyval.node) = so_eval_create_reg_comp(get_regname((yyvsp[-3].reg), REGTYPE_OUT), (int) (yyvsp[-1].value)); }
    break;

  case 12: /* veclhs: LEX_TMP_VEC_REG  */
                                { (yyval.node) = so_eval_create_reg(get_regname((yyvsp[0].reg), REGTYPE_TMP));}
    break;

  case 13: /* veclhs: LEX_OUT_VEC_REG  */
                                { (yyval.node) = so_eval_create_reg(get_regname((yyvsp[0].reg), REGTYPE_OUT));}
    break;

  case 14: /* fltstatement: boolstatement '?' fltstatement ':' fltstatement  */
              { (yyval.node) = so_eval_create_ternary(ID_FLT_COND, (yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 15: /* fltstatement: fltstatement '?' fltstatement ':' fltstatement  */
              { (yyval.node) = so_eval_create_ternary(ID_FLT_COND, so_eval_create_unary(ID_TEST_FLT, (yyvsp[-4].node)), (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 16: /* fltstatement: vecstatement '?' fltstatement ':' fltstatement  */
              { (yyval.node) = so_eval_create_ternary(ID_FLT_COND, so_eval_create_unary(ID_TEST_VEC, (yyvsp[-4].node)), (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 17: /* fltstatement: fltstatement '+' fltstatement  */
                                              { (yyval.node) = so_eval_create_binary(ID_ADD, (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 18: /* fltstatement: fltstatement '-' fltstatement  */
                                              { (yyval.node) = so_eval_create_binary(ID_SUB, (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 19: /* fltstatement: fltstatement '/' fltstatement  */
                                              { (yyval.node) = so_eval_create_binary(ID_DIV, (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 20: /* fltstatement: fltstatement '*' fltstatement  */
                                              { (yyval.node) = so_eval_create_binary(ID_MUL, (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 21: /* fltstatement: fltstatement '%' fltstatement  */
                                              { (yyval.node) = so_eval_create_binary(ID_FMOD, (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 22: /* fltstatement: LEX_ATAN2 '(' fltstatement ',' fltstatement ')'  */
              { (yyval.node) = so_eval_create_binary(ID_ATAN2, (yyvsp[-3].node), (yyvsp[-1].node)); }
    break;

  case 23: /* fltstatement: LEX_POW '(' fltstatement ',' fltstatement ')'  */
              { (yyval.node) = so_eval_create_binary(ID_POW, (yyvsp[-3].node), (yyvsp[-1].node)); }
    break;

  case 24: /* fltstatement: LEX_FMOD '(' fltstatement ',' fltstatement ')'  */
              { (yyval.node) = so_eval_create_binary(ID_FMOD, (yyvsp[-3].node), (yyvsp[-1].node)); }
    break;

  case 25: /* fltstatement: '-' fltstatement  */
                                             { (yyval.node) = so_eval_create_unary(ID_NEG, (yyvsp[0].node)); }
    break;

  case 26: /* fltstatement: '(' fltstatement ')'  */
                                     { (yyval.node) = (yyvsp[-1].node); }
    break;

  case 27: /* fltstatement: LEX_FLTFUNC '(' fltstatement ')'  */
                                                 { (yyval.node) = so_eval_create_unary((yyvsp[-3].id), (yyvsp[-1].node));}
    break;

  case 28: /* fltstatement: LEX_LEN '(' vecstatement ')'  */
                                             { (yyval.node) = so_eval_create_unary(ID_LEN, (yyvsp[-1].node));}
    break;

  case 29: /* fltstatement: LEX_DOT '(' vecstatement ',' vecstatement ')'  */
              { (yyval.node) = so_eval_create_binary(ID_DOT, (yyvsp[-3].node), (yyvsp[-1].node)); }
    break;

  case 30: /* fltstatement: LEX_TMP_FLT_REG  */
                                { (yyval.node) = so_eval_create_reg(get_regname((yyvsp[0].reg), REGTYPE_TMP));}
    break;

  case 31: /* fltstatement: LEX_OUT_FLT_REG  */
                                { (yyval.node) = so_eval_create_reg(get_regname((yyvsp[0].reg), REGTYPE_OUT));}
    break;

  case 32: /* fltstatement: LEX_IN_FLT_REG  */
                               { (yyval.node) = so_eval_create_reg(get_regname((yyvsp[0].reg), REGTYPE_IN));}
    break;

  case 33: /* fltstatement: LEX_TMP_VEC_REG '[' LEX_VALUE ']'  */
              { (yyval.node) = so_eval_create_reg_comp(get_regname((yyvsp[-3].reg), REGTYPE_TMP), (int) (yyvsp[-1].value));}
    break;

  case 34: /* fltstatement: LEX_IN_VEC_REG '[' LEX_VALUE ']'  */
              { (yyval.node) = so_eval_create_reg_comp(get_regname((yyvsp[-3].reg), REGTYPE_IN), (int) (yyvsp[-1].value));}
    break;

  case 35: /* fltstatement: LEX_OUT_VEC_REG '[' LEX_VALUE ']'  */
              { (yyval.node) = so_eval_create_reg_comp(get_regname((yyvsp[-3].reg), REGTYPE_OUT), (int) (yyvsp[-1].value));}
    break;

  case 36: /* fltstatement: LEX_VALUE  */
              { (yyval.node) = so_eval_create_flt_val((yyvsp[0].value)); }
    break;

  case 37: /* vecstatement: boolstatement '?' vecstatement ':' vecstatement  */
              { (yyval.node) = so_eval_create_ternary(ID_VEC_COND, (yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 38: /* vecstatement: fltstatement '?' vecstatement ':' vecstatement  */
              { (yyval.node) = so_eval_create_ternary(ID_VEC_COND, so_eval_create_unary(ID_TEST_FLT, (yyvsp[-4].node)), (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 39: /* vecstatement: vecstatement '?' vecstatement ':' vecstatement  */
              { (yyval.node) = so_eval_create_ternary(ID_VEC_COND, so_eval_create_unary(ID_TEST_VEC, (yyvsp[-4].node)), (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 40: /* vecstatement: vecstatement '+' vecstatement  */
                                               { (yyval.node) = so_eval_create_binary(ID_ADD_VEC, (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 41: /* vecstatement: vecstatement '-' vecstatement  */
                                              { (yyval.node) = so_eval_create_binary(ID_SUB_VEC, (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 42: /* vecstatement: vecstatement '*' fltstatement  */
                                              { (yyval.node) = so_eval_create_binary(ID_MUL_VEC_FLT, (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 43: /* vecstatement: vecstatement '/' fltstatement  */
                                              { (yyval.node) = so_eval_create_binary(ID_DIV_VEC_FLT, (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 44: /* vecstatement: fltstatement '*' vecstatement  */
                                              { (yyval.node) = so_eval_create_binary(ID_MUL_VEC_FLT, (yyvsp[0].node), (yyvsp[-2].node)); }
    break;

  case 45: /* vecstatement: LEX_CROSS '(' vecstatement ',' vecstatement ')'  */
              { (yyval.node) = so_eval_create_binary(ID_CROSS, (yyvsp[-3].node), (yyvsp[-1].node)); }
    break;

  case 46: /* vecstatement: '-' vecstatement  */
                                            { (yyval.node) = so_eval_create_unary(ID_NEG_VEC, (yyvsp[0].node)); }
    break;

  case 47: /* vecstatement: '(' vecstatement ')'  */
                                     { (yyval.node) = (yyvsp[-1].node); }
    break;

  case 48: /* vecstatement: LEX_NORMALIZE '(' vecstatement ')'  */
              { (yyval.node) = so_eval_create_unary(ID_NORMALIZE, (yyvsp[-1].node)); }
    break;

  case 49: /* vecstatement: LEX_TMP_VEC_REG  */
                                { (yyval.node) = so_eval_create_reg(get_regname((yyvsp[0].reg), REGTYPE_TMP));}
    break;

  case 50: /* vecstatement: LEX_OUT_VEC_REG  */
                                { (yyval.node) = so_eval_create_reg(get_regname((yyvsp[0].reg), REGTYPE_OUT));}
    break;

  case 51: /* vecstatement: LEX_IN_VEC_REG  */
                               { (yyval.node) = so_eval_create_reg(get_regname((yyvsp[0].reg), REGTYPE_IN));}
    break;

  case 52: /* vecstatement: LEX_VEC3F '(' fltstatement ',' fltstatement ',' fltstatement ')'  */
              { (yyval.node) = so_eval_create_ternary(ID_VEC3F, (yyvsp[-5].node), (yyvsp[-3].node), (yyvsp[-1].node)); }
    break;

  case 53: /* boolstatement: fltstatement LEX_EQ fltstatement  */
                                                 { (yyval.node) = so_eval_create_binary(ID_EQ, (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 54: /* boolstatement: fltstatement LEX_NEQ fltstatement  */
                                                  { (yyval.node) = so_eval_create_binary(ID_NEQ, (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 55: /* boolstatement: vecstatement LEX_EQ vecstatement  */
                                                 { (yyval.node) = so_eval_create_binary(ID_EQ, (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 56: /* boolstatement: vecstatement LEX_NEQ vecstatement  */
                                                  { (yyval.node) = so_eval_create_binary(ID_NEQ, (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 57: /* boolstatement: fltstatement LEX_COMPARE fltstatement  */
                                                      { (yyval.node) = so_eval_create_binary((yyvsp[-1].id), (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 58: /* boolstatement: boolstatement LEX_AND boolstatement  */
                                                    { (yyval.node) = so_eval_create_binary(ID_AND, (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 59: /* boolstatement: boolstatement LEX_OR boolstatement  */
                                                   { (yyval.node) = so_eval_create_binary(ID_OR, (yyvsp[-2].node), (yyvsp[0].node)); }
    break;

  case 60: /* boolstatement: '!' boolstatement  */
                                              { (yyval.node) = so_eval_create_unary(ID_NOT, (yyvsp[0].node)); }
    break;

  case 61: /* boolstatement: '(' boolstatement ')'  */
                                      { (yyval.node) = (yyvsp[-1].node); }
    break;



      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == SO_EVALEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= SO_EVALEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == SO_EVALEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = SO_EVALEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != SO_EVALEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}



/*
 * Creates a register name from the register type and register char.
 *
 * Note: don't "const" the return type, as that will trigger a bug in
 * Microsoft Visual C++ v6.0. 20000606 mortene.
*/
static char *
get_regname(char reg, int regtype)
{
  static char buf[3];
  buf[2] = 0;

  if (regtype != REGTYPE_IN) {
    if (regtype == REGTYPE_TMP) buf[0] = 't';
    else if (regtype == REGTYPE_OUT) buf[0] = 'o';
    buf[1] = reg;
    buf[2] = 0;
  }
  else {
    buf[0] = reg;
    buf[1] = 0;
  }
  return buf;
}



/* Define YY_DECL before including so_eval.ic so the flex-generated file skips
   its #ifndef YY_DECL block, which would otherwise emit a redundant
   'extern int yylex()' forward declaration that conflicts with the static
   so_evallex declaration in the bison-generated prologue above. */
#ifndef YY_DECL
#define YY_DECL int yylex(void)
#endif
#include "so_eval.ic" /* our lexical scanner */

/* some very simple error handling for now :) */
static std::mutex evaluator_parse_mutex;
static thread_local char *myerrorptr;
static thread_local char myerrorbuf[512];

/*
 * parse the text string into a tree structure.
 */
so_eval_node *
so_eval_parse(const char *buffer)
{
  const std::lock_guard<std::mutex> guard(evaluator_parse_mutex);
  /* FIXME: better error handling is obviously needed */
  YY_BUFFER_STATE state;
  myerrorptr = NULL;
  root_node = NULL;
  state = so_eval_scan_string(buffer); /* flex routine */
  so_evalparse(); /* start parsing */
  so_eval_delete_buffer(state); /* flex routine */
  if (myerrorptr) return NULL;
  return root_node;
}

/*
 * Returns current error message or NULL if none.
 *
 * Note: don't "const" the return type, as that will trigger a bug in
 * Microsoft Visual C++ v6.0. 20000606 mortene.
 */
char *
so_eval_error(void)
{
  return myerrorptr;
}

/*
 * Called by bison parser upon lexical/syntax error.
 */
int
so_evalerror(const char *myerr)
{
  strncpy(myerrorbuf, myerr, 512);
  myerrorbuf[511] = 0; /* just in case string was too long */
  myerrorptr = myerrorbuf; /* signal error */
  root_node = NULL;
  return 0;
}
