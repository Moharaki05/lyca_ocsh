#include <stdio.h>
#include <string.h>
#include "ocsh.h"
#include "script.h"




// Skip a block of code until the matching "end" keyword, for nested blocks
int skipBlock(FILE *file)
{
    char line[512];
    char keyword[50];
    int depth = 1;

    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *current = line;

        while (isspace((unsigned char)*current))
            current++;

        if (sscanf(current, "%49s", keyword) != 1)
            continue;

        if (strcmp(keyword, "if") == 0 ||
            strcmp(keyword, "for") == 0 ||
            strcmp(keyword, "while") == 0)
        {
            depth++;
        }
        else if (strcmp(keyword, "end") == 0)
        {
            depth--;

            if (depth == 0)
                return 1;
        }
    }

    fprintf(stderr, "OCSH: missing end in skipped block\n");
    return 0;
}

void executeIf(char line[], FILE *file)
{
    // Skip leading spaces before IF
    char *condition = line;

    while (isspace((unsigned char)*condition))
        condition++;

    // Execute the first condition
    executeLine(condition + 3);

    int branchTaken = (lastStatus == 0);
    int shouldExecute = branchTaken;

    char bodyLine[512];
    char keyword[50];

    while (fgets(bodyLine, sizeof(bodyLine), file) != NULL)
    {
        // Skip indentation
        char *current = bodyLine;

        while (isspace((unsigned char)*current))
            current++;

        // Extract the first word
        if (sscanf(current, "%49s", keyword) != 1)
            continue;

        // Ignore THEN
        if (strcmp(keyword, "then") == 0)
            continue;

        // ELSE IF
        if (strncmp(current, "else if ", 8) == 0)
        {
            if (branchTaken)
            {
                shouldExecute = 0;
            }
            else
            {
                executeLine(current + 8);

                shouldExecute = (lastStatus == 0);

                if (shouldExecute)
                    branchTaken = 1;
            }

            continue;
        }

        // ELSE
        if (strcmp(keyword, "else") == 0)
        {
            shouldExecute = !branchTaken;
            branchTaken = 1;
            continue;
        }

        // NESTED 
        if (strcmp(keyword, "if") == 0)
        {
            if (shouldExecute)
            {
                // Execute nested IF recursively
                executeIf(current, file);
            }
            else
            {
                // Skip the entire nested IF
                if (!skipBlock(file))
                    return;
            }

            continue;
        }

        // END
        if (strcmp(keyword, "end") == 0)
            break;

        // Execute commands in selected branch
        if (shouldExecute)
        {
            executeLine(bodyLine);
        }
    }
}

// void executeFor(char line[])
// {
// }

// void executeWhile(char line[])
// {
// }

void executeCommand(char line[])
{
    executeLine(line);
}

// Read the script
int runScript(char filename[])
{

    FILE *file = fopen(filename, "r");

    if (file == NULL)
    {
        perror("Error opening script");
        return 1;
    }

    char line[512];
    char keyword[50];

    while (fgets(line, sizeof(line), file) != NULL)
    {

        // Read first word of the line
        if (sscanf(line, "%49s", keyword) != 1)
        {
            continue;
        }

        if (strcmp(keyword, "if") == 0)
        {
            executeIf(line, file);
        }
        else if (strcmp(keyword, "for") == 0)
        {
            printf("Found FOR\n");
            // executeFor(line);
        }
        else if (strcmp(keyword, "while") == 0)
        {
            printf("Found WHILE\n");
            // executeWhile(line);
        }
        else if (strcmp(keyword, "end") == 0 ||
                 strcmp(keyword, "then") == 0 ||
                 strcmp(keyword, "else") == 0 ||
                 strcmp(keyword, "do") == 0)
        {
            // TODO: Handle block keywords
        }
        else
        {
            executeCommand(line);
        }
    }

    fclose(file);
    return 0;
}