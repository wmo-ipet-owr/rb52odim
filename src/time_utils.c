/*
 * time_utils.c
 *
 * Author: Peter Rodriguez 2016-Sep-19
 *
 * compile only: gcc -g -c time_utils.c -o time_utils.o
 * 
 * 2022-01-13:  PR  use timegm() instead of mktime() to use UTC not OS local timezone (TZ env var)
 *                  tm_struct should not round by millisec, for (iso8601 -> systime -> iso8601)
 * 2016-09-19:  PR  new subroutines,
 *                  - func_iso8601_2_tm_struct()
 *                  - func_iso8601_2_systime()
 *                  - func_systime_2_iso8601()
 *                  - func_iso8601_2_yyyymmddhhmmss()
 *                  - func_iso8601_2_yyyymmdd()
 *                  - func_iso8601_2_hhmmss()
 *                  - func_iso8601_2_urpvalid()
 *                  - func_add_nsecs_2_iso8601()
 *
 */

#include "time_utils.h"

long int cast_systime_2_time_t(double systime){

    //note: time_t doesn't handle millisecs, a long int not a double var
    //any millisecs shall increase the systime_t
    int milli=(systime-floor(systime))*1000.;
    if(milli != 0) {
      return (time_t) systime+1;
    } else {
      return (time_t) systime;
    }

}

//#############################################################################
struct tm func_iso8601_2_tm_struct(char *inp_iso8601) {

    char s_year  [4+1] = "\0"; //"YYYY"
    char s_month [2+1] = "\0"; //"mm"
    char s_day   [2+1] = "\0"; //"dd"
    char s_hour  [2+1] = "\0"; //"HH"
    char s_minute[2+1] = "\0"; //"MM"
    char s_second[6+1] = "\0"; //"SS.nnn"
    struct tm tm_info={0};

    sscanf(inp_iso8601,"%4c-%2c-%2c%*c%2c:%2c:%6c",
        s_year,
        s_month,
        s_day,
        //ignore this char
        s_hour,
        s_minute,
        s_second);

    tm_info.tm_year = atoi(s_year) - 1900;
    tm_info.tm_mon  = atoi(s_month) - 1;
    tm_info.tm_mday = atoi(s_day);
    tm_info.tm_hour = atoi(s_hour);
    tm_info.tm_min  = atoi(s_minute);
    tm_info.tm_sec  = atoi(s_second);
    /* millisec handling in func_iso8601_2_systime() */

    return(tm_info);

}

//#############################################################################
double func_iso8601_2_systime(char *iso8601) {

    struct tm tm_info=func_iso8601_2_tm_struct(iso8601);
    double systime=timegm(&tm_info); //uses Universal Coordinated Time (UTC)

    //add millisecs
    char *p_dot=strstr(iso8601,".");
    if(p_dot != NULL) {
      float milli=0;
      sscanf(p_dot+1,"%f",&milli);
      systime+=milli/1000.;
    }

    return(systime);

}

//#############################################################################
char* func_systime_2_iso8601(double systime) {

    static char this_iso8601_string[MAX_ISO8601_STRING+1]="\0";
    time_t systime_t=systime;
    strftime(this_iso8601_string,MAX_ISO8601_STRING,"%Y-%m-%d %H:%M:%S",gmtime(&systime_t));

    //add millisecs
    int milli=(systime-floor(systime))*1000.;
    if(milli != 0) {
      char s_milli[4+1]= "\0"; //".nnn"
      sprintf(s_milli,".%03d",milli);
      strcat(this_iso8601_string,s_milli);
    }

    return(this_iso8601_string);

}

//#############################################################################
char* func_iso8601_2_yyyymmddhhmmss(char* iso8601) {

    static char this_iso8601_string[MAX_ISO8601_STRING+1]="\0";
    double systime=func_iso8601_2_systime(iso8601);
    time_t systime_t=cast_systime_2_time_t(systime);
    strftime(this_iso8601_string,MAX_ISO8601_STRING,"%Y%m%d%H%M%S",gmtime(&systime_t));
    return(this_iso8601_string);

}

//#############################################################################
char* func_iso8601_2_yyyymmdd(char* iso8601) {

    static char this_iso8601_string[MAX_ISO8601_STRING+1]="\0";
    double systime=func_iso8601_2_systime(iso8601);
    time_t systime_t=cast_systime_2_time_t(systime);
    strftime(this_iso8601_string,MAX_ISO8601_STRING,"%Y%m%d",gmtime(&systime_t));
    return(this_iso8601_string);

}

//#############################################################################
char* func_iso8601_2_hhmmss(char* iso8601) {

    static char this_iso8601_string[MAX_ISO8601_STRING+1]="\0";
    double systime=func_iso8601_2_systime(iso8601);
    time_t systime_t=cast_systime_2_time_t(systime);
    strftime(this_iso8601_string,MAX_ISO8601_STRING,"%H%M%S",gmtime(&systime_t));
    return(this_iso8601_string);

}

//#############################################################################
char* func_iso8601_2_urpvalid(char* inp_iso8601, int L_ROUNDING, int minute_res) {
    // L_ROUNDING=0=flooring, 1=rounding

    time_t inp_systime=cast_systime_2_time_t(func_iso8601_2_systime(inp_iso8601));
    time_t nINTERVAL_SECs=minute_res*60;
    time_t out_systime;

    if(L_ROUNDING){ //rounding
        fprintf(stdout,"  ROUNDING %s within %ld sec\n",inp_iso8601,nINTERVAL_SECs);
        double d_tmpa=(inp_systime+(nINTERVAL_SECs/2.))/nINTERVAL_SECs;
        out_systime=floor(d_tmpa)*nINTERVAL_SECs;
    } else { //flooring
        fprintf(stdout,"  FLOORING %s within %ld sec\n",inp_iso8601,nINTERVAL_SECs);
        out_systime=inp_systime-(inp_systime % nINTERVAL_SECs);
    }

//    fprintf(stdout,"out_systime = %ld\n",out_systime);
//    fprintf(stdout,"out_iso8601 = %s\n",func_systime_2_iso8601(out_systime));
//    fprintf(stdout,"\n");

    static char this_iso8601_string[MAX_ISO8601_STRING+1]="\0";
    strftime(this_iso8601_string,MAX_ISO8601_STRING,"%Y%m%d%H%M",gmtime(&out_systime));
    return(this_iso8601_string);

}

//#############################################################################
char* func_add_nsecs_2_iso8601(char* iso8601, double n_secs) {

    return(func_systime_2_iso8601(func_iso8601_2_systime(iso8601)+n_secs));

}

