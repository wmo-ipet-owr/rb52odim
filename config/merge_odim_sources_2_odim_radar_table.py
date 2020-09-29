#!/usr/bin/env python

import os,sys,re
progname=os.path.basename(__file__)

from datetime import datetime
bgn_timestamp='{0}'.format(datetime.now().strftime("%Y-%m-%d %H:%M:%S"))

import logging
log_file='{0}.log'.format(progname)
# create root logger
logger=logging.getLogger()
logger.setLevel(logging.INFO)
# create log_file handler
flogh=logging.FileHandler(log_file,mode='w')
flogh.setLevel(logging.INFO)
# create console handler
clogh=logging.StreamHandler()
clogh.setLevel(logging.INFO)
# add the handlers to the root logger
logger.addHandler(flogh)
logger.addHandler(clogh)

logging.info('Running {0} @ {1}'.format(progname,bgn_timestamp))

odim_source_file='odim_source.rave_ec.2020-09.xml'
sBGN_SOURCE_CANADA='<ca CCCC="CWAO" org="53">'
sEND_SOURCE_CANADA='</ca>'

odim_table_file='odim_radar_table.xml'
sINDENT='    ' #4-spaces
table_ver='1.0'
table_author='Peter Rodriguez'
table_updated=bgn_timestamp[:10]
out_fp=open(odim_table_file,'w')
out_fp.write('<?xml version="1.0" encoding="UTF-8"?>\n')
out_fp.write('<table ver="{0}" author="{1}" updated="{2}">\n'.format(table_ver,table_author,table_updated))

L_MERGE=True
to_merge_odim_table_file='odim_radar_table.STB_research_Xbands.xml'
sBGN_MERGE_TABLE='<table'
sEND_MERGE_TABLE='</table>'

from collections import OrderedDict
ordered_dict_keys=[\
    'radar_id'    ,\
    'bgn_date'    ,\
    'end_date'    ,\
    'label'       ,\
    'operator'    ,\
    'band'        ,\
    'odim_node'   ,\
    'locale'      ,\
    'admin_state' ,\
    'country'     ,\
    'wmo_id'      ,\
    'rf_call_sign',\
    'make'        ,\
    'model'       ,\
    'serial'      ,\
    'txtype'      ,\
    'poltype'     ,\
    'comment'     ,\
]
dict_radar_template=OrderedDict((key,'') for key in ordered_dict_keys)
dict_radar_template['operator']='NRP'
dict_radar_template['country' ]='Canada'
dict_radar_template['comment' ]='Auto-merged entry from : '+odim_source_file

dict_Sband=dict_radar_template.copy()
dict_Sband['band'   ]='S'
dict_Sband['make'   ]='SELEX/Rainbow'
dict_Sband['model'  ]='1700S'
dict_Sband['txtype' ]='klystron'
dict_Sband['poltype']='simultaneous-dual'

dict_Xband=dict_radar_template.copy()
dict_Xband['band'   ]='X'
dict_Xband['make'   ]='SELEX/Rainbow'
dict_Xband['model'  ]='60DX'
dict_Xband['txtype' ]='magnetron'
dict_Xband['poltype']='simultaneous-dual'

# not output to odim_table_file
NRP_exception_X_ids=['ca'+s for s in ['xra','xsi','xsm','xss','xti','xwl']] #pre-CWRRP, these are Cbands
dict_Cband=dict_radar_template.copy()
dict_Cband['band'   ]='C'
dict_Cband['make'   ]='Vaisala/IRIS'
dict_Cband['model'  ]=''
dict_Cband['txtype' ]='magnetron'
dict_Cband['poltype']='single'

with open(odim_source_file,'r') as fp:
    match=False
    for line in fp:
        if sBGN_SOURCE_CANADA in line:
            match=True
            sOUT='<!-- Converted : {0} -->'.format(odim_source_file)
            logging.info(sOUT)
            out_fp.write(sOUT+'\n')
            continue
        elif sEND_SOURCE_CANADA in line:
            match=False
            continue
        elif match:
            logging.info(line.rstrip())
            (node,plc)=list(filter(None,re.split('<| plc="|">.*',line.strip())))
            (locale,admin_state)=plc.rsplit(' ',1)
            this_dict_radar=None
            if node[2] == 's':
                this_dict_radar=dict_Sband.copy()
            elif node[2] == 'x':
                if node not in NRP_exception_X_ids:
                    this_dict_radar=dict_Xband.copy()
            if this_dict_radar:
                this_dict_radar['radar_id'   ]=node.upper()
                this_dict_radar['odim_node'  ]=node
                this_dict_radar['locale'     ]=locale
                this_dict_radar['admin_state']=admin_state
                this_dict_radar['label'      ]='{band}-band at {locale}, {admin_state} ({operator})' \
                                               .format(**this_dict_radar)
                out_fp.write('{0}<radar id="{radar_id}" bgn_date="{bgn_date}" end_date="{end_date}">\n' \
                    .format(sINDENT,**this_dict_radar))
                for key in ordered_dict_keys[3:]:
                    out_fp.write('{0}{1}<{2}>{3}</{2}>\n'.format(sINDENT,sINDENT,key,this_dict_radar[key]))
                out_fp.write('{0}</radar>\n'.format(sINDENT))

if L_MERGE:
    merge_content_arr=[]
    with open(to_merge_odim_table_file,'r') as fp:
        match=False
        for line in fp:
            if sBGN_MERGE_TABLE in line:
                match=True
                sOUT='<!-- Merged : {0} -->'.format(to_merge_odim_table_file)
                logging.info(sOUT)
                out_fp.write(sOUT+'\n')
                continue
            elif sEND_MERGE_TABLE in line:
                match=False
                continue
            elif match:
                if '<radar id=' in line: logging.info(line.rstrip())
                merge_content_arr.append(line.rstrip())
    for sOUT in merge_content_arr:
        out_fp.write(sOUT+'\n')

out_fp.write('</table>\n')
out_fp.write('<!-- END XML -->\n')
logging.info('Created : {0}'.format(odim_table_file))
