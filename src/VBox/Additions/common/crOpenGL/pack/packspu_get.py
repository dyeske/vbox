# Copyright (c) 2001, Stanford University
# All rights reserved.
#
# See the file LICENSE.txt for information on redistributing this software.

from __future__ import print_function
import sys

import apiutil


apiutil.CopyrightC()

print("""
/* DO NOT EDIT - THIS FILE AUTOMATICALLY GENERATED BY packspu_get.py SCRIPT */
#include "packspu.h"
#include "cr_packfunctions.h"
#include "cr_net.h"
#include "cr_mem.h"
#include "packspu_proto.h"
""")

print("""
static GLboolean crPackIsPixelStoreParm(GLenum pname)
{
    if (pname == GL_UNPACK_ALIGNMENT
        || pname == GL_UNPACK_ROW_LENGTH
        || pname == GL_UNPACK_SKIP_PIXELS
        || pname == GL_UNPACK_LSB_FIRST
        || pname == GL_UNPACK_SWAP_BYTES
#ifdef CR_OPENGL_VERSION_1_2
        || pname == GL_UNPACK_IMAGE_HEIGHT
#endif
        || pname == GL_UNPACK_SKIP_ROWS
        || pname == GL_PACK_ALIGNMENT
        || pname == GL_PACK_ROW_LENGTH
        || pname == GL_PACK_SKIP_PIXELS
        || pname == GL_PACK_LSB_FIRST
        || pname == GL_PACK_SWAP_BYTES
#ifdef CR_OPENGL_VERSION_1_2
        || pname == GL_PACK_IMAGE_HEIGHT
#endif
        || pname == GL_PACK_SKIP_ROWS)
    {
        return GL_TRUE;
    }
    return GL_FALSE;
}
""")

print('#ifdef DEBUG');
from get_sizes import *
print('#endif');

simple_funcs = [ 'GetIntegerv', 'GetFloatv', 'GetDoublev', 'GetBooleanv' ]
vertattr_get_funcs = [ 'GetVertexAttribdv' 'GetVertexAttribfv' 'GetVertexAttribiv' ]

keys = apiutil.GetDispatchedFunctions(sys.argv[1]+"/APIspec.txt")

for func_name in keys:
    params = apiutil.Parameters(func_name)
    return_type = apiutil.ReturnType(func_name)
    if apiutil.FindSpecial( "packspu", func_name ):
        continue

    if "get" in apiutil.Properties(func_name):
        print('%s PACKSPU_APIENTRY packspu_%s(%s)' % ( return_type, func_name, apiutil.MakeDeclarationString( params ) ))
        print('{')
        print('\tGET_THREAD(thread);')
        print('\tint writeback = 1;')
        if return_type != 'void':
            print('\t%s return_val = (%s) 0;' % (return_type, return_type))
            params.append( ("&return_val", "foo", 0) )
        print('\tif (!CRPACKSPU_IS_WDDM_CRHGSMI() && !(pack_spu.thread[pack_spu.idxThreadInUse].netServer.conn->actual_network))')
        print('\t{')
        print('\t\tcrError( "packspu_%s doesn\'t work when there\'s no actual network involved!\\nTry using the simplequery SPU in your chain!" );' % func_name)
        print('\t}')
        if func_name in simple_funcs:
            print("""
    if (crPackIsPixelStoreParm(pname)
        || pname == GL_DRAW_BUFFER
#ifdef CR_OPENGL_VERSION_1_3
        || pname == GL_ACTIVE_TEXTURE
#endif
#ifdef CR_ARB_multitexture
        || pname == GL_ACTIVE_TEXTURE_ARB
#endif
        || pname == GL_TEXTURE_BINDING_1D
        || pname == GL_TEXTURE_BINDING_2D
#ifdef CR_NV_texture_rectangle
        || pname == GL_TEXTURE_BINDING_RECTANGLE_NV
#endif
#ifdef CR_ARB_texture_cube_map
        || pname == GL_TEXTURE_BINDING_CUBE_MAP_ARB
#endif
#ifdef CR_ARB_vertex_program
        || pname == GL_MAX_VERTEX_ATTRIBS_ARB
#endif
#ifdef GL_EXT_framebuffer_object
        || pname == GL_FRAMEBUFFER_BINDING_EXT
        || pname == GL_READ_FRAMEBUFFER_BINDING_EXT
        || pname == GL_DRAW_FRAMEBUFFER_BINDING_EXT
#endif
        || pname == GL_ARRAY_BUFFER_BINDING
        || pname == GL_ELEMENT_ARRAY_BUFFER_BINDING
        || pname == GL_PIXEL_PACK_BUFFER_BINDING
        || pname == GL_PIXEL_UNPACK_BUFFER_BINDING
        )
        {
#ifdef DEBUG
            if (!crPackIsPixelStoreParm(pname)
#ifdef CR_ARB_vertex_program
                && (pname!=GL_MAX_VERTEX_ATTRIBS_ARB)
#endif
               )
            {
                unsigned int i = 0;
                %s localparams;
                localparams = (%s) crAlloc(__numValues(pname) * sizeof(*localparams));
                crState%s(pname, localparams);
                crPack%s(%s, &writeback);
                packspuFlush( (void *) thread );
                CRPACKSPU_WRITEBACK_WAIT(thread, writeback);
                for (i=0; i<__numValues(pname); ++i)
                {
                    if (localparams[i] != params[i])
                    {
                        crWarning("Incorrect local state in %s for %%x param %%i", pname, i);
                        crWarning("Expected %%i but got %%i", (int)localparams[i], (int)params[i]);
                    }
                }
                crFree(localparams);
                return;
            }
            else
#endif
            {
                crState%s(pname, params);
                return;
            }

        }
            """ % (params[-1][1], params[-1][1], func_name, func_name, apiutil.MakeCallString(params), func_name, func_name))

        if func_name in vertattr_get_funcs:
            print("""
    if (pname != GL_CURRENT_VERTEX_ATTRIB_ARB)
    {
#ifdef DEBUG
        %s localparams;
        localparams = (%s) crAlloc(__numValues(pname) * sizeof(*localparams));
        crState%s(index, pname, localparams);
        crPack%s(index, %s, &writeback);
        packspuFlush( (void *) thread );
        CRPACKSPU_WRITEBACK_WAIT(thread, writeback);
        for (i=0; i<crStateHlpComponentsCount(pname); ++i)
        {
            if (localparams[i] != params[i])
            {
                crWarning("Incorrect local state in %s for %%x param %%i", pname, i);
                crWarning("Expected %%i but got %%i", (int)localparams[i], (int)params[i]);
            }
        }
        crFree(localparams);
#else
        crState%s(pname, params);
#endif
        return;
    }
            """ % (params[-1][1], params[-1][1], func_name, func_name, apiutil.MakeCallString(params), func_name, func_name))

        params.append( ("&writeback", "foo", 0) )
        print('\tcrPack%s(%s);' % (func_name, apiutil.MakeCallString( params ) ))
        print('\tpackspuFlush( (void *) thread );')
        print('\tCRPACKSPU_WRITEBACK_WAIT(thread, writeback);')



        lastParamName = params[-2][0]
        if return_type != 'void':
            print('\treturn return_val;')
        print('}\n')
