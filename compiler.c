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

int token;                    //current token
char *src, *old_src;          //pointer to source code string
int poolsize;                 //default size of text/date/stack
int line;                     //line number
int *text,                    // text segment
    *old_text,                // for dump text segment
    *stack;                   // stack
char *data;                   // data segment
int *pc, *bp, *sp, ax, cycle; // virtual machine registers

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
    }
    
    token = *src++;
    return;
}

void expression(int level)
{
    //do something
}

void program()
{
    next();
    while (token > 0)
    {
        printf("token is: %c\n", token);
        next();
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
#define int long long
    int i, fd;
    argc--;
    argv++;
    poolsize = 256 * 1024;
    line = 1;
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
    memset(text, 0, poolsize);
    memset(data, 0, poolsize);
    memset(stack, 0, poolsize);
    bp = sp = (int *)((int)stack + poolsize);
    ax = 0;

    i = 0;
    text[i++] = IMM;
    text[i++] = 10;
    text[i++] = PUSH;
    text[i++] = IMM;
    text[i++] = 20;
    text[i++] = ADD;
    text[i++] = PUSH;
    text[i++] = EXIT;
    pc = text;
    program();
    return eval();
}