/* --------------------------------------------------------------------
Copyright (C) 2016 The Crown (i.e. Her Majesty the Queen in Right of Canada)

This file is an add-on to RAVE.

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
 * Description
 * @file
 * @author Daniel Michelson and Peter Rodriguez, Environment Canada
 * @date 2016-08-17
 * 2016-09-21: PR Modified to replace RAVE_MALLOC() & RAVE_FREE() to RAVE_MALLOC() & RAVE_FREE() accordingly
 *                - vi cmd: 15,$s/RAVE_MALLOC(/RAVE_MALLOC(/g
 *                - vi cmd: 15,$s/RAVE_FREE(/RAVE_FREE(/g
 */
#include "rb52odim.h"

/**
 * Function name: objectTypeFromRB5
 * Intent: Based on number of scans in the payload, return the corresponding RAVE ObjectType enum
 * for SCAN or PVOL
 */
int objectTypeFromRB5(strRB5_INFO rb5_info) {
   if ((strcmp(rb5_info.scan_type,"vol") == 0) ||
       (strcmp(rb5_info.scan_type,"azi") == 0) ||
        //to be special handled and faked into an ODIM ELEV object on output
       (strcmp(rb5_info.scan_type,"ele") == 0)) { 
      if(rb5_info.n_slices > 1) return Rave_ObjectType_PVOL;
      else return Rave_ObjectType_SCAN;
   }
   else {
      return Rave_ObjectType_UNDEFINED;
   }
} // End function: objectTypeFromRB5


/*
 * Input object is an empty Toolbox sweep/moment of data and a native RB5 object (if that's how RB5 data are provided).
 */
int populateParam(PolarScanParam_t* param, strRB5_INFO *rb5_info, strRB5_PARAM_INFO *rb5_param) {
	int ret = 0;
	//	RaveDataType type;

	/* Map RB5 moments to ODIM, e g. corrected horizontal reflectivity */
	PolarScanParam_setQuantity(param, map_rb5_to_h5_param(rb5_param->sparam));

	/* Linear scaling factor, with an example for 8-bit reflectivity */
	PolarScanParam_setGain(param, rb5_param->data_step);

	/* Linear scaling offset, with an example for 8-bit reflectivity */
	PolarScanParam_setOffset(param, rb5_param->data_range_min);

	/* Value for 'no data', ie. unradiated areas, with a convention used for reflectivity */
    //left to user to mask by either <dataflag> or <txpower> if available
	PolarScanParam_setNodata(param, rb5_param->raw_binary_max); 

	/* Value for 'undetected', ie. areas radiated but with no echo, with a convention used for reflectivity */
	PolarScanParam_setUndetect(param, 0);

	/* Access the data buffer from RB5. Ensure they are ordered properly, ie. with the first ray pointing north. */
	//rb5_util vars
    //Note: my decode returns a void*, user must resolve by data_depth, i.e.  data_type
	void *raw_arr=NULL;
    return_param_blobid_raw(&(*rb5_info), &(*rb5_param), &raw_arr);
//  float *data_arr=NULL;
//  convert_raw_to_data(&(*rb5_param),&raw_arr,&data_arr);

	/* Figure out what data depth this moment of data is in, ie. 8, 16, 32, or 64-bit (u)int or float.
	 * Map to Toolbox equivalent. This example is for 16-bit unsigned int */
           if(rb5_param->raw_binary_depth ==  8) {
	     //type = RaveDataType_UCHAR;
//        uint8_t  *out_raw_arr=((uint8_t  *)raw_arr);
//	    ret = PolarScanParam_setData(param, rb5_param->nbins, rb5_param->nrays, out_raw_arr, type);
	    ret = PolarScanParam_setData(param, rb5_param->nbins, rb5_param->nrays, ((uint8_t  *)raw_arr), RaveDataType_UCHAR);
    } else if(rb5_param->raw_binary_depth == 16) {
	     //type = RaveDataType_USHORT;
//        uint16_t *out_raw_arr=((uint16_t *)raw_arr);
//	    ret = PolarScanParam_setData(param, rb5_param->nbins, rb5_param->nrays, out_raw_arr, type);
	    ret = PolarScanParam_setData(param, rb5_param->nbins, rb5_param->nrays, ((uint16_t *)raw_arr), RaveDataType_USHORT);
    } else if(rb5_param->raw_binary_depth == 32) {
	     //type = RaveDataType_UINT;
//        uint32_t *out_raw_arr=((uint32_t *)raw_arr);
//	    ret = PolarScanParam_setData(param, rb5_param->nbins, rb5_param->nrays, out_raw_arr, type);
	    ret = PolarScanParam_setData(param, rb5_param->nbins, rb5_param->nrays, ((uint32_t *)raw_arr), RaveDataType_UINT);
    }

	/* Set the data in the Toolbox object */
//	ret = PolarScanParam_setData(param, rb5_param->nbins, rb5_param->nrays, data_arr, RaveDataType_FLOAT);
//	ret = PolarScanParam_setData(param, rb5_param->nbins, rb5_param->nrays, out_raw_arr, type); //hmm, doesn't type cast

	if ( raw_arr != NULL ) RAVE_FREE( raw_arr);
//  if (data_arr != NULL ) RAVE_FREE(data_arr);

	/* We'll add appropriate exception handling later */
	return ret;
}


/*
 * Input object is an empty Toolbox polar scan object and a native RB5 object.
 */
int populateScan(PolarScan_t* scan, strRB5_INFO *rb5_info, int this_slice) {
	int ret = 0;
	int i;
	int np;  /* Number of moments/parameters in this scan of data */
	RaveCoreObject* object = (RaveCoreObject*)scan;

    //rb5_util vars
	strRB5_PARAM_INFO rb5_param;
	//static char xpath[MAX_STRING]="\0";
	static char xpath_bgn[MAX_STRING]="\0";
	static char iso8601[MAX_STRING]="\0";
	int L_RB5_PARAM_VERBOSE=0;

    //#############################################################################//
	/* Set select mandatory 'what' attributes. See Table 13 in the ODIM_H5 spec. */

	sprintf(iso8601,"%s",rb5_info->slice_iso8601_bgn[this_slice]);
	ret = PolarScan_setStartDate(scan, func_iso8601_2_yyyymmdd(iso8601)); //"YYYYMMDD"
	ret = PolarScan_setStartTime(scan, func_iso8601_2_hhmmss(iso8601)); //"HHmmss"

	sprintf(iso8601,"%s",rb5_info->slice_iso8601_end[this_slice]);
	ret = PolarScan_setEndDate(scan, func_iso8601_2_yyyymmdd(iso8601)); //"YYYYMMDD"
	ret = PolarScan_setEndTime(scan, func_iso8601_2_hhmmss(iso8601)); //"HHmmss"

    //#############################################################################//
	/* Set optional 'how' attributes. There are lots! See Table 8 in the ODIM_H5 spec. */

    // HOW/RADAR_DATA
    if (strcmp(rb5_info->scan_type,"ele") == 0) { 
	    ret = addDoubleAttribute(object, "how/elevspeed",rb5_info->slice_antspeed_rpm     [this_slice]);
    } else {
	    ret = addDoubleAttribute(object, "how/rpm",      rb5_info->slice_antspeed_rpm     [this_slice]);
    }

	ret = addDoubleAttribute(object, "how/pulsewidth",   rb5_info->slice_pw_microsec      [this_slice]);
	ret = addDoubleAttribute(object, "how/lowprf",       rb5_info->slice_lo_prf           [this_slice]);
	ret = addDoubleAttribute(object, "how/highprf",      rb5_info->slice_hi_prf           [this_slice]);
	ret = addDoubleAttribute(object, "how/radconstH",    rb5_info->slice_radconst_h       [this_slice]);
	ret = addDoubleAttribute(object, "how/radconstV",    rb5_info->slice_radconst_v       [this_slice]);
    
	ret = addDoubleAttribute(object, "how/NI",           rb5_info->slice_nyquist_vel      [this_slice]);
	ret = addLongAttribute  (object, "how/Vsamples",     rb5_info->slice_num_samples      [this_slice]);

//	ret = addDoubleAttribute(object, "how/nomTXpower",   ); // n/a
//	ret = addDoubleAttribute(object, "how/powerdiff",    ); // n/a
//	ret = addDoubleAttribute(object, "how/powerdiff",    ); // n/a
//	ret = addDoubleAttribute(object, "how/phasediff",    ); // sys_phidp

    // HOW/POLAR_DATA
	ret = addLongAttribute  (object, "how/scan_index",   this_slice+1);
	ret = addLongAttribute  (object, "how/scan_count",   rb5_info->n_slices);

    // HOW/QUALITY
	ret = addStringAttribute(object, "how/binmethod",    "AVERAGE");
    if (strcmp(rb5_info->scan_type,"ele") == 0) { 
	    ret = addStringAttribute(object, "how/elmethod",     "AVERAGE");
    	ret = addStringAttribute(object, "how/anglesync",    "elevation");
    	ret = addDoubleAttribute(object, "how/anglesyncRes", rb5_info->slice_ray_angle_res_deg[this_slice]);
    } else {
    	ret = addDoubleAttribute(object, "how/astart",       0.0);  /* IRIS quirk */
    	ret = addStringAttribute(object, "how/azmethod",     "AVERAGE");
    	ret = addStringAttribute(object, "how/anglesync",    "azimuth");
    	ret = addDoubleAttribute(object, "how/anglesyncRes", rb5_info->slice_ray_angle_res_deg[this_slice]);
    }

	ret = addStringAttribute(object, "how/malfunc",      "False");  /* Corrupt scans need to be caught */
	ret = addStringAttribute(object, "how/radar_msg",    "");  /* BITE message if malfunc=True*/
	ret = addDoubleAttribute(object, "how/NEZH",         rb5_info->slice_noise_power_h    [this_slice]);
	ret = addDoubleAttribute(object, "how/NEZV",         rb5_info->slice_noise_power_v    [this_slice]);

	ret = addStringAttribute(object, "how/clutterType", "GdrxCmOff"); //RB5 manual says "/volume/scan/slice[*]/gdrx_cluttermap" (not used)
	ret = addStringAttribute(object, "how/clutterMap",  "Off"); // (not used)
//	ret = addDoubleAttribute(object, "how/zcalH",       ); // n/a
//	ret = addDoubleAttribute(object, "how/zcalV",       ); // n/a
//	ret = addDoubleAttribute(object, "how/nsampleH",    ); // n/a
//	ret = addDoubleAttribute(object, "how/nsampleV",    ); // n/a
	ret = addDoubleAttribute(object, "how/SQI",         rb5_info->slice_sqi_threshold    [this_slice]);
	ret = addDoubleAttribute(object, "how/CSR",         rb5_info->slice_csr_threshold    [this_slice]);
	ret = addDoubleAttribute(object, "how/LOG",         rb5_info->slice_log_threshold    [this_slice]);
	ret = addStringAttribute(object, "how/VPRcorr",     "False");

///*
    // from <txpower> determine max & avg
    L_RB5_PARAM_VERBOSE=0;
    char req_rayinfo_name[MAX_STRING]="\0";
    strcpy(req_rayinfo_name,"txpower");
    int idx_req=find_in_string_arr(rb5_info->rayinfo_name_arr,rb5_info->n_rayinfos,req_rayinfo_name);
    if(idx_req != -1) {
    	sprintf(xpath_bgn,"((/volume/scan/slice)[%2d]/slicedata/%s)[%2d]/",this_slice+1,"rayinfo",idx_req+1);
		rb5_param=get_rb5_param_info(rb5_info,xpath_bgn,L_RB5_PARAM_VERBOSE);
        void *raw_arr=NULL;
        float *data_arr=NULL;
        return_param_blobid_raw(&(*rb5_info), &rb5_param, &raw_arr);
        convert_raw_to_data(&rb5_param,&raw_arr,&data_arr);
        
        int iray_peak_pwr=0;
        double avg_pwr=0.0;
        double sum_pwr=0.0;
        double cnt_pwr=0.0;
        for (i = 0; i < rb5_param.n_elems_data; i++) {
          if(data_arr[i] > 0.0) {
            sum_pwr+=data_arr[i];
            cnt_pwr+=1.0;
          }  
          if(data_arr[i] > data_arr[iray_peak_pwr]) {
             iray_peak_pwr=i;
          }
        } //for (i = 0; i < rb5_param.n_elems_data; i++) {
        if(cnt_pwr > 0.0) avg_pwr=sum_pwr/cnt_pwr;

	    ret = addDoubleAttribute(object, "how/peakpwr",     data_arr[iray_peak_pwr]/1000.); //[kW]
        ret = addDoubleAttribute(object, "how/avgpwr",      avg_pwr); //[W]

        if ( raw_arr != NULL ) RAVE_FREE( raw_arr);
        if (data_arr != NULL ) RAVE_FREE(data_arr);
    }
//*/

//	ret = addDoubleAttribute(object, "how/dynrange",    ); // n/a
//	ret = addDoubleAttribute(object, "how/RAC",         ); // n/a
//	ret = addStringAttribute(object, "how/BBC",         "False");
//	ret = addDoubleAttribute(object, "how/PAC",         ); // n/a
//	ret = addDoubleAttribute(object, "how/S2N",         ); // ?!?

    //#############################################################################//
	/* Set mandatory 'where' attributes, Table 4 in the ODIM_H5 spec. */

    PolarScan_setRstart (scan, rb5_info->slice_bin_range_bgn_km [this_slice]); /* Kilometers */
    PolarScan_setRscale (scan, rb5_info->slice_bin_range_res_km [this_slice]*1000.); /* Meters */
    //see for PolarScan_setXXX() defn: http://git.baltrad.eu/git/?p=rave.git;a=blob_plain;f=librave/toolbox/polarscan.c;hb=HEAD
    if (strcmp(rb5_info->scan_type,"ele") == 0) {
        //A1gate should be 0 (minimum ray startangle index)
        PolarScan_setA1gate (scan, 0); // Index of the first azimuth gate radiated in the scan
        // /data/dmichelson/projects/rave/librave/toolbox/polarscan.c:55 struct _PolarScan_t->azangle DOES NOT EXIST
        PolarScan_setElangle(scan, rb5_info->angle_deg_arr[this_slice]*DEG_TO_RAD); // FAKE to fool polar_odim_io.c check of "where/elangle"
    } else {
        PolarScan_setA1gate (scan, rb5_info->iray_0degN   [this_slice]); // Index of the first azimuth gate radiated in the scan
        PolarScan_setElangle(scan, rb5_info->angle_deg_arr[this_slice]*DEG_TO_RAD); /* Radians! Use constant DEG_TO_RAD if necessary. */
    } 

	//this is variable per slice to catch 361 or 721 rays case
	//non-existent func: PolarScan_setNrays(scan, rb5_info->nrays[this_slice]);
	//non-existent func: PolarScan_setNbins(scan, rb5_info->nbins[this_slice]);
	ret = addLongAttribute(object, "where/nrays", rb5_info->nrays[this_slice]);
	ret = addLongAttribute(object, "where/nbins", rb5_info->nbins[this_slice]);

	/* Determine number of moments/parameters per scan */
	np = rb5_info->n_rawdatas;

	/* Loop through the moments, populating a Toolbox object for each */
	L_RB5_PARAM_VERBOSE=0;
	for (i=0;i<np;i++) {
		PolarScanParam_t* param = RAVE_OBJECT_NEW(&PolarScanParam_TYPE);

		sprintf(xpath_bgn,"((/volume/scan/slice)[%2d]/slicedata/%s)[%2d]/",this_slice+1,"rawdata",i+1);
		rb5_param=get_rb5_param_info(rb5_info,xpath_bgn,L_RB5_PARAM_VERBOSE);

		ret = populateParam(param, &(*rb5_info), &rb5_param);

if(L_RB52ODIM_DEBUG) fprintf(stdout,"Adding rawdata = %s to scan...\n",rb5_param.sparam);

		ret = PolarScan_addParameter(scan, param);
		RAVE_OBJECT_RELEASE(param);
	}


	/* Detailed ray readout az and el angles and acquisition times. Helper function below. */
	ret = setRayAttributes(scan, &(*rb5_info), this_slice);

	/* We'll add appropriate exception handling later */
	return ret;
}


/*
 * Input object is an empty Toolbox core object ((object type to be determined below)).
 */
int populateObject(RaveCoreObject* object, strRB5_INFO *rb5_info) {
	int ret = 0;
	int nscans = 0;

	/* Determine number of scans == n_slices */
	nscans = rb5_info->n_slices;

    //rb5_util vars
    static char iso8601[MAX_STRING]="\0";
	static char radar_id[MAX_STRING]="\0";
	static char radar_name[MAX_STRING]="\0";
	static char radar_model[MAX_STRING]="\0";
	static char source[MAX_STRING]="\0";
	static char tmp_a[MAX_STRING]="\0";

    //#############################################################################//
	/*  Top-level 'what' attributes, Table 1 of the ODIM_H5 spec. */
    
	ret = addStringAttribute(object, "what/_creator_program", "rb52odim");
	ret = addStringAttribute(object, "what/_orig_file_format", strcat(strcpy(tmp_a,"Rainbow "),rb5_info->rainbow_version));
	ret = addStringAttribute(object, "what/_orig_file_name", rb5_info->inp_fullfile);

    /* Time is recorded according to object and sweep order:
     * If bottom-up volume or scan, it's the start of the (first/lowest) scan.
     * Otherwise, it's a top-down volume and it's the end of the last/lowest scan. */
    strcpy(iso8601,rb5_info->slice_iso8601_bgn[0]);
    if (RAVE_OBJECT_CHECK_TYPE(object, &PolarVolume_TYPE)) {
        if (! PolarVolume_isAscendingScans((PolarVolume_t*)object)) {
            strcpy(iso8601,rb5_info->slice_iso8601_end[nscans-1]);
        }
    }

    // Rainbow should be configured with a unique 3-char id & descriptive site name
    strcpy(radar_id  ,rb5_info->sensor_id);
    strcpy(radar_name,rb5_info->sensor_name);
    /*
    int i;
    for(i=0;radar_id[i];i++){ //cast each char to lower-case
        radar_id[i]=tolower(radar_id[i]);
    }
    for(i=0;radar_name[i];i++){ // blank all non-alphanumeric chars
        if(!isalnum(radar_name[i])) radar_name[i]=' ';
    }
    sprintf(source,"NOD:ca%s,PLC:%s",radar_id,
                                     radar_name);
    */
    // otherwise use mapSource2Nod()
    strcpy(source,mapSource2Nod(radar_id));

    if (RAVE_OBJECT_CHECK_TYPE(object, &PolarVolume_TYPE)) {
      PolarVolume_setDate     ((PolarVolume_t*)object,func_iso8601_2_yyyymmdd(iso8601));
      PolarVolume_setTime     ((PolarVolume_t*)object,func_iso8601_2_hhmmss(iso8601));
      PolarVolume_setSource   ((PolarVolume_t*)object,source);
      PolarVolume_setLongitude((PolarVolume_t*)object,rb5_info->sensor_lon_deg*DEG_TO_RAD);
      PolarVolume_setLatitude ((PolarVolume_t*)object,rb5_info->sensor_lat_deg*DEG_TO_RAD);
      PolarVolume_setHeight   ((PolarVolume_t*)object,rb5_info->sensor_alt_m); //m_asl
      PolarVolume_setBeamwidth((PolarVolume_t*)object,rb5_info->sensor_beamwidth_deg*DEG_TO_RAD);
    } else {
      PolarScan_setDate     ((PolarScan_t*)object,func_iso8601_2_yyyymmdd(iso8601));
      PolarScan_setTime     ((PolarScan_t*)object,func_iso8601_2_hhmmss(iso8601));
      PolarScan_setSource   ((PolarScan_t*)object,source);
      PolarScan_setLongitude((PolarScan_t*)object,rb5_info->sensor_lon_deg*DEG_TO_RAD);
      PolarScan_setLatitude ((PolarScan_t*)object,rb5_info->sensor_lat_deg*DEG_TO_RAD);
      PolarScan_setHeight   ((PolarScan_t*)object,rb5_info->sensor_alt_m); //m_asl
      PolarScan_setBeamwidth((PolarScan_t*)object,rb5_info->sensor_beamwidth_deg*DEG_TO_RAD);
    }

    //#############################################################################//
	/* Set optional 'how' attributes. There are lots! See Table 8 in the ODIM_H5 spec. */

	ret = addStringAttribute(object, "how/task", rb5_info->scan_name); //"The RB5 task name");
	strcpy(radar_model,mapSource2Model(radar_id));
	strcat(strcpy(tmp_a,"SELEX"),radar_model);
	ret = addStringAttribute(object, "how/system", tmp_a); //According to Table 10
	ret = addStringAttribute(object, "how/TXtype", "magnetron");
	ret = addStringAttribute(object, "how/poltype", "simultaneous-dual");
	ret = addStringAttribute(object, "how/polmode", "simultaneous-dual");
	ret = addStringAttribute(object, "how/software", "RAINBOW"); //According to Table 11
	ret = addStringAttribute(object, "how/sw_version", rb5_info->rainbow_version); //"major.minor.veryminor");
	ret = addStringAttribute(object, "how/simulated", "False");

	ret = addDoubleAttribute(object, "how/wavelength", rb5_info->sensor_wavelength_cm);

//	ret = addDoubleAttribute(object, "how/RXbandwidth", ); // n/a

//	ret = addDoubleAttribute(object, "how/TXlossH", ); // n/a
//	ret = addDoubleAttribute(object, "how/TXlossV", ); // n/a
//	ret = addDoubleAttribute(object, "how/injectlossH", ); // n/a
//	ret = addDoubleAttribute(object, "how/injectlossV", ); // n/a
//	ret = addDoubleAttribute(object, "how/RXlossH", ); // n/a
//	ret = addDoubleAttribute(object, "how/RXlossV", ); // n/a
//	ret = addDoubleAttribute(object, "how/radomelossH", ); // n/a
//	ret = addDoubleAttribute(object, "how/radomelossV", ); // n/a
//	ret = addDoubleAttribute(object, "how/antgainH", ); // n/a
//	ret = addDoubleAttribute(object, "how/antgainV", ); // n/a
	ret = addDoubleAttribute(object, "how/beamwH", rb5_info->sensor_beamwidth_deg);
	ret = addDoubleAttribute(object, "how/beamwV", rb5_info->sensor_beamwidth_deg);
//	ret = addDoubleAttribute(object, "how/gasattn", ); // n/a

    // NOTE, rest set at SCAN level: rpm, prf's pw, Nyquist, noise_power_dbz, nsamples

if(L_RB52ODIM_DEBUG) fprintf(stdout,"Done top-level 'how' attributes...\n");

	/* Populate each */
    int ireqSWEEP=0;
    //fprintf(stdout,"Populating with %2d scans...\n",nscans);
    if (RAVE_OBJECT_CHECK_TYPE(object, &PolarVolume_TYPE)) {
	  for (ireqSWEEP=0;ireqSWEEP<nscans;ireqSWEEP++) {
		PolarScan_t* scan = RAVE_OBJECT_NEW(&PolarScan_TYPE);
		ret = populateScan((PolarScan_t*)scan, &(*rb5_info), ireqSWEEP);
        if(ret != 1) {
          RAVE_OBJECT_RELEASE(scan);
          return -1;
        }
        //fprintf(stdout,"Adding scan = %2d (%4.1f deg) to PVOL...\n",ireqSWEEP,rb5_info->angle_deg_arr[ireqSWEEP]);
		ret = PolarVolume_addScan((PolarVolume_t*)object, scan);
		RAVE_OBJECT_RELEASE(scan);
	  }
    } else {
      /* Only one scan to populate */
      //fprintf(stdout,"Adding scan = %2d (%4.1f deg) to SCAN...\n",ireqSWEEP,rb5_info->angle_deg_arr[ireqSWEEP]);
      ret = populateScan((PolarScan_t*)object, &(*rb5_info), ireqSWEEP);
      if(ret != 1) {
        return -1;
      }
 
    }

	/* We'll add appropriate exception handling later */
	return ret;
}


/*
 * Reads an RB5 buffer and returns a RaveIO_t* with a complete payload.
 */
RaveIO_t* getRaveIObuf(const char* ifile, char **inp_buffer, size_t buffer_len) {
    RaveIO_t* RETURN_raveio_NULL = NULL;

//#############################################################################

    //get RB5 top level info
    char *inp_fname;
    inp_fname=(char *)ifile;
    strRB5_INFO rb5_info;
    strcpy(rb5_info.inp_fullfile,inp_fname);
//printf("GOT inp_fname = %s\n", inp_fname);
//printf("buffer_len= %ld\n", buffer_len);
//printf("READ buffer = %.250s\n",*inp_buffer);

    rb5_info.buffer_len=buffer_len;
    rb5_info.buffer=*inp_buffer;
//return RETURN_raveio_NULL;

    int L_VERBOSE=0;
    if(populate_rb5_info(&rb5_info,L_VERBOSE) != 0) {
      fprintf(stderr,"Error cannot process file = %s\n", inp_fname);
      return RETURN_raveio_NULL;
    }

//#############################################################################
    RaveIO_t* raveio = RAVE_OBJECT_NEW(&RaveIO_TYPE);
    RaveCoreObject* object = NULL;
    int rot = Rave_ObjectType_UNDEFINED;

    /* If the RB5 file contains a scan or a pvol, create equivalent object */
    rot = objectTypeFromRB5(rb5_info);
    if (rot == Rave_ObjectType_PVOL) {
      object = (RaveCoreObject*)RAVE_OBJECT_NEW(&PolarVolume_TYPE);
    } else {
      if(rot == Rave_ObjectType_SCAN) {
        object = (RaveCoreObject*)RAVE_OBJECT_NEW(&PolarScan_TYPE);
      } else return RETURN_raveio_NULL;
    }

    /* Map RB5 object(s) to Toolbox ones. */
    populateObject(object, &rb5_info);
    close_rb5_info(&rb5_info);

    /* Set the object into the I/O container */
    RaveIO_setObject(raveio, object);
    RAVE_OBJECT_RELEASE(object);

    return raveio;
}

/*
 * Reads an RB5 file and returns a RaveIO_t* with a complete payload.
 */
RaveIO_t* getRaveIO(const char* ifile) {
    RaveIO_t* RETURN_raveio_NULL = NULL;

//#############################################################################

    // ingest entire RB5 file into a buffer
    char *inp_fname;
    inp_fname=(char *)ifile;
    char *buffer=NULL;
    size_t buffer_len=read_rb5_file_2_buffer(inp_fname,&buffer);
    if(buffer_len == 0) {
      fprintf(stderr,"Error cannot process file = %s\n", inp_fname);
      return RETURN_raveio_NULL;
    }

    return getRaveIObuf(ifile,&buffer,buffer_len);

}

/**
 * Function name: is_regular_file
 * Intent: determines whether the given path is to a regular file
 */
int is_regular_file(const char *path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

/*
 * Verifies it is an RB5 raw buffer and of the compatible version.
 */
int isRainbow5buf(char **inp_buffer) {

	int RETURN_yes = 0;
	int RETURN_no = -1;

    char line[MAX_STRING]="\0";
    char *eol = strchr(*inp_buffer, '\n');
    size_t len=eol-*inp_buffer;
//    fprintf(stdout,"len : %ld\n",len);
    strncpy(line,*inp_buffer,len);
    line[len]='\0'; //add terminator
//    fprintf(stdout,"line : %s\n",line);

    char *substring="<volume version=\"";
    char match_val[MAX_STRING]="\0";
    char *match_bgn=NULL;
    char *match_end=NULL;
    int match_len=0;
    match_bgn=strstr(line,substring); //init match pointer
    match_bgn=match_bgn ? strchr(match_bgn  ,'\"') : 0; //1st " in match
    match_end=match_bgn ? strchr(match_bgn+1,'\"') : 0; //next " in match
    if(!(match_bgn && match_end)){
        fprintf(stdout,"Error: Cannot find <volume>\n");
        return RETURN_no;
    }
    ++match_bgn;
    match_len=match_end-match_bgn;
    strncpy(match_val,match_bgn,match_len);
    match_val[match_len]='\0';
//    fprintf(stdout,"match_val : %s\n",match_val);
    if(strcmp(match_val,MINIMUM_RAINBOW_VERSION) < 0){
        //fprintf(stdout,"Error: Incompatible Rainbow version, this is v%s, (v%s minumum)\n",match_val,MINIMUM_RAINBOW_VERSION);
        return RETURN_no;
    }

//    fprintf(stdout,"YES! This is a proper RB5 raw file : %s\n", inp_fname);
    return RETURN_yes;
}

/*
 * Verifies it is an RB5 raw file and of the compatible version.
 */
int isRainbow5(const char* inp_fname) {

//	int RETURN_yes = 0;
	int RETURN_no = -1;

    FILE *fp = NULL;
    fp = fopen(inp_fname, "r");
    if (NULL == fp) {
        fprintf(stderr,"Error while opening file = %s\n", inp_fname);
        return RETURN_no;
    }

    char *line=NULL;
    size_t len=0;
    getline(&line,&len,fp);
    fclose(fp);
//keep for subsequent '\n' line extraction in isRainbow5buf()
//    line[strlen(line)-1]='\0'; //trim trailing <LF>
//    fprintf(stdout,"line : %s\n",line);

    int RETURN_val=isRainbow5buf(&line);
    free(line);
    //RAVE_FREE(line);
    return RETURN_val;
}

/* START HELPER FUNCTIONS */


/*
 * Input object is an empty Toolbox polar scan object and a native RB5 object.
 */
int setRayAttributes(PolarScan_t* scan, strRB5_INFO *rb5_info, int this_slice) {
	int ret = 0;
//	long nrays = PolarScan_getNrays(scan); // use rb5_info.nrays

    static char iso8601_0[MAX_STRING]="\0";
    sprintf(iso8601_0,"%s",rb5_info->slice_iso8601_bgn[this_slice]);
    double systime_0=func_iso8601_2_systime(iso8601_0);

    //rb5_util vars
    strRB5_PARAM_INFO rb5_param;
    static char xpath_bgn[MAX_STRING]="\0";
    void *raw_arr=NULL;
    float *data_arr=NULL;
    int i;
    size_t this_nrays=rb5_info->nrays[this_slice];

    //RAVE attribs are either double or long arrays (need to cast)
    double *ddata_arr =(double *)RAVE_MALLOC(this_nrays*sizeof(double));
    long   *ldata_arr =(long   *)RAVE_MALLOC(this_nrays*sizeof(long  ));

    //mid_angle_readbacks
    if(strcmp(rb5_info->scan_type,"ele") == 0){
        for(i=0;i<this_nrays;i++) ddata_arr[i]=(rb5_info->slice_moving_angle_arr[this_slice])[i];
        RaveAttribute_t* el_attr = RaveAttributeHelp_createDoubleArray("how/elangles", ddata_arr, this_nrays);
        ret = PolarScan_addAttribute(scan, el_attr);
        RAVE_OBJECT_RELEASE(el_attr);
        for(i=0;i<this_nrays;i++) ddata_arr[i]=(rb5_info->slice_moving_angle_start_arr[this_slice])[i];
        RaveAttribute_t* el_attr1 = RaveAttributeHelp_createDoubleArray("how/startelA", ddata_arr, this_nrays);
        ret = PolarScan_addAttribute(scan, el_attr1);
        RAVE_OBJECT_RELEASE(el_attr1);
        for(i=0;i<this_nrays;i++) ddata_arr[i]=(rb5_info->slice_moving_angle_stop_arr[this_slice])[i];
        RaveAttribute_t* el_attr2 = RaveAttributeHelp_createDoubleArray("how/stopelA", ddata_arr, this_nrays);
        ret = PolarScan_addAttribute(scan, el_attr2);
        RAVE_OBJECT_RELEASE(el_attr2);
        for(i=0;i<this_nrays;i++) ddata_arr[i]=(rb5_info->slice_fixed_angle_arr[this_slice])[i];
        RaveAttribute_t* az_attr = RaveAttributeHelp_createDoubleArray("how/azangles", ddata_arr, this_nrays);
        ret = PolarScan_addAttribute(scan, az_attr);
        RAVE_OBJECT_RELEASE(az_attr);
        for(i=0;i<this_nrays;i++) ddata_arr[i]=(rb5_info->slice_fixed_angle_start_arr[this_slice])[i];
        RaveAttribute_t* az_attr1 = RaveAttributeHelp_createDoubleArray("how/startazA", ddata_arr, this_nrays);
        ret = PolarScan_addAttribute(scan, az_attr1);
        RAVE_OBJECT_RELEASE(az_attr1);
        for(i=0;i<this_nrays;i++) ddata_arr[i]=(rb5_info->slice_fixed_angle_stop_arr[this_slice])[i];
        RaveAttribute_t* az_attr2 = RaveAttributeHelp_createDoubleArray("how/stopazA", ddata_arr, this_nrays);
        ret = PolarScan_addAttribute(scan, az_attr2);
        RAVE_OBJECT_RELEASE(az_attr2);
    } else {
        for(i=0;i<this_nrays;i++) ddata_arr[i]=(rb5_info->slice_moving_angle_arr[this_slice])[i];
        RaveAttribute_t* az_attr = RaveAttributeHelp_createDoubleArray("how/azangles", ddata_arr, this_nrays);
        ret = PolarScan_addAttribute(scan, az_attr);
        RAVE_OBJECT_RELEASE(az_attr);
        for(i=0;i<this_nrays;i++) ddata_arr[i]=(rb5_info->slice_moving_angle_start_arr[this_slice])[i];
        RaveAttribute_t* az_attr1 = RaveAttributeHelp_createDoubleArray("how/startazA", ddata_arr, this_nrays);
        ret = PolarScan_addAttribute(scan, az_attr1);
        RAVE_OBJECT_RELEASE(az_attr1);
        for(i=0;i<this_nrays;i++) ddata_arr[i]=(rb5_info->slice_moving_angle_stop_arr[this_slice])[i];
        RaveAttribute_t* az_attr2 = RaveAttributeHelp_createDoubleArray("how/stopazA", ddata_arr, this_nrays);
        ret = PolarScan_addAttribute(scan, az_attr2);
        RAVE_OBJECT_RELEASE(az_attr2);
        for(i=0;i<this_nrays;i++) ddata_arr[i]=(rb5_info->slice_fixed_angle_arr[this_slice])[i];
        RaveAttribute_t* el_attr = RaveAttributeHelp_createDoubleArray("how/elangles", ddata_arr, this_nrays);
        ret = PolarScan_addAttribute(scan, el_attr);
        RAVE_OBJECT_RELEASE(el_attr);
        for(i=0;i<this_nrays;i++) ddata_arr[i]=(rb5_info->slice_fixed_angle_start_arr[this_slice])[i];
        RaveAttribute_t* el_attr1 = RaveAttributeHelp_createDoubleArray("how/startelA", ddata_arr, this_nrays);
        ret = PolarScan_addAttribute(scan, el_attr1);
        RAVE_OBJECT_RELEASE(el_attr1);
        for(i=0;i<this_nrays;i++) ddata_arr[i]=(rb5_info->slice_fixed_angle_stop_arr[this_slice])[i];
        RaveAttribute_t* el_attr2 = RaveAttributeHelp_createDoubleArray("how/stopelA", ddata_arr, this_nrays);
        ret = PolarScan_addAttribute(scan, el_attr2);
        RAVE_OBJECT_RELEASE(el_attr2);
    }

    int L_RB5_PARAM_VERBOSE=0;
    int this_rayinfo;
    for(this_rayinfo=0;this_rayinfo<rb5_info->n_rayinfos;this_rayinfo++){
      sprintf(xpath_bgn,"((/volume/scan/slice)[%2d]/slicedata/%s)[%2d]/",this_slice+1,"rayinfo",this_rayinfo+1);
      rb5_param=get_rb5_param_info(rb5_info,xpath_bgn,L_RB5_PARAM_VERBOSE);

      return_param_blobid_raw(&(*rb5_info), &rb5_param, &raw_arr);
      convert_raw_to_data(&rb5_param,&raw_arr,&data_arr);

if(L_RB52ODIM_DEBUG) fprintf(stdout,"Adding rayinfo = %s to scan...\n",rb5_param.sparam);

      // Note: angle readback not done here anymore, see above
      // added capture and logic to decode-side
      if(strcmp(rb5_param.sparam,"dataflag") == 0){
        for (i=0;i<this_nrays;i++) ldata_arr[i]=data_arr[i];
        RaveAttribute_t* dataflag_attr = RaveAttributeHelp_createLongArray("how/dataflag", ldata_arr, this_nrays);
        ret = PolarScan_addAttribute(scan, dataflag_attr);
	    RAVE_OBJECT_RELEASE(dataflag_attr);
        // add comment about dataflag bit encoding
        ret = addStringAttribute((RaveCoreObject*)scan, "how/comment",
            "From RB5_FileFormat_5430.pdf, Sec 2.3.2.1.1: Array 'rayinfo'\n"
            "dataflag 16-bits:\n"
            "0x0001 = signal processing error\n"
            "0x0002 = pulse error\n"
            "0x0004 = digital AFC step during CPI occured\n"
            "0x0008 = not used\n"
            "0x0010 = not used\n"
            "0x0020 = not used\n"
            "0x0040 = not used\n"
            "0x0080 = not used\n"
            "0x0100 = high PRF indication for fixed dual PRF mode\n"
            "0x0200 = TX power above limit (default: 120% of nominal value)\n"
            "0x0400 = TX power below limit (default: 80% of nominal power)\n"
            "0x0600 = (= 0x0200 | 0x0400) TX power below critical limit (default: 50% of nominal power)\n"
            "0x0800 = not used\n"
            "0x1000 = not used\n"
            "0x2000 = not used\n"
            "0x4000 = not used\n"
            "0x8000 = not used");
      }else if(strcmp(rb5_param.sparam,"numpulses") == 0){
        for (i=0;i<this_nrays;i++) ldata_arr[i]=data_arr[i];
        RaveAttribute_t* numpulses_attr = RaveAttributeHelp_createLongArray("how/numpulses", ldata_arr, this_nrays);
        ret = PolarScan_addAttribute(scan, numpulses_attr);
	    RAVE_OBJECT_RELEASE(numpulses_attr);
      }else if(strcmp(rb5_param.sparam,"timestamp") == 0){ //EPOCH SECONDS
        for (i=0;i<this_nrays;i++) ddata_arr[i]=(double)data_arr[i]/1000. + systime_0;
        RaveAttribute_t* startazT_attr = RaveAttributeHelp_createDoubleArray("how/startazT", ddata_arr, this_nrays);
        ret = PolarScan_addAttribute(scan, startazT_attr);
	    RAVE_OBJECT_RELEASE(startazT_attr);
      }else if(strcmp(rb5_param.sparam,"txpower") == 0){ //KILOWATTS
        for (i=0;i<this_nrays;i++) ddata_arr[i]=(double)data_arr[i]/1000.;
        RaveAttribute_t* txpower_attr = RaveAttributeHelp_createDoubleArray("how/TXpower", ddata_arr, this_nrays);
        ret = PolarScan_addAttribute(scan, txpower_attr);
	    RAVE_OBJECT_RELEASE(txpower_attr);
      }else if(strcmp(rb5_param.sparam,"noisepowerh") == 0){ //UNITS?!?
        for (i=0;i<this_nrays;i++) ldata_arr[i]=data_arr[i];
        RaveAttribute_t* noisepowerh_attr = RaveAttributeHelp_createLongArray("how/noisepowerh", ldata_arr, this_nrays);
        ret = PolarScan_addAttribute(scan, noisepowerh_attr);
	    RAVE_OBJECT_RELEASE(noisepowerh_attr);
      }else if(strcmp(rb5_param.sparam,"noisepowerv") == 0){ //UNITS?!?
        for (i=0;i<this_nrays;i++) ldata_arr[i]=data_arr[i];
        RaveAttribute_t* noisepowerv_attr = RaveAttributeHelp_createLongArray("how/noisepowerv", ldata_arr, this_nrays);
        ret = PolarScan_addAttribute(scan, noisepowerv_attr);
	    RAVE_OBJECT_RELEASE(noisepowerv_attr);
      }

      if ( raw_arr != NULL ) RAVE_FREE( raw_arr);
      if (data_arr != NULL ) RAVE_FREE(data_arr);

    } //for(this_rayinfo=0;this_rayinfo<rb5_info->n_rayinfos;this_rayinfo++){

    if (ddata_arr != NULL ) RAVE_FREE(ddata_arr);
    if (ldata_arr != NULL ) RAVE_FREE(ldata_arr);

	/* We'll add appropriate exception handling later */
	return ret;
}


/*
 * Helper to add a long integer attribute to a Toolbox object.
 */
int addLongAttribute(RaveCoreObject* object, const char* name, long value) {
	int ret = 0;
	RaveAttribute_t* attr;

	attr = RaveAttributeHelp_createLong(name, value);
	ret = addAttribute(object, attr);

	RAVE_OBJECT_RELEASE(attr);
	return ret;
}


/*
 * Helper to add a double attribute to a Toolbox object.
 */
int addDoubleAttribute(RaveCoreObject* object, const char* name, double value) {
	int ret = 0;
	RaveAttribute_t* attr;

	attr = RaveAttributeHelp_createDouble(name, value);
	ret = addAttribute(object, attr);

	RAVE_OBJECT_RELEASE(attr);
	return ret;
}


/*
 * Helper to add a string attribute to a Toolbox object.
 */
int addStringAttribute(RaveCoreObject* object, const char* name, const char* value) {
	int ret = 0;
	RaveAttribute_t* attr;

	attr = RaveAttributeHelp_createString(name, value);
	ret = addAttribute(object, attr);

	RAVE_OBJECT_RELEASE(attr);
	return ret;
}


/*
 * Helper to actually add a preset attribute to a Toolbox object.
 */
int addAttribute(RaveCoreObject* object, RaveAttribute_t* attr) {
	int ret = 0;

	if (RAVE_OBJECT_CHECK_TYPE(object, &PolarVolume_TYPE)) {
		ret = PolarVolume_addAttribute((PolarVolume_t*)object, attr);
	} else if (RAVE_OBJECT_CHECK_TYPE(object, &PolarScan_TYPE)) {
		ret = PolarScan_addAttribute((PolarScan_t*)object, attr);
	} else if (RAVE_OBJECT_CHECK_TYPE(object, &PolarScanParam_TYPE)) {
		ret = PolarScanParam_addAttribute((PolarScanParam_t*)object, attr);
	} else ret = 1;

	return ret;
}

/**
 * Function name: mapSource2Nod
 * Intent: A function to Mapping of RB5 site key name to ODIM node identifier.
 * Input:
 * (1) a pointer to a character string holding an RB5 radar site name
 * Output:
 * (1) a pointer to the address of a literal holding an ODIM node identifier.
 */
char* mapSource2Nod(const char* key) {
   if      (!strcmp(key,"XKR"))              return "NOD:caxkr,PLC:King City ON";
   else if (!strcmp(key,"XAH"))              return "NOD:caxah,PLC:Albert Head BC";
   else if (!strcmp(key,"XXY"))              return "NOD:caxxy,PLC:Whitehorse YT";
   else if (!strcmp(key,"XFB"))              return "NOD:caxfb,PLC:Iqaluit NU";
   else if (!strcmp(key,"XSO"))              return "NOD:caxso,PLC:Exeter ON";
   else if (!strcmp(key,"XBL"))              return "NOD:caxbl,PLC:Blainville QC";
   else 	                                 return "NOD:ca---,PLC:Unknown";
   return NULL;
}

/**
 * Function name: mapSource2Model. Lots of redundancy, but doing it this way makes it easier for others to add their radars.
 * Intent: A function to Mapping of RB5 site key name to SELEX radar model name.
 * Input:
 * (1) a pointer to a character string holding an RB5 radar site name
 * Output:
 * (1) a pointer to the address of a literal holding the radar's model name.
 */
char* mapSource2Model(const char* key) {
	if      (!strcmp(key,"XKR"))              return "60DX";
	else if (!strcmp(key,"XAH"))              return "60DX";
	else if (!strcmp(key,"XXY"))              return "60DX";
	else if (!strcmp(key,"XFB"))              return "60DX";
	else if (!strcmp(key,"XSO"))              return "60DX";
	else if (!strcmp(key,"XBL"))              return "60DX";
	else 	                                  return "";
	return NULL;
}

/* END HELPER FUNCTIONS */
