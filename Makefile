###########################################################################
# Copyright (C) 2016 The Crown (i.e. Her Majesty the Queen in Right of Canada)
#
# This file is an add-on to RAVE.
#
# RAVE is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# RAVE is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
# 
# You should have received a copy of the GNU Lesser General Public License
# along with RAVE.  If not, see <http://www.gnu.org/licenses/>.
# ------------------------------------------------------------------------
# 
# rb52odim Makefile
# @file
# @author Daniel Michelson and Peter Rodriguez, Environment and Climate Change Canada
# @date 2016-08-17
###########################################################################
.PHONY: all src modules test doc install

all:		src modules

src:
		$(MAKE) -C src

modules:
		$(MAKE) -C modules

test:
		@chmod +x ./tools/test_rb52odim.sh
		@./tools/test_rb52odim.sh

doc:
		$(MAKE) -C doxygen doc

install:
		$(MAKE) -C src install
		$(MAKE) -C modules install
		$(MAKE) -C Lib install
		$(MAKE) -C bin install
		$(MAKE) -C config install

.PHONY=clean
clean:
		$(MAKE) -C src clean
		$(MAKE) -C modules clean
		$(MAKE) -C doxygen clean

.PHONY=distclean		 
distclean:	clean
		$(MAKE) -C src distclean
		$(MAKE) -C modules distclean
		$(MAKE) -C doxygen distclean
