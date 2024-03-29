#!/usr/bin/env python
'''
Copyright (C) 2012- Swedish Meteorological and Hydrological Institute (SMHI)

This file is part of the bRopo extension to RAVE.

RAVE is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RAVE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with RAVE.  If not, see <http://www.gnu.org/licenses/>.
'''
## 
# @file
# @author Anders Henja, SMHI
# @date 2016-10-17
# @author Peter Rodriguez, ECCCC
# @date 2020-12-09
#
import sys, os, errno
import _raveio, _polarvolume, _polarscan, _rave
import datetime, math

ACQUISITION_UPDATE_TIME = 6  # minutes

### Adjust to closest nomimal time as per ../Lib/rb52odim.py, compileVolumeFromVolumes()
# not using create_nominal_time_str() below

## Rounds date and time to the nearest acquisition interval (minute past hour),
# assuming it is regular and starting at minute 0.
# @param string date in YYYYMMDD format
# @param string time in HHmmSS format
# @param int update interval in minutes
# @returns tuple containing date and time in the same format as input arguments
def roundDT(DATE, TIME, INTERVAL=ACQUISITION_UPDATE_TIME):
    HINTERVAL = INTERVAL / 2.0
    tm = datetime.datetime(int(DATE[:4]), int(DATE[4:6]), int(DATE[6:8]),
                           int(TIME[:2]), int(TIME[2:4]), int(TIME[4:]))
    tm += datetime.timedelta(minutes=HINTERVAL)
    tm -= datetime.timedelta(minutes=tm.minute % INTERVAL,
                             seconds=tm.second,microseconds=tm.microsecond)
    return tm.strftime('%Y%m%d'), tm.strftime('%H%M%S')

##
# Helps with merging scans or volumes containing different parameters but where date/time & source matches. The
# date/time is matched on a nominal time base. Which means that if for example interval = 15, then scans from 00:00 -> 14:59
# will be assumed to have the same nominal time (00:00).
#
class polar_merger(object):
  ##
  # Constructor
  # @param interval the nominal time intervals. 
  def __init__(self, interval=ACQUISITION_UPDATE_TIME):
    self.interval=interval

  ##
  # Merges a list of files. The file names should point to either polar volumes or polar scans
  # and the what/date, what/time, what/source and eventually where/elangle needs to match.
  # @param fstrs - a list of file strings
  # @return a rave object
  def merge_files(self, fstrs):
    objs = []
    for f in fstrs:
      print("Processing : %s" % f)
      rio = _raveio.open(f)
      if not rio.objectType in (_rave.Rave_ObjectType_PVOL, _rave.Rave_ObjectType_SCAN):
        raise TypeError("Can only merge volumes and scans")
      objs.append(rio.object)
    
    return self.merge(objs)

  def merge(self, robjs):
    source = None
    dtstr = None
    for obj in robjs:
      objdtstr = self.create_nominal_time_str(obj.date, obj.time)
      if source is None and dtstr is None:
        source = obj.source
        dtstr=objdtstr

      if source != obj.source or dtstr != objdtstr:
        raise TypeError("source, date and time must be identical when merging")

    self._verify_elangles(robjs)

    result = self._merge_files(robjs)

    # Adjust to closest nomimal time
    if result.isAscendingScans():
        result.date, result.time  = roundDT(result.date, result.time, INTERVAL=self.interval)
    else:
        lowest = result.getScanClosestToElevation(-90.0, 0)
        result.date, result.time  = roundDT(lowest.enddate, lowest.endtime, INTERVAL=self.interval)
    print(result.date, result.time)

#    dtstr= self.create_nominal_time_str(result.date, result.time)
#    result.date = dtstr[:8]
#    result.time = dtstr[8:]
    
    return result

  ##
  # If incoming objects are all scans, then the elevation angles must be same in order to merge files.
  # If at least one of the incoming objects are a volume, we can handle any elevation angles since we will
  # just fill the volume.
  #
  # 
  def _verify_elangles(self, pos):
    gotvolume=False
    elangle = None
    for po in pos:
      if _polarvolume.isPolarVolume(po):
        gotvolume=True

    if not gotvolume:
      for po in pos:
        if elangle is None:
          elangle = po.elangle
        if po.elangle != elangle:
          raise TypeError("When merging scans elevation angles between files must be same.")

  def _merge_files(self, pos):
    result = None
    # First we want to check if one of these items is a volume since it makes more sence to add
    # scans to a volume than having to check if a later object is volume and then remerge
    for i in range(len(pos)):
      if _polarvolume.isPolarVolume(pos[i]):
        result = pos[i]
        del pos[i]
        break

    if result is None:
      result = pos[0]
      del pos[0]

    for po in pos:
      self._add_object_to(po, result)

    return result

  ##
  #
  def _add_object_to(self, srco, tgto):
    if _polarvolume.isPolarVolume(srco) and _polarscan.isPolarScan(tgto):
      raise TypeError("Can not merge a volume into a scan")

    # Is srco is a polarvolume, we use a recursive back with the individual scans    
    if _polarvolume.isPolarVolume(srco):
      for i in range(srco.getNumberOfScans()):
        s = srco.getScan(i)
        self._add_object_to(s, tgto)
      return

    # From here on we know that srco always is a scan
    tgtscan = tgto
    if _polarvolume.isPolarVolume(tgto):
      cscan = tgto.getScanClosestToElevation(srco.elangle, 0)
      if cscan.elangle != srco.elangle:
        tgto.addScan(srco)
      else:
        self._merge_parameters(srco, cscan)
    else:
      self._merge_parameters(srco, tgtscan)

  ##
  # Merges the parameters from srcscan into tgtscan. If parameter already exists in tgtscan no merging of parameter is performed.
  #
  def _merge_parameters(self, srcscan, tgtscan):
    names = srcscan.getParameterNames()
    for n in names:
      if not tgtscan.hasParameter(n):
        tgtscan.addParameter(srcscan.getParameter(n))
  ##
  # Creates a nominal date / time string
  def create_nominal_time_str(self, dstr, tstr):
    year = int(dstr[:4])
    month = int(dstr[4:6])
    mday = int(dstr[6:8])
    hour = int(tstr[:2])
    minute = int(tstr[2:4])
    minute = minute - minute%self.interval
    return datetime.datetime(year,month,mday,hour,minute,0).strftime("%Y%m%d%H%M%S")

if __name__=="__main__":
#    files=["plbrz_pvol_20161014T0800Z_0x1.h5","plbrz_pvol_20161014T0800Z_0x4.h5"]
#  
#    files=["deboo_scan_0.5_20161017T1500Z_0x1.h5","deboo_scan_0.5_20161017T1500Z_0x4.h5"]
#  
#    #FAIL: TypeError: When merging scans elevation angles between files must be same.  
#    files=["tmp_work/XAH/2015-12-09/DOPVOL1_A.azi/XAH.2015-12-09_1650Z.DOPVOL1_A.azi.h5",
#           "tmp_work/XAH/2015-12-09/DOPVOL1_B.azi/XAH.2015-12-09_1650Z.DOPVOL1_B.azi.h5",
#           "tmp_work/XAH/2015-12-09/DOPVOL1_C.azi/XAH.2015-12-09_1650Z.DOPVOL1_C.azi.h5"]
#    out_filename="tmp_work/merged.XAH.2015-12-09_1650Z.DOPVOL1.h5"
#  
#    #../test/script.to_make_ref_files.sh
#    files=["../test/CASRA_2017121520000300dBuZ.vol.h5.tmp",
#           "../test/CASRA_2017121520000300dBZ.vol.h5.tmp",
#           "../test/CASRA_2017121520000300PhiDP.vol.h5.tmp",
#           "../test/CASRA_2017121520000300RhoHV.vol.h5.tmp",
#           "../test/CASRA_2017121520000300SQI.vol.h5.tmp",
#           "../test/CASRA_2017121520000300uPhiDP.vol.h5.tmp",
#           "../test/CASRA_2017121520000300V.vol.h5.tmp",
#           "../test/CASRA_2017121520000300W.vol.h5.tmp",
#           "../test/CASRA_2017121520000300ZDR.vol.h5.tmp"]
#    out_filename="../test/CASRA_20171215200003_pvol.ref.h5"

    from optparse import OptionParser

    usage = "usage: %prog -i <input simple param ODIM_H5 files> -o <output multi-param ODIM_H5 file> [h]"
    parser = OptionParser(usage=usage)

    parser.add_option("-i", "--input", dest="ifiles",
                      help="Sequence of comma-separated input ODIM_H5 file names. No white spaces allowed.")

    parser.add_option("-o", "--output", dest="ofile",
                      help="Output ODIM_H5 file name.")

    (options, args) = parser.parse_args()

    if not options.ifiles or not options.ofile:
        parser.print_help()
        sys.exit(errno.EINVAL)

    files=options.ifiles.split(",")
    out_filename=options.ofile

    rio = _raveio.new()
    rio.object = polar_merger().merge_files(files)
    rio.save(out_filename)
    print("Created : %s" % out_filename)
