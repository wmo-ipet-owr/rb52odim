// compile: gcc -g -Wall time_utils.c test_time_utils.c -lm -o test_time_utils

// check: valgrind --leak-check=full ./test_time_utils

/* NOTE: test w/ time zone change
 * sudo timedatectl set-timezone "America/Toronto"
 * timedatectl
 * ./test_time_utils
 * sudo timedatectl set-timezone "Etc/UTC"
 */

#include "time_utils.h"

int main(int argc, char **argv) {

    char blank_iso8601_string[MAX_ISO8601_STRING+1]="\0";
    char *iso8601=blank_iso8601_string;
    char *fmt_s="%25s = %s\n";
    char *fmt_f="%25s = %f\n";

    strcpy(iso8601,"2016-09-16 16:25:56.789");
    double systime=func_iso8601_2_systime(iso8601);

    fprintf(stdout, fmt_s, "iso8601", iso8601);
    fprintf(stdout, fmt_f, "-> systime", systime);
    fprintf(stdout, fmt_s, "-> systime -> iso8601", func_systime_2_iso8601(func_iso8601_2_systime(iso8601)));
    fprintf(stdout, fmt_s, "+ 100secs", func_add_nsecs_2_iso8601(iso8601,100));
    fprintf(stdout, fmt_s, "-> yyyymmddhhmmss", func_iso8601_2_yyyymmddhhmmss(iso8601));
    fprintf(stdout, fmt_s, "-> yyyymmdd", func_iso8601_2_yyyymmdd(iso8601));
    fprintf(stdout, fmt_s, "-> hhmmss", func_iso8601_2_hhmmss(iso8601));
    fprintf(stdout, fmt_s, "-> urpvalid",func_iso8601_2_urpvalid(iso8601,1,5));

    return(1);
}
