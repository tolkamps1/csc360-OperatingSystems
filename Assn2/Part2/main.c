#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#define TOKEN_DELIM ", \r\n"
#define BUFFER_SIZE 128


/**
 * Tokenizes lined to get time and value
 */
char** tokenize_line(char *line){
  int buffsize = BUFFER_SIZE, position = 0;
  char **tokens = malloc(buffsize * sizeof(char*));
  char *token;
  if (!tokens) {
    fprintf(stderr, "Allocation error\n");
    exit(EXIT_FAILURE);
  }
  token = strtok(line, TOKEN_DELIM);
  while (token != NULL) {
  tokens[position] = token;
  position++;
  if (position >= buffsize) {
      buffsize += BUFFER_SIZE;
      tokens = realloc(tokens, buffsize * sizeof(char*));
      if (!tokens) {
        fprintf(stderr, "Allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
    token = strtok(NULL, TOKEN_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}


/**
 * Initialization. Prints the best L1 line by intercept and slope.
 */
int main (int argc, char **argv){
    // data file stremflow_time_series.csv || testcase files
    char **args;
    if(argc < 2){
        printf("Please provide file path for CSV data\n.");
        exit(EXIT_FAILURE);
    }
    char *line = NULL;
    size_t buffsize = 0;
    ssize_t line_size;
    const char* file_path = argv[1];
    FILE *fp = fopen(file_path, "r");
    if (fp){
        line_size = getline(&line, &buffsize, fp);
        while(line_size >= 0){
            args = tokenize_line(line);
            printf("Date: %s, Value: %s\n", args[0], args[1]);
            // DO STUFF
            free(args);
            line_size = getline(&line, &buffsize, fp);
        }
        free(line);
        fclose(fp);
    }
    else{
        printf("File %s failed to open. Please check path.\n", file_path);
        exit(EXIT_FAILURE);

    }
    printf("Done.\n");
    exit(EXIT_SUCCESS);
}