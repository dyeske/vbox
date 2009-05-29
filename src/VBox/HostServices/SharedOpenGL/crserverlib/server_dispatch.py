# Copyright (c) 2001, Stanford University
# All rights reserved.
#
# See the file LICENSE.txt for information on redistributing this software.

import sys, string, re

import apiutil



apiutil.CopyrightC()

print """
/* DO NOT EDIT - THIS FILE AUTOMATICALLY GENERATED BY server_dispatch.py SCRIPT */
#include "cr_spu.h"
#include "chromium.h"
#include "cr_error.h"
#include "server_dispatch.h"
#include "server.h"
#include "cr_unpack.h"

CRCurrentStatePointers crServerCurrent;
"""


for func_name in apiutil.AllSpecials( sys.argv[1]+"/../state_tracker/state" ):
    params = apiutil.Parameters(func_name)
    if (apiutil.FindSpecial( "server", func_name ) or
        "get" in apiutil.Properties(func_name)):
        continue

    wrap = apiutil.GetCategoryWrapper(func_name)
    if wrap:
        print '#if defined(CR_%s)' % wrap
    print 'void SERVER_DISPATCH_APIENTRY crServerDispatch%s( %s )' % ( func_name, apiutil.MakeDeclarationString( params ) )
    print '{'
    print '\tcrState%s( %s );' % (func_name, apiutil.MakeCallString( params ) )
    print '\tcr_server.head_spu->dispatch_table.%s( %s );' % (func_name, apiutil.MakeCallString( params ) )
    print '}'
    if wrap:
        print '#endif'


keys = apiutil.GetDispatchedFunctions(sys.argv[1]+"/APIspec.txt")
for func_name in keys:
    current = 0
    array = ""
    m = re.search( r"^(Color|Normal)([1234])(ub|b|us|s|ui|i|f|d)$", func_name )
    if m :
        current = 1
        name = string.lower( m.group(1)[:1] ) + m.group(1)[1:]
        type = m.group(3) + m.group(2)
    m = re.search( r"^(SecondaryColor)(3)(ub|b|us|s|ui|i|f|d)(EXT)$", func_name )
    if m :
        current = 1
        name = string.lower(m.group(1)[:1] ) + m.group(1)[1:]
        type = m.group(3) + m.group(2)
    m = re.search( r"^(TexCoord)([1234])(ub|b|us|s|ui|i|f|d)$", func_name )
    if m :
        current = 1
        name = string.lower( m.group(1)[:1] ) + m.group(1)[1:]
        type = m.group(3) + m.group(2)
        array = "[0]"
    m = re.search( r"^(MultiTexCoord)([1234])(ub|b|us|s|ui|i|f|d)ARB$", func_name )
    if m :
        current = 1
        name = "texCoord"
        type = m.group(3) + m.group(2)
        array = "[texture-GL_TEXTURE0_ARB]"
    m = re.match( r"^(Index)(ub|b|us|s|ui|i|f|d)$", func_name )
    if m :
        current = 1
        name = string.lower( m.group(1)[:1] ) + m.group(1)[1:]
        type = m.group(2) + "1"
    m = re.match( r"^(EdgeFlag)$", func_name )
    if m :
        current = 1
        name = string.lower( m.group(1)[:1] ) + m.group(1)[1:]
        type = "l1"
    m = re.match( r"^(FogCoord)(f|d)(EXT)$", func_name)
    if m :
        current = 1
        name = string.lower( m.group(1)[:1] ) + m.group(1)[1:]
        type = m.group(2) + "1"
        
    # Vertex attribute commands w/ some special cases
    m = re.search( r"^(VertexAttrib)([1234])(s|i|f|d)ARB$", func_name )
    if m :
        current = 1
        name = string.lower( m.group(1)[:1] ) + m.group(1)[1:]
        type = m.group(3) + m.group(2)
        array = "[index]"
    if func_name == "VertexAttrib4NubARB":
        current = 1
        name = "vertexAttrib"
        type = "ub4"
        array = "[index]"

    if current:
        params = apiutil.Parameters(func_name)
        print 'void SERVER_DISPATCH_APIENTRY crServerDispatch%s( %s )' % ( func_name, apiutil.MakeDeclarationString(params) )
        print '{'
        print '\tcr_server.head_spu->dispatch_table.%s( %s );' % (func_name, apiutil.MakeCallString(params) )
        print "\tcr_server.current.c.%s.%s%s = cr_unpackData;" % (name,type,array)
        print '}\n' 

print """
void crServerInitDispatch(void)
{
    crSPUInitDispatchTable( &(cr_server.dispatch) );
    crSPUCopyDispatchTable( &(cr_server.dispatch), &(cr_server.head_spu->dispatch_table ) );
"""

for func_name in keys:
    if ("get" in apiutil.Properties(func_name) or
        apiutil.FindSpecial( "server", func_name ) or
        apiutil.FindSpecial( sys.argv[1]+"/../state_tracker/state", func_name )):

        wrap = apiutil.GetCategoryWrapper(func_name)
        if wrap:
            print '#if defined(CR_%s)' % wrap
            
        print '\tcr_server.dispatch.%s = crServerDispatch%s;' % (func_name, func_name)
        if wrap:
            print '#endif'

print '}'

