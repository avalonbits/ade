/*
 * A stand-in for a tool that reports more than one error, which neither acc
 * nor zap does yet: it writes three, in their form, to the file named on its
 * command line, and fails with 100 as they do. For test/run.sh, built with
 * acc for the Agon.
 */
#include <stdio.h>

int main(int argc, char** argv)
{
    FILE* f;

    if (argc < 2) {
        return 1;
    }
    f = fopen(argv[1], "w");
    if (f == NULL) {
        return 1;
    }
    fputs("main.c:2:5: error: the first\r\n", f);
    fputs("main.c:4:1: error: the second\r\n", f);
    fputs("add.c:1:3: error: the third\r\n", f);
    fclose(f);

    return 100;
}
