#!/bin/env python

import os,sys,re
from datetime import date

odim_source_file='odim_source.rave_ec.2020-09.xml'
sBGN_SOURCE_CANADA='<ca CCCC="CWAO" org="53">'
sEND_SOURCE_CANADA='</ca>'

odim_table_file='odim_radar_table.xml'
sINDENT='    ' #4-spaces
table_ver='1.0'
table_author='Peter Rodriguez'
table_updated=date.today().strftime("%Y-%m-%d")
out_fp=open(odim_table_file,'w')
out_fp.write('<?xml version="1.0" encoding="UTF-8"?>\n')
out_fp.write('<table ver="'+table_ver+'" author="'+table_author+'" updated="'+table_updated+'">\n')

L_MERGE=True
to_merge_odim_table_file='odim_radar_table.STB_research_Xbands.xml'
sBGN_MERGE_TABLE='<table'
sEND_MERGE_TABLE='</table>'

dict_radar_template={ \
    'radar_id'    :'', \
    'bgn_date'    :'', \
    'end_date'    :'', \
    'label'       :'', \
    'operator'    :'RUAD', \
    'band'        :'', \
    'odim_node'   :'', \
    'locale'      :'', \
    'admin_state' :'', \
    'country'     :'Canada', \
    'wmo_id'      :'', \
    'rf_call_sign':'', \
    'make'        :'', \
    'model'       :'', \
    'serial'      :'', \
    'txtype'      :'', \
    'poltype'     :'', \
    'comment'     :'Auto-merged entry from : '+odim_source_file, \
    }

dict_Sband=dict_radar_template
dict_Sband['band'   ]='S'
dict_Sband['make'   ]='SELEX/Rainbow'
dict_Sband['model'  ]='1700S'
dict_Sband['txtype' ]='klystron'
dict_Sband['poltype']='simultaneous-dual'

dict_Xband=dict_radar_template
dict_Xband['band'   ]='X'
dict_Xband['make'   ]='SELEX/Rainbow'
dict_Xband['model'  ]='60DX'
dict_Xband['txtype' ]='magnetron'
dict_Xband['poltype']='simultaneous-dual'

# not output to odim_table_file
NRP_exception_X_ids=['ca'+s for s in ['xra','xsi','xsm','xss','xti','xwl']] #pre-CWRRP, these are Cbands
dict_Cband=dict_radar_template
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
            out_fp.write('<!-- '+'Converted : '+odim_source_file+' -->\n')

            continue
        elif sEND_SOURCE_CANADA in line:
            match=False
            continue
        elif match:
            print(line.rstrip())
            (node,plc)=list(filter(None,re.split('<| plc="|">.*',line.strip())))
            (locale,admin_state)=plc.rsplit(maxsplit=1)
            this_dict_radar=None
            if node[2] == 's':
                this_dict_radar=dict_Sband
            elif node[2] == 'x':
                if node not in NRP_exception_X_ids:
                    this_dict_radar=dict_Xband
            if this_dict_radar:
                this_dict_radar['radar_id'   ]=node.upper()
                this_dict_radar['label'      ]=this_dict_radar['band']+'-band'+\
                    ' at '+this_dict_radar['locale']+', '+this_dict_radar['admin_state']+\
                    ' ('+this_dict_radar['operator']+')'
                this_dict_radar['odim_node'  ]=node
                this_dict_radar['locale'     ]=locale
                this_dict_radar['admin_state']=admin_state
                out_fp.write(sINDENT+\
                    '<radar id="'+this_dict_radar['radar_id']+'" '+ \
                    'bgn_date="'+this_dict_radar['bgn_date']+'" '+ \
                    'end_date="'+this_dict_radar['end_date']+'">\n')
                for key in list(dict_radar_template.keys())[3:]:
                    out_fp.write(sINDENT+sINDENT+\
                        '<'+key+'>'+this_dict_radar[key]+'</'+key+'>\n')
                out_fp.write(sINDENT+'</radar>\n')

if L_MERGE:
    merge_content_arr=[]
    with open(to_merge_odim_table_file,'r') as fp:
        match=False
        for line in fp:
            if sBGN_MERGE_TABLE in line:
                match=True
                out_fp.write('<!-- '+'Merged : '+to_merge_odim_table_file+' -->\n')
                continue
            elif sEND_MERGE_TABLE in line:
                match=False
                continue
            elif match:
                print(line.rstrip())
                merge_content_arr.append(line.rstrip())
    for line in merge_content_arr:
        out_fp.write(line+'\n')

out_fp.write('<!-- END XML -->\n')
print('Created :',odim_table_file)

