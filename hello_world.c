#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
    puts("Hello, World!");
    char* a = "Hello";
    printf("%i", strstr(a, "rello") == 0);
    return 0;
}
