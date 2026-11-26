#include "ocsh.h"
#include "script.h"

// skipBlock
// Skips lines until the "end" that matches the block we are in.
// Used when a branch is NOT going to run, so we ignore its body.
int skipBlock(FILE *file)
{
    char line[512];
    char keyword[50];
    int depth = 1;

    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *current = line;

        // skip the spaces at the start of the line
        while (isspace((unsigned char)*current))
            current++;

        // get the first word
        if (sscanf(current, "%49s", keyword) != 1)
            continue;

        // check if another nested block inside
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

    // ran out of lines and never found the matching end
    fprintf(stderr, "OCSH: missing end in skipped block\n");
    return 0;
}

// executeIf
void executeIf(char line[], FILE *file)
{
    // Skip leading spaces before IF
    char *condition = line;

    while (isspace((unsigned char)*condition))
        condition++;

    // Run the condition command (condition + 3 skips the word "if")
    // The result is saved in lastStatus (0 means success)
    executeLine(condition + 3);

    int branchTaken = (lastStatus == 0);
    int shouldExecute = branchTaken;

    char bodyLine[512];
    char keyword[50];

    // Read the rest of the if block line by line
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
                // an earlier branch already ran, so this one is skipped
                shouldExecute = 0;
            }
            else
            {
                // run this new condition (+ 8 skips "else if ")
                executeLine(current + 8);

                shouldExecute = (lastStatus == 0);

                // remember that a branch ran so the next else is skipped
                if (shouldExecute)
                    branchTaken = 1;
            }

            continue;
        }

        // ELSE
        if (strcmp(keyword, "else") == 0)
        {
            // only run the else if no branch ran before
            shouldExecute = !branchTaken;
            branchTaken = 1;
            continue;
        }

        // NESTED IF
        if (strcmp(keyword, "if") == 0)
        {
            if (shouldExecute)
            {
                executeIf(current, file);
            }
            else
            {
                // this branch is not running, so skip the whole nested IF
                if (!skipBlock(file))
                    return;
            }

            continue;
        }

        // NESTED FOR
        if (strcmp(keyword, "for") == 0)
        {
            if (shouldExecute)
            {
                // run the loop
                executeFor(current, file);
            }
            else
            {
                // this branch is not running, so skip the whole loop
                if (!skipBlock(file))
                    return;
            }

            continue;
        }

        // NESTED WHILE
        if (strcmp(keyword, "while") == 0)
        {
            if (shouldExecute)
            {
                executeWhile(current, file);
            }
            else
            {
                if (!skipBlock(file))
                    return;
            }

            continue;
        }

        // END of this if block
        if (strcmp(keyword, "end") == 0)
            break;

        // Execute normal commands if a branch is selected
        if (shouldExecute)
        {
            executeLine(bodyLine);
        }
    }
}

// executeFor
void executeFor(char line[], FILE *file)
{
    char variable[50];
    char values[512];
    char bodyLine[512];
    char keyword[50];

    char *valueList[100];

    int valueCount = 0;
    int foundDo = 0;
    int foundEnd = 0;

    int depth = 1;

    // Read the variable and values
    char *header = line;

    while (isspace((unsigned char)*header))
        header++;

    // variable gets "i", values gets "1 2 3"
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

    // Make a temp file to store the body of the loop.
    FILE *temp = tmpfile();

    if (temp == NULL)
    {
        printf("Could not create temp file for FOR\n");
        return;
    }

    // 1. read the body until the MATCHING end.
    while (fgets(bodyLine, sizeof(bodyLine), file) != NULL)
    {
        if (sscanf(bodyLine, "%49s", keyword) == 1)
        {
            // a nested block appears
            if (strcmp(keyword, "if") == 0 ||
                strcmp(keyword, "for") == 0 ||
                strcmp(keyword, "while") == 0)
            {
                depth++;
            }
            // an end shows up
            else if (strcmp(keyword, "end") == 0)
            {
                depth--;

                if (depth == 0)
                {
                    foundEnd = 1;
                    break;
                }
            }
        }

        // save the line in the temp file
        fputs(bodyLine, temp);
    }

    if (!foundEnd)
    {
        printf("Missing END\n");
        fclose(temp);
        return;
    }

    // Separate the values ("1 2 3" becomes "1", "2", "3")
    char *value = strtok(values, " \t\r\n");

    while (value != NULL)
    {
        if (valueCount >= 100)
        {
            printf("Too many values\n");
            fclose(temp);
            return;
        }

        valueList[valueCount] = value;
        valueCount++;

        value = strtok(NULL, " \t\r\n");
    }

    if (valueCount == 0)
    {
        printf("FOR requires at least one value\n");
        fclose(temp);
        return;
    }

    // 2. run the body once for each value
    for (int i = 0; i < valueCount; i++)
    {
        // give the loop variable its value
        setVar(variable, valueList[i]);

        // go back to the start of the saved body and run it.
        rewind(temp);
        runLines(temp);
    }

    // done with the temp file
    fclose(temp);
}

// executeWhile
void executeWhile(char line[], FILE *file)
{
    char condition[512];
    char command[512];
    char bodyLine[512];
    char keyword[50];

    int foundDo = 0;
    int foundEnd = 0;

    int depth = 1;

    // Skip leading spaces before WHILE
    char *header = line;

    while (isspace((unsigned char)*header))
        header++;

    // gets everything after the word "while"
    if (sscanf(header, "while %511[^\n]", condition) != 1)
    {
        printf("Invalid WHILE syntax1\n");
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
        printf("Invalid WHILE syntax2\n");
        return;
    }

    // Make a temp file to store the body of the loop.
    FILE *temp = tmpfile();

    if (temp == NULL)
    {
        printf("Could not create temp file for WHILE\n");
        return;
    }

    // 1. read the body until the MATCHING end
    while (fgets(bodyLine, sizeof(bodyLine), file) != NULL)
    {
        if (sscanf(bodyLine, "%49s", keyword) == 1)
        {
            // a nested block appears
            if (strcmp(keyword, "if") == 0 ||
                strcmp(keyword, "for") == 0 ||
                strcmp(keyword, "while") == 0)
            {
                depth++;
            }
            // an end shows up
            else if (strcmp(keyword, "end") == 0)
            {
                depth--;

                if (depth == 0)
                {
                    foundEnd = 1;
                    break;
                }
            }
        }

        // save the line in the temp file
        fputs(bodyLine, temp);
    }

    if (!foundEnd)
    {
        printf("Missing END\n");
        fclose(temp);
        return;
    }

    // 2. keep looping while the condition succeeds (lastStatus == 0)
    while (1)
    {
        // copy the condition first then run
        strcpy(command, condition);
        executeLine(command);

        // if condition failed, end loop
        if (lastStatus != 0)
            break;

        // go back to the start of the saved body and run it
        rewind(temp);
        runLines(temp);
    }

    // done with the temp file
    fclose(temp);
}

// runLines
// Reads lines from any FILE and runs them.
void runLines(FILE *file)
{
    char line[512];
    char keyword[50];

    while (fgets(line, sizeof(line), file) != NULL)
    {
        // Read first word of the line, skip empty lines
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
            executeWhile(line, file);
        }
        else if (strcmp(keyword, "end") == 0 ||
                 strcmp(keyword, "then") == 0 ||
                 strcmp(keyword, "else") == 0 ||
                 strcmp(keyword, "do") == 0)
        {
            // TODO: Handle stuffs like continue/break if i decided to do so
        }
        else if (strcmp(keyword, "#") == 0)
        {
            continue; // comment line, ignore
        }
        else
        {
            // normal command
            executeLine(line);
        }
    }
}

// runScript
int runScript(char filename[])
{
    FILE *file = fopen(filename, "r");

    if (file == NULL)
    {
        perror("Error opening script");
        return 1;
    }

    runLines(file);

    fclose(file);
    return 0;
}