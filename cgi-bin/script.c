#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

    char *method = getenv("REQUEST_METHOD");
    if (!strcmp(method, "POST"))
    {
        long cont_len = 0;
        printf("\nstdin:\n");
        sscanf(getenv("CONTENT_LENGTH"), "%ld", &cont_len);
        while (cont_len)
        {
            int n = fread(cwd, 1, sizeof(cwd), stdin);
            if (n <= 0)
                break;
            fwrite(cwd, 1, n, stdout);
            cont_len -= n;
            printf("\n");
        }
    }

    return 0;
}
