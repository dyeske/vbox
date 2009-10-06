/*
 * IDirect3DSwapChain8 implementation
 *
 * Copyright 2005 Oliver Stieber
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

/*
 * Sun LGPL Disclaimer: For the avoidance of doubt, except that if any license choice
 * other than GPL or LGPL is available it will apply instead, Sun elects to use only
 * the Lesser General Public License version 2.1 (LGPLv2) at this time for any software where
 * a choice of LGPL license versions is made available with the language indicating
 * that LGPLv2 or any later version may be used, or where a choice of which version
 * of the LGPL is applied is otherwise unspecified.
 */

#include "config.h"
#include "d3d8_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3d8);

/* IDirect3DSwapChain IUnknown parts follow: */
static HRESULT WINAPI IDirect3DSwapChain8Impl_QueryInterface(LPDIRECT3DSWAPCHAIN8 iface, REFIID riid, LPVOID* ppobj)
{
    IDirect3DSwapChain8Impl *This = (IDirect3DSwapChain8Impl *)iface;

    if (IsEqualGUID(riid, &IID_IUnknown)
        || IsEqualGUID(riid, &IID_IDirect3DSwapChain8)) {
        IUnknown_AddRef(iface);
        *ppobj = This;
        return S_OK;
    }

    WARN("(%p)->(%s,%p),not found\n", This, debugstr_guid(riid), ppobj);
    *ppobj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI IDirect3DSwapChain8Impl_AddRef(LPDIRECT3DSWAPCHAIN8 iface) {
    IDirect3DSwapChain8Impl *This = (IDirect3DSwapChain8Impl *)iface;
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) : AddRef from %d\n", This, ref - 1);

    return ref;
}

static ULONG WINAPI IDirect3DSwapChain8Impl_Release(LPDIRECT3DSWAPCHAIN8 iface) {
    IDirect3DSwapChain8Impl *This = (IDirect3DSwapChain8Impl *)iface;
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) : ReleaseRef to %d\n", This, ref);

    if (ref == 0) {
        wined3d_mutex_lock();
        IWineD3DSwapChain_Destroy(This->wineD3DSwapChain);
        wined3d_mutex_unlock();

        if (This->parentDevice) IUnknown_Release(This->parentDevice);
        HeapFree(GetProcessHeap(), 0, This);
    }
    return ref;
}

/* IDirect3DSwapChain8 parts follow: */
static HRESULT WINAPI IDirect3DSwapChain8Impl_Present(LPDIRECT3DSWAPCHAIN8 iface, CONST RECT *pSourceRect, CONST RECT *pDestRect, HWND hDestWindowOverride, CONST RGNDATA *pDirtyRegion) {
    IDirect3DSwapChain8Impl *This = (IDirect3DSwapChain8Impl *)iface;
    HRESULT hr;
    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    hr = IWineD3DSwapChain_Present(This->wineD3DSwapChain, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, 0);
    wined3d_mutex_unlock();

    return hr;
}

static HRESULT WINAPI IDirect3DSwapChain8Impl_GetBackBuffer(LPDIRECT3DSWAPCHAIN8 iface, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface8** ppBackBuffer) {
    IDirect3DSwapChain8Impl *This = (IDirect3DSwapChain8Impl *)iface;
    HRESULT hrc = D3D_OK;
    IWineD3DSurface *mySurface = NULL;

    TRACE("(%p) Relay\n", This);

    wined3d_mutex_lock();
    hrc = IWineD3DSwapChain_GetBackBuffer(This->wineD3DSwapChain, iBackBuffer, (WINED3DBACKBUFFER_TYPE )Type, &mySurface);
    if (hrc == D3D_OK && NULL != mySurface) {
       IWineD3DSurface_GetParent(mySurface, (IUnknown **)ppBackBuffer);
       IWineD3DSurface_Release(mySurface);
    }
    wined3d_mutex_unlock();

    return hrc;
}

const IDirect3DSwapChain8Vtbl Direct3DSwapChain8_Vtbl =
{
    IDirect3DSwapChain8Impl_QueryInterface,
    IDirect3DSwapChain8Impl_AddRef,
    IDirect3DSwapChain8Impl_Release,
    IDirect3DSwapChain8Impl_Present,
    IDirect3DSwapChain8Impl_GetBackBuffer
};
