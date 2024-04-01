#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

extern char **environ;
//======================================================================
int main(int argc, char *argv[])
{
    char cwd[4096];
    setbuf(stdout, NULL);

    if (!getcwd(cwd, sizeof(cwd)))
        cwd[0] = 0;

    printf("Content-Type: text/plain\r\n"
            "\r\n"
            "Hello from script: %s\n"
            "CWD: %s;\n\n",
            argv[0], cwd);
            
    int i = 0;
    for ( ; environ[i]; ++i)
    {
        printf("%s\n", environ[i]);
    }

    return 0;
}
