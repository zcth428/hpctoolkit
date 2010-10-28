#!/usr/bin/env python
'''
Use nm to extract the symbol table info for start/end of functions.
Put the data into a C array for later compilation.
Start / end data ultimately used by unwinder
'''
import os
import sys
import re

t_re_s = r'^ (?:0*)([1-9a-f][0-9a-f]*) \s+ (.) \s+ (\S+)$'
t_re   = re.compile(t_re_s,re.X)

c_prog = '''
unsigned long csprof_nm_addrs[] = {
%s
};
int csprof_nm_addrs_len  = sizeof(csprof_nm_addrs) / sizeof(csprof_nm_addrs[0]);
'''
def process(prog):
#    (pathn,base) = os.path.split(prog)
#    c_prog_name = os.path.join(sys.path[0],c_prog_name)
#    so_name     = os.path.join(sys.path[0],'nm.%s.so' % base)

    c_prog_name = '%s.nm.c'  % prog
    so_name     = '%s.nm.so' % prog

    seg_addrs = []
    for l in os.popen('nm -v %s' % prog):
        m = t_re.match(l)
        if m and m.group(2) == 'T':
            seg_addrs.append('0x'+m.group(1))
    c_prog_f = file(c_prog_name,'w')
    addr_s   = '  ' + ',\n  '.join(seg_addrs)
    c_prog_f.write(c_prog % addr_s)
    c_prog_f.close()
    os.system('gcc %s -fPIC -shared -nostartfiles -o %s' %(c_prog_name,so_name))

if __name__ == '__main__':
    process(sys.argv[1])
