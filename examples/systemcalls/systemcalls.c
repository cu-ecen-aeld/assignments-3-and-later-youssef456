#include "systemcalls.h"
<<<<<<< HEAD
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

=======

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/

    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

>>>>>>> f5ed9a8a346cc7e74cfb9bcc65c008042dc7329c
bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
<<<<<<< HEAD
    char *command[count + 1];
    int i;
    for (i = 0; i < count; i++)
=======
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
>>>>>>> f5ed9a8a346cc7e74cfb9bcc65c008042dc7329c
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
<<<<<<< HEAD

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

=======
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/

    va_end(args);

    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
>>>>>>> f5ed9a8a346cc7e74cfb9bcc65c008042dc7329c
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
<<<<<<< HEAD
    char *command[count + 1];
    int i;
    for (i = 0; i < count; i++)
=======
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
>>>>>>> f5ed9a8a346cc7e74cfb9bcc65c008042dc7329c
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
<<<<<<< HEAD

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

=======
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/

    va_end(args);

    return true;
}
>>>>>>> f5ed9a8a346cc7e74cfb9bcc65c008042dc7329c
