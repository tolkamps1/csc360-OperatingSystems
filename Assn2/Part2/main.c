#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <float.h>

#define TOKEN_DELIM ", \r\n"
#define BUFFER_SIZE 128

float data [18][2];

typedef struct line{
    float x_intercept;
    float slope;
    float y_intercept;
} line_eq;


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
 * Calulates best L1 line. Start is the starting index for the data
 * array (inclusive). End is the ending index (not inclusive).
 */
line_eq L1_line(int start, int end){
    line_eq L1;
    L1.slope = 0;
    L1.x_intercept = 0;
    L1.y_intercept = 0;
    // For all points
    for(int x = start; x < end; x++){
        printf("x: %d\n", x);
        float min = DBL_MAX;
        for(int j = x + 1; j < end; j++){
            printf("j: %d\n", j);
            float line_intercept = data[x][1];
            float line_slope = (j-x/((data[j][1]+1)-(data[x][1])+1));
            float sum = 0;
            for(int k = 0; k < 18; k++){
                sum += abs((data[k][1] - (line_slope*1) - (line_slope*k)));
            }
            if(sum < min){
                min = sum;
                L1.x_intercept = line_intercept;
                L1.slope = line_slope;
                L1.y_intercept = x-(line_slope * data[x][1]);
            }
        }
        printf("Line\n x-int: %f\n y-int: %f\n slope: %f\n", L1.x_intercept, L1.y_intercept, L1.slope);
    }
    return L1;
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
        int count = -1;
        while(line_size >= 0){
            args = tokenize_line(line);
            // Skip words declaration in first line
            if(count < 0){
                count ++;
                free(args);
                line_size = getline(&line, &buffsize, fp);
                continue;
            }
            data[count][0] = (int) count;
            data[count][1] = atof(args[1]);
            printf("Date: %s, Value: %s\n", args[0], args[1]);
            free(args);
            line_size = getline(&line, &buffsize, fp);
            count++;
        }
        free(line);
        fclose(fp);
    }
    else{
        printf("File %s failed to open. Please check path.\n", file_path);
        exit(EXIT_FAILURE);
    }
    // Calculate line
    line_eq best_L1 = L1_line(0,6);
    printf("Best L1 line\n x-int: %f\n y-int: %f\n slope: %f\n", best_L1.x_intercept, best_L1.y_intercept, best_L1.slope);

    printf("Done.\n");
    exit(EXIT_SUCCESS);
}