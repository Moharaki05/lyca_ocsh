#pragma once

int runScript(char filename[]);

int skipBlock(FILE *file);
void executeIf(char line[], FILE *file);
void executeFor(char line[], FILE *file);
void executeWhile(char line[], FILE *file);
void runLines(FILE *file);