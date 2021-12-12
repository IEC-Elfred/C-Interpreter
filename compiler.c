/*
 * @Description: 
 * @Version: 2.0
 * @Autor: elfred
 * @Date: 2021-11-29 21:09:58
 */
#include <fcntl.h>  // for open
#include <unistd.h> // for close
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#define int long long
/*


+------------------+
|    stack   |     |      high address
|    ...     v     |
|                  |
|                  |
|                  |
|                  |
|    ...     ^     |
|    heap    |     |
+------------------+
| bss  segment     |
+------------------+
| data segment     |
+------------------+
| text segment     |      low address
+------------------+ 
*/

// 代码段（text）用于存放代码（指令）。
// 数据段（data）用于存放初始化了的数据，如int i = 10;，就需要存放到数据段中。
// 未初始化数据段（bss）用于存放未初始化的数据，如 int i[1000];，因为不关心其中的真正数值，所以单独存放可以节省空间，减少程序的体积。
// 栈（stack）用于处理函数调用相关的数据，如调用帧（calling frame）或是函数的局部变量等。
// 堆（heap）用于为程序动态分配内存。

int token;                    //current token
char *src, *old_src;          //pointer to source code string
int poolsize;                 //default size of text/date/stack
int line;                     //line number
int *text,                    // text segment
    *old_text,                // for dump text segment
    *stack;                   // stack
char *data;                   // data segment
int *pc, *bp, *sp, ax, cycle; // virtual machine registers
int token_val;                // value of current token (mainly for number)
int *current_id,              // current parsed ID
    *symbols;                 // symbol table
char *line = NULL;
char *src = NULL;
// types of variable/function
enum
{
    CHAR,
    INT,
    PTR
};
int *idmain;   // the `main` function
int basetype;  // the type of a declaration, make it global for convenience
int expr_type; // the type of an expression
int index_of_bp;

struct identifier
{
    int token;
    int hash;
    char *name;
};

enum
{
    Token,
    Hash,
    Name,
    Type,
    Class,
    Value,
    BType,
    BClass,
    BValue,
    IdSize
};

// instructions
enum
{
    LEA,
    IMM,
    JMP,
    CALL,
    JZ,
    JNZ,
    ENT,
    ADJ,
    LEV,
    LI,
    LC,
    SI,
    SC,
    PUSH,
    OR,
    XOR,
    AND,
    EQ,
    NE,
    LT,
    GT,
    LE,
    GE,
    SHL,
    SHR,
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    OPEN,
    READ,
    CLOS,
    PRTF,
    MALC,
    MSET,
    MCMP,
    EXIT
};

// tokens and classes (operators last and in precedence order)
enum
{
    Num = 128,
    Fun,
    Sys,
    Glo,
    Loc,
    Id,
    Char,
    Else,
    Enum,
    If,
    Int,
    Return,
    Sizeof,
    While,
    Assign,
    Cond,
    Lor,
    Lan,
    Or,
    Xor,
    And,
    Eq,
    Ne,
    Lt,
    Gt,
    Le,
    Ge,
    Shl,
    Shr,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Inc,
    Dec,
    Brak
};

void next()
{
    char *last_pos;
    int hash;

    while (token = *src)
    {
        ++src;

        // parse token here
        if (token == '\n')
        {
            ++line;
        }
        else if (token == '#')
        {
            // skip macro, because we will not support it
            while (*src != 0 && *src != '\n')
            {
                src++;
            }
        }
        else if ((token >= 'a' && token <= 'z') || (token >= 'A' && token <= 'Z') || (token == '_'))
        {

            // parse identifier
            last_pos = src - 1;
            hash = token;

            while ((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') || (*src >= '0' && *src <= '9') || (*src == '_'))
            {
                hash = hash * 147 + *src;
                src++;
            }

            // look for existing identifier, linear search
            current_id = symbols;
            while (current_id[Token])
            {
                if (current_id[Hash] == hash && !memcmp((char *)current_id[Name], last_pos, src - last_pos))
                {
                    //found one, return
                    token = current_id[Token];
                    return;
                }
                current_id = current_id + IdSize;
            }

            // store new ID
            current_id[Name] = (int)last_pos;
            current_id[Hash] = hash;
            //todo
            token = current_id[Token] = Id;
            return;
        }
        else if (token >= '0' && token <= '9')
        {
            // parse number, three kinds: dec(123) hex(0x123) oct(017)
            token_val = token - '0';
            if (token_val > 0)
            {
                // dec, starts with [1-9]
                while (*src >= '0' && *src <= '9')
                {
                    token_val = token_val * 10 + *src++ - '0';
                }
            }
            else
            {
                // starts with 0
                if (*src == 'x' || *src == 'X')
                {
                    //hex
                    token = *++src;
                    while ((token >= '0' && token <= '9') || (token >= 'a' && token <= 'f') || (token >= 'A' && token <= 'F'))
                    {
                        //for ASCII
                        token_val = token_val * 16 + (token & 15) + (token >= 'A' ? 9 : 0);
                        token = *++src;
                    }
                }
                else
                {
                    // oct
                    while (*src >= '0' && *src <= '7')
                    {
                        token_val = token_val * 8 + *src++ - '0';
                    }
                }
            }

            token = Num;
            return;
        }
        else if (token == '"' || token == '\'')
        {
            // parse string literal, currently, the only supported escape
            // character is '\n', store the string literal into data.
            last_pos = data;
            //not match " / ' , or not at the end
            while (*src != 0 && *src != token)
            {
                token_val = *src++;
                if (token_val == '\\')
                {
                    // escape character
                    token_val = *src++;
                    if (token_val == 'n')
                    {
                        token_val = '\n';
                    }
                }
                //string
                if (token == '"')
                {
                    *data++ = token_val;
                }
            }

            src++;
            // if it is a single character, return Num token
            if (token == '"')
            {
                token_val = (int)last_pos;
            }
            else
            {
                token = Num;
            }

            return;
        }
        else if (token == '/')
        {
            if (*src == '/')
            {
                // skip comments
                while (*src != 0 && *src != '\n')
                {
                    ++src;
                }
            }
            else
            {
                // divide operator
                token = Div;
                return;
            }
        }
        else if (token == '=')
        {
            // parse '==' and '='
            if (*src == '=')
            {
                src++;
                token = Eq;
            }
            else
            {
                token = Assign;
            }
            return;
        }
        else if (token == '+')
        {
            // parse '+' and '++'
            if (*src == '+')
            {
                src++;
                token = Inc;
            }
            else
            {
                token = Add;
            }
            return;
        }
        else if (token == '-')
        {
            // parse '-' and '--'
            if (*src == '-')
            {
                src++;
                token = Dec;
            }
            else
            {
                token = Sub;
            }
            return;
        }
        else if (token == '!')
        {
            // parse '!='
            if (*src == '=')
            {
                src++;
                token = Ne;
            }
            return;
        }
        else if (token == '<')
        {
            // parse '<=', '<<' or '<'
            if (*src == '=')
            {
                src++;
                token = Le;
            }
            else if (*src == '<')
            {
                src++;
                token = Shl;
            }
            else
            {
                token = Lt;
            }
            return;
        }
        else if (token == '>')
        {
            // parse '>=', '>>' or '>'
            if (*src == '=')
            {
                src++;
                token = Ge;
            }
            else if (*src == '>')
            {
                src++;
                token = Shr;
            }
            else
            {
                token = Gt;
            }
            return;
        }
        else if (token == '|')
        {
            // parse '|' or '||'
            if (*src == '|')
            {
                src++;
                token = Lor;
            }
            else
            {
                token = Or;
            }
            return;
        }
        else if (token == '&')
        {
            // parse '&' and '&&'
            if (*src == '&')
            {
                src++;
                token = Lan;
            }
            else
            {
                token = And;
            }
            return;
        }
        else if (token == '^')
        {
            token = Xor;
            return;
        }
        else if (token == '%')
        {
            token = Mod;
            return;
        }
        else if (token == '*')
        {
            token = Mul;
            return;
        }
        else if (token == '[')
        {
            token = Brak;
            return;
        }
        else if (token == '?')
        {
            token = Cond;
            return;
        }
        else if (token == '~' || token == ';' || token == '{' || token == '}' || token == '(' || token == ')' || token == ']' || token == ',' || token == ':')
        {
            // directly return the character as token;
            return;
        }
    }
    return;
}

void match(int tk)
{
    if (token != tk)
    {
        printf("expected token: %d(%c), got: %d(%c)\n", tk, tk, token, token);
        exit(-1);
    }
    next();
}

void expression(int level)
{
    //do something
}

void global_declaration()
{
    // global_declaration ::= enum_decl | variable_decl | function_decl
    //
    // enum_decl ::= 'enum' [id] '{' id ['=' 'num'] {',' id ['=' 'num'} '}'
    //
    // variable_decl ::= type {'*'} id { ',' {'*'} id } ';'
    //
    // function_decl ::= type {'*'} id '(' parameter_decl ')' '{' body_decl '}'

    int type; // tmp, actual type for variable
    int i;    // tmp

    basetype = INT;

    // parse enum, this should be treated alone.
    if (token == Enum)
    {
        // enum [id] { a = 10, b = 20, ... }
        match(Enum);
        if (token != '{')
        {
            match(Id); // skip the [id] part
        }
        if (token == '{')
        {
            // parse the assign part
            match('{');
            enum_declaration();
            match('}');
        }

        match(';');
        return;
    }

    // parse type information
    if (token == Int)
    {
        match(Int);
    }
    else if (token == Char)
    {
        match(Char);
        basetype = CHAR;
    }

    // parse the comma seperated variable declaration.
    while (token != ';' && token != '}')
    {
        type = basetype;
        // parse pointer type, note that there may exist `int ****x;`
        while (token == Mul)
        {
            match(Mul);
            type = type + PTR;
        }

        if (token != Id)
        {
            // invalid declaration
            printf("%d: bad global declaration\n", line);
            exit(-1);
        }
        if (current_id[Class])
        {
            // identifier exists
            printf("%d: duplicate global declaration\n", line);
            exit(-1);
        }
        match(Id);
        current_id[Type] = type;

        if (token == '(')
        {
            current_id[Class] = Fun;
            current_id[Value] = (int)(text + 1); // the memory address of function
            function_declaration();
        }
        else
        {
            // variable declaration
            current_id[Class] = Glo;       // global variable
            current_id[Value] = (int)data; // assign memory address
            data = data + sizeof(int);
        }

        if (token == ',')
        {
            match(',');
        }
    }
    next();
}

void function_parameter()
{
    int type;
    int params;
    params = 0;
    while (token != ')')
    {
        // ①

        // int name, ...
        type = INT;
        if (token = Int)
        {
            match(Int);
        }
        else if (token = Char)
        {
            type = CHAR;
            match(Char);
        }

        //pointer type
        while (token == Mul)
        {
            match(Mul);
        }
        // parameter name
        if (token != Id)
        {
            printf("%d: bad parameter declaration\n", line);
            exit(-1);
        }
        if (current_id[Class] == Loc)
        {
            printf("%d: duplicate parameter declaration\n", line);
            exit(-1);
        }
        match(Id);

        //②
        // store the local variable
        current_id[BClass] = current_id[Class];
        current_id[Class] = Loc;
        current_id[BType] = current_id[Type];
        current_id[Type] = type;
        current_id[BValue] = current_id[Value];
        current_id[Value] = params++;

        if (token == ',')
        {
            match(',');
        }
    }
    index_of_bp = params + 1;
}

void function_body()
{
    // type func_name (...) {...}
    //                   -->|   |<--

    // ... {
    // 1. local declarations
    // 2. statements
    // }
    int pos_local; //position of local variables on stack
    int type;
    pos_local = index_of_bp;

    // ①
    while (token == Int || token == Char)
    {
        //local variable declaration, just like global ones
        basetype = (token == Int) ? Int : CHAR;
        match(token);
        while (token != ';')
        {
            type = basetype;
            while (token == Mul)
            {
                match(Mul);
                type = type + PTR;
            }
            if (token != Id)
            {
                // invalid declaration
                printf("%d: bad local declaration\n", line);
                exit(-1);
            }
            if (current_id[Class] == Loc)
            {
                // identifier exists
                printf("%d: duplicate local declaration\n", line);
                exit(-1);
            }
            match(Id);
            // store the local variable
            current_id[BClass] = current_id[Class];
            current_id[Class] = Loc;
            current_id[BType] = current_id[Type];
            current_id[Type] = type;
            current_id[BValue] = current_id[Value];
            current_id[Value] = ++pos_local; // index of current parameter
            if (token == ',')
            {
                match(',');
            }
        }
        match(';');
    }
    // ②
    // save the stack size for local variables
    *++text = ENT;
    *++text = pos_local - index_of_bp;

    // statements
    while (token != '}')
    {
        statement();
    }

    // emit code for leaving the sub function
    *++text = LEV;
}

void function_declare()
{
    // type func_name (...) {...}
    //               | this part
    match('(');
    function_parameter();
    match(')');
    match('{');
    function_body();
    //match('}');                 //  ①

    // ②
    // unwind local variable declarations for all local variables.
    current_id = symbols;
    while (current_id[Token])
    {
        if (current_id[Class] == Loc)
        {
            current_id[Class] = current_id[BClass];
            current_id[Type] = current_id[BType];
            current_id[Value] = current_id[BValue];
        }
        current_id = current_id + IdSize;
    }
}

void program()
{
    // get next token
    next();
    while (token > 0)
    {
        global_declaration();
    }
}

int eval()
{
    int op, *tmp;
    while (1)
    {
        op = *pc++;
        switch (op)
        {
        case IMM:
            ax = *pc++; // load immediate value to ax
            break;
        case LC:
            ax = *(char *)ax; // load character to ax, address in ax
            break;
        case LI:
            ax = *(int *)ax; // load integer to ax, address in ax
            break;
        case SC:
            *(char *)*sp++ = ax; // save character to address, value in ax, address on stack
            break;
        case SI:
            *(int *)*sp++ = ax; // save integer to address, value in ax, address on stack
            break;
        case PUSH:
            *--sp = ax; // push the value of ax onto the stack
            break;
        case JMP:
            pc = (int *)*pc; // jump to the address,the next address is saved in *pc
            break;
        case JZ:
            pc = ax ? pc + 1 : (int *)*pc; // jump if ax is zero
            break;
        case JNZ:
            pc = ax ? (int *)*pc : pc + 1; // jump if ax is not zero
            break;
        case CALL:
            *--sp = (int)(pc + 1); // call subroutine
            break;
            // case RET:
            //     pc = (int *)*sp++;
            //     break;
            /*
    sub_function(arg1, arg2, arg3);

        |    ....       | high address
        +---------------+
        | arg: 1        |    new_bp + 4
        +---------------+
        | arg: 2        |    new_bp + 3
        +---------------+
        | arg: 3        |    new_bp + 2
        +---------------+
        |return address |    new_bp + 1
        +---------------+
        | old BP        | <- new BP
        +---------------+
        | local var 1   |    new_bp - 1
        +---------------+
        | local var 2   |    new_bp - 2
        +---------------+
        |    ....       |  low address
*/
        case ENT:
            *--sp = (int)bp; //make new stack frame
            bp = sp;
            sp = sp - *pc++;
            break;
        case ADJ:
            sp = sp + *pc++; //add esp, <size>，remove arguments from frame
            break;
        case LEV:
            sp = bp; //restore call frame and PC . It has contain RET func
            bp = (int *)*sp++;
            pc = (int *)*sp++;
            break;
        case LEA:
            ax = (int)(bp + *pc++); //load address for arguments.LEA <offset>
            break;
        case OR:
            ax = *sp++ | ax;
            break;
        case XOR:
            ax = *sp++ ^ ax;
            break;
        case AND:
            ax = *sp++ & ax;
            break;
        case EQ:
            ax = *sp++ == ax;
            break;
        case NE:
            ax = *sp++ != ax;
            break;
        case LT:
            ax = *sp++ < ax;
            break;
        case LE:
            ax = *sp++ <= ax;
            break;
        case GT:
            ax = *sp++ > ax;
            break;
        case GE:
            ax = *sp++ >= ax;
            break;
        case SHL:
            ax = *sp++ << ax;
            break;
        case SHR:
            ax = *sp++ >> ax;
            break;
        case ADD:
            ax = *sp++ + ax;
            break;
        case SUB:
            ax = *sp++ - ax;
            break;
        case MUL:
            ax = *sp++ * ax;
            break;
        case DIV:
            ax = *sp++ / ax;
            break;
        case MOD:
            ax = *sp++ % ax;
            break;
        case EXIT:
            printf("exit(%d)", *sp);
            return *sp;
            break;
        case OPEN:
            ax = open((char *)sp[1], sp[0]);
            break;
        case CLOS:
            ax = close(*sp);
            break;
        case READ:
            ax = read(sp[2], (char *)sp[1], *sp);
            break;
        case PRTF:
            tmp = sp + pc[1];
            ax = printf((char *)tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-5], tmp[-6]);
            break;
        case MALC:
            ax = (int)malloc(*sp);
            break;
        case MSET:
            ax = (int)memset((char *)sp[2], sp[1], *sp);
            break;
        case MCMP:
            ax = memcmp((char *)sp[2], sp[1], *sp);
            break;
        default:
            printf("unknown instruction:%d\n", op);
            return -1;
        }
    }
    return 0;
}

#undef int

/*第一个参数argc表示向main函数传递的参数的个数，但是它实际上要比你在命令行里输入的数据多一个，因为第一个参数它保存了该程序的路径名，也就是说，如果你向命令行输入3个数，则argc实际上等于4；
第二个参数argv保存命令行输入的参数值
argv[0]指向程序自身运行目录路径和程序名，
argv[1]指向程序在DOS命令中执行程序名后的第一个字符串
argv[2]指向第二个字符串
.......
argv[argc] 为NULL */
int main(int argc, char **argv)
{

    int i, fd;

    argc--;
    argv++;

    poolsize = 256 * 1024; // arbitrary size
    line = 1;

    if ((fd = open(*argv, 0)) < 0)
    {
        printf("could not open(%s)\n", *argv);
        return -1;
    }

    // allocate memory for virtual machine
    if (!(text = old_text = malloc(poolsize)))
    {
        printf("could not malloc(%d) for text area\n", poolsize);
        return -1;
    }
    if (!(data = malloc(poolsize)))
    {
        printf("could not malloc(%d) for data area\n", poolsize);
        return -1;
    }
    if (!(stack = malloc(poolsize)))
    {
        printf("could not malloc(%d) for stack area\n", poolsize);
        return -1;
    }
    if (!(symbols = malloc(poolsize)))
    {
        printf("could not malloc(%d) for symbol table\n", poolsize);
        return -1;
    }

    memset(text, 0, poolsize);
    memset(data, 0, poolsize);
    memset(stack, 0, poolsize);
    memset(symbols, 0, poolsize);
    bp = sp = (int *)((int)stack + poolsize);
    ax = 0;

    src = "char else enum if int return sizeof while "
          "open read close printf malloc memset memcmp exit void main";

    // add keywords to symbol table
    i = Char;
    while (i <= While)
    {
        next();
        current_id[Token] = i++;
    }

    // add library to symbol table
    i = OPEN;
    while (i <= EXIT)
    {
        next();
        current_id[Class] = Sys;
        current_id[Type] = INT;
        current_id[Value] = i++;
    }

    next();
    current_id[Token] = Char; // handle void type
    next();
    idmain = current_id; // keep track of main

    // read the source file
    if ((fd = open(*argv, 0)) < 0)
    {
        printf("could not open(%s)\n", *argv);
        return -1;
    }

    if (!(src = old_src = malloc(poolsize)))
    {
        printf("could not malloc(%d) for source area\n", poolsize);
        return -1;
    }
    // read the source file
    if ((i = read(fd, src, poolsize - 1)) <= 0)
    {
        printf("read() returned %d\n", i);
        return -1;
    }
    src[i] = 0; // add EOF character
    close(fd);

    program();

    return eval();
}
