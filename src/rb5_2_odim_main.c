/* --------------------------------------------------------------------
Copyright (C) 2016 The Crown (i.e. Her Majesty the Queen in Right of Canada)

This file is part of RAVE.

RAVE is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RAVE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with RAVE.  If not, see <http://www.gnu.org/licenses/>.
------------------------------------------------------------------------*/
/**
 * Command-line binary "rb5_2_odim"
 * @file src/rb5_2_odim_main.c
 * @author Peter Rodriguez, Environment Canada
 * @date 2022-08-04
 *
 * Compile:
 *   make -f Makefile.w_rb5_2_odim_main
 *
 * Debug:
 *   valgrind --leak-check=full --show-reachable=yes ./rb5_2_odim -i ../test/org/2016081612320300dBZ.azi -o dummy.azi.h5
 *   valgrind --leak-check=full --show-reachable=yes ./rb5_2_odim -i ../test/org/2016092614304000dBZ.vol -o dummy.vol.h5
 *   valgrind --leak-check=full --show-reachable=yes ./rb5_2_odim -i ../test/org/CASSR_2023020717120300dBZ.vol.gz -o dummy.h5 //bad datetimehighaccuracy issue on iSLICE (base-0) = 7,8,9,10
 *
 * Example:
 * ./rb5_2_odim -i ../test/org/CASRA_2017121520000300dBZ.vol.gz -o CASRA_20171215200003_dBZ.test.h5
 * h5dump --attribute=/dataset1/how/astart CASRA_20171215200003_dBZ.test.h5
 */

//#include "rave_debug.h"
#include <rb52odim.h>

int main(int argc,char *argv[]) {
    int RETURN_FAILURE = -1;
    int ret = 0;
    int i;
    const char *ifile=NULL, *ofile=NULL;

    if (argc!=5) {
      printf("usage: %s -i RB5_file -o ODIM_H5_file\n", argv[0]);
      return 1;
    }

    for (i=1;i<argc;i++) {
      if (strcmp(argv[i], "-i") == 0) {
        i++;
        ifile = argv[i];
      }
      else if (strcmp(argv[i], "-o") == 0) {
        i++;
        ofile = argv[i];
      }
      else {
        printf("usage: %s -i RB5_file -o ODIM_H5_file\n", argv[0]);
        return RETURN_FAILURE;
      }
    }

//#############################################################################

    // hack for command-line version without BALTRAD envvars
    if(getenv("RB52ODIMCONFIG")==NULL){
      fprintf(stderr,"Error cannot getenv(\"RB52ODIMCONFIG\")\n");
      char sPATH[MAX_STRING]="\0";
      getcwd(sPATH, sizeof(sPATH));
      strcat(sPATH,"/../config");
      char sCMD[MAX_STRING]="\0";
      strcpy(sCMD,"RB52ODIMCONFIG=");
      strcat(sCMD,sPATH);
      fprintf(stderr,"Setting envar : %s\n",sCMD);
      putenv(sCMD);
    }

//#############################################################################

//    /* call this before any of your RAVE code */
//    Rave_initializeDebugger();
//    Rave_setDebugLevel(RAVE_SPEWDEBUG);

//#############################################################################
// from rb52odim.c:getRaveIO()
//#############################################################################

    RaveIO_t* raveio = RAVE_OBJECT_NEW(&RaveIO_TYPE);
    RaveCoreObject* object = NULL;
    RaveIO_setObject(raveio, object); //init with raveio.object = NULL
    int rot = Rave_ObjectType_UNDEFINED;

//#############################################################################

    //use open_xml_buffer() to ingest file
    char *inp_fname=(char *)ifile;
    strXML_FILE_INFO xml_info;
    strcpy(xml_info.inp_fullfile,inp_fname);
    if(open_xml_buffer(&xml_info) != 0) {
      fprintf(stderr,"Error cannot process file = %s\n", inp_fname);
      return(EXIT_FAILURE);
    }

    //get RB5 top level info
    //init with xml_info
    strRB5_INFO rb5_info;
    strcpy(rb5_info.inp_fullfile,xml_info.inp_fullfile);
    rb5_info.buffer=xml_info.buffer;
    rb5_info.buffer_len=xml_info.buffer_len;
    rb5_info.byte_offset_blobspace=xml_info.byte_offset_end_of_xml;
    rb5_info.doc=xml_info.doc;
    rb5_info.xpathCtx=xml_info.xpathCtx;

    int L_VERBOSE=1;
    if(populate_rb5_info(&rb5_info,L_VERBOSE) != EXIT_SUCCESS) {
      fprintf(stderr,"Error cannot process file = %s\n", inp_fname);
      return RETURN_FAILURE;
//      return raveio;
    }

    printf("Successfully ingested : %s\n", inp_fname);

//#############################################################################

    /* If the RB5 file contains a scan or a pvol, create equivalent object */
    rot = objectTypeFromRB5(rb5_info);
    if (rot == Rave_ObjectType_PVOL) {
      object = (RaveCoreObject*)RAVE_OBJECT_NEW(&PolarVolume_TYPE);
    } else if (rot == Rave_ObjectType_SCAN) {
      object = (RaveCoreObject*)RAVE_OBJECT_NEW(&PolarScan_TYPE);
    } else {
      return RETURN_FAILURE;
    }

    /* Map RB5 object(s) to Toolbox ones. */
    ret = populateObject(object, &rb5_info);
    close_rb5_info(&rb5_info);
    xmlCleanupParser(); // free globals in main() only for thread safety & valgrind

    /* Set the object into the I/O container */
    RaveIO_setObject(raveio, object);
    RAVE_OBJECT_RELEASE(object);

//#############################################################################

    /* write to ODIM_H5 file */
    ret = RaveIO_save(raveio, ofile);
    RaveIO_close(raveio);
    RAVE_OBJECT_RELEASE(raveio);

    return ret;
}
