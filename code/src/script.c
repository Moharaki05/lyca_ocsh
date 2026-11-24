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

void executeFor(char line[], FILE *file)
{
    char variable[50];
    char values[512];

    char body[100][512];
    char bodyLine[512];
    char keyword[50];

    char *valueList[100];

    int bodyCount = 0;
    int valueCount = 0;
    int foundDo = 0;
    int foundEnd = 0;

    // Read the variable and values
    char *header = line;

    while (isspace((unsigned char)*header))
        header++;

    if (sscanf(header, "for %49s in %511[^\n]",
               variable, values) != 2)
    {
        printf("Invalid FOR syntax1\n");
        return;
    }

    // Find DO
    while (fgets(bodyLine, sizeof(bodyLine), file) != NULL)
    {
        if (sscanf(bodyLine, "%49s", keyword) != 1)
            continue;

        if (strcmp(keyword, "do") == 0)
        {
            foundDo = 1;
            break;
        }
    }

    if (!foundDo)
    {
        printf("Invalid FOR syntax2\n");
        return;
    }

    // Read the body until END
    while (fgets(bodyLine, sizeof(bodyLine), file) != NULL)
    {
        if (sscanf(bodyLine, "%49s", keyword) == 1 &&
            strcmp(keyword, "end") == 0)
        {
            foundEnd = 1;
            break;
        }

        if (bodyCount >= 100)
        {
            printf("FOR body too large\n"); // let me know if yall need me to increase the size
            return;
        }

        strcpy(body[bodyCount], bodyLine);
        bodyCount++;
    }

    if (!foundEnd)
    {
        printf("Missing END\n");
        return;
    }

    // Separate the values
    char *value = strtok(values, " \t\r\n");

    while (value != NULL)
    {
        if (valueCount >= 100)
        {
            printf("Too many values\n"); // can only store up to 100 elements, (for i in 1 2 3 .... 100), lmk if i should increase them
            return;
        }

        valueList[valueCount] = value;
        valueCount++;

        value = strtok(NULL, " \t\r\n");
    }

    if (valueCount == 0)
    {
        printf("FOR requires at least one value\n");
        return;
    }

    // Execute the body for each value
    for (int i = 0; i < valueCount; i++)
    {
        setVar(variable, valueList[i]);

        for (int j = 0; j < bodyCount; j++)
        {
            char command[512];

            strcpy(command, body[j]);
            executeLine(command);
        }
    }
}

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
            executeFor(line, file);
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