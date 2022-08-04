#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>
#include <math.h> //add -lm to compile

#define DUMMY_ISO8601_STRING "YYYY-mm-dd HH:MM:SS.nnn"
#define MAX_ISO8601_STRING sizeof(DUMMY_ISO8601_STRING)

//#############################################################################
// function declarations
long int cast_systime_2_time_t(double systime);
struct tm func_iso8601_2_tm_struct(char *inp_iso8601);
double func_iso8601_2_systime(char *iso8601);
char* func_systime_2_iso8601(double systime);
char* func_iso8601_2_yyyymmddhhmmss(char* iso8601);
char* func_iso8601_2_yyyymmdd(char* iso8601);
char* func_iso8601_2_hhmmss(char* iso8601);
char* func_iso8601_2_urpvalid(char* inp_iso8601, int L_ROUNDING, int minute_res);
char* func_add_nsecs_2_iso8601(char* iso8601, double n_secs);
