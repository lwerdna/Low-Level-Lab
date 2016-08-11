#!/usr//bin/env python
import os
import re
import sys

pathResources = './rsrc'

for root, dirs, files in os.walk(pathResources):
    for fname in files:
        fpath = os.path.join(pathResources, fname);
        fobj = open(fpath, "rb")
        stuff = fobj.read()
        fobj.close()

        # non alphanumerics replaced with underscore
        arrname = 'rsrc_' + re.sub(r'[^\w]', '_', fname);

        if sys.argv[1:] and sys.argv[1] == 'header':
            print '// from file %s' % fpath
            print 'extern unsigned char %s[%d];' % (arrname, len(stuff))
        elif sys.argv[1:] and sys.argv[1] == 'source':
            print '// from file %s' % fpath
            print 'unsigned char %s[%d] = { ' % (arrname, len(stuff))
            print ','.join(map(lambda x: '0x%X' % ord(x), list(stuff)))
            print ' };\n'
        else:
            raise "ERROR: need argument \"header\" or \"source\""


