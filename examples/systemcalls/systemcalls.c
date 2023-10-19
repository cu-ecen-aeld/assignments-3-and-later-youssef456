#include "systemcalls.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

bool do_system(const char *cmd)
{
    int exit_status = system(cmd);
    return exit_status == 0; // Check if the command was executed successfully
}

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char *command[count + 1];
    int i;
    for (i = 0; i < count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    pid_t child_pid = fork();
    if (child_pid == 0)
    {
        // Child process
        execv(command[0], command);
        // If execv returns, there was an error
        perror("execv"); // Print an error message
        exit(EXIT_FAILURE); // Terminate the child process
    }
    else if (child_pid < 0)
    {
        // Fork failed
        perror("fork");
        return false;
    }
    else
    {
        // Parent process
        int status;
        waitpid(child_pid, &status, 0);
        return WIFEXITED(status) && (WEXITSTATUS(status) == 0);
    }

    va_end(args);
}

bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char *command[count + 1];
    int i;
    for (i = 0; i < count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    pid_t child_pid = fork();
    if (child_pid == 0)
    {
        // Child process
        FILE *output = fopen(outputfile, "w");
        if (output == NULL)
        {
            perror("fopen");
            exit(EXIT_FAILURE);
        }

        // Redirect standard output to the output file
        dup2(fileno(output), STDOUT_FILENO);
        fclose(output);

        execv(command[0], command);
        // If execv returns, there was an error
        perror("execv"); // Print an error message
        exit(EXIT_FAILURE); // Terminate the child process
    }
    else if (child_pid < 0)
    {
        // Fork failed
        perror("fork");
        return false;
    }
    else
    {
        // Parent process
        int status;
        waitpid(child_pid, &status, 0);
        return WIFEXITED(status) && (WEXITSTATUS(status) == 0);
    }

    va_end(args);
}

