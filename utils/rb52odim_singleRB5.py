#!/usr/bin/env python
'''
Copyright (C) 2017 The Crown (i.e. Her Majesty the Queen in Right of Canada)

This file is an add-on to RAVE.

RAVE is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RAVE and this software are distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with RAVE.  If not, see <http://www.gnu.org/licenses/>.

'''
## Command-line utility for converting a single Rainbow 5 file into a single
#  ODIM_H5 file.

## 
# @file
# @author Daniel Michelson and Peter Rodriguez, Environment and Climate Change Canada
# @date 2017-06-09

# NOTE: this code is the basis for ../Lib/rb52odim.py:singleRB5()

# Peter, 2020-Dec-10
# force use of LOCAL copy of rb52odim.py
# Let's be independent from ${RAVEROOT}=/opt/baltrad, for example
import sys
sys.path.insert(1,'../Lib/')
sys.path.insert(1,'../modules/')

import sys, os, errno
import _rave, _raveio

import _rb52odim #Note using ../modules/_rb52odim.so
# NOTE: but baked w/ /opt/baltrad/rave/lib/librb52odim.so?
# NOTE: fixed make install w/ chmod 775
#from rb52odim import * #Note using ../Lib/rb52odim.py

## Reads RB5 files and merges their contents into an output ODIM_H5 file
# @param optparse.OptionParser object
def singleRB5(options):
    inp_fullfile = options.ifile
    out_fullfile = options.ofile

    #TMPFILE = False
    #validate(inp_fullfile)
    #orig_ifile = copy(inp_fullfile)
    #if mimetypes.guess_type(inp_fullfile)[1] == 'gzip':
    #    inp_fullfile = gunzip(inp_fullfile)
    #    TMPFILE = True
    if not _rb52odim.isRainbow5(inp_fullfile):
        raise IOError("%s is not a proper RB5 raw file" % inp_fullfile)
    #    raise IOError("%s is not a proper RB5 raw file" % orig_ifile)
    rio = _rb52odim.readRB5(inp_fullfile)

    #if TMPFILE:
    #    os.remove(inp_fullfile)

    rio.save(out_fullfile)
    print("Created : %s" % out_fullfile)


if __name__=="__main__":
    from optparse import OptionParser

    usage = "usage: %prog -i <input RB5 file> -o <output ODIM_H5 file> [h]"
    parser = OptionParser(usage=usage)

    parser.add_option("-i", "--input", dest="ifile",
                      help="Single input Rainbow 5 file name.")

    parser.add_option("-o", "--output", dest="ofile",
                      help="Output ODIM_H5 file name.")

    (options, args) = parser.parse_args()

    if not options.ifile or not options.ofile:
        parser.print_help()
        sys.exit(errno.EINVAL)        

    if options.ifile:

        # Rudimentary input file validation
        fstr = options.ifile
        if not os.path.isfile(fstr):
            print("%s is not a regular file. Bailing ..." % fstr)
            sys.exit(errno.ENOENT)
        if os.path.getsize(fstr) == 0:
            print("%s is zero-length. Bailing ..." % fstr)
            sys.exit(-1)

    singleRB5(options)
