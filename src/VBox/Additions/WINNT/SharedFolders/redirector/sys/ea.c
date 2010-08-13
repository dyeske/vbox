/*++

 Copyright (c) 1989 - 1999 Microsoft Corporation

 Module Name:

 ea.c

 Abstract:

 This module implements 'extended attributes' on a file handle

 --*/

#include "precomp.h"
#pragma hdrstop

//
//  The local debug trace level
//

#define Dbg                              (DEBUG_TRACE_EA)

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, VBoxMRxQueryEaInformation)
#endif

#ifdef VBOX
/*
 * We don't support extended attributes as the host OS might not know the concept at all
 *
 */
#endif

//
//  Extended Attributes (EA) functionality
//

NTSTATUS VBoxMRxQueryEaInformation (IN OUT PRX_CONTEXT RxContext)
/*++

 Routine Description:

 This routine queries IFS for extended attributes like
 scatter-gather list and filename for an IFS handle.

 Arguments:

 RxContext - the RDBSS context

 Return Value:

 RXSTATUS - The return status for the operation

 Notes:


 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;

    RxCaptureFcb;
    RxCaptureFobx;
    RxCaptureParamBlock;

    PMRX_SRV_OPEN pSrvOpen = capFobx->pSrvOpen;
    PFILE_FULL_EA_INFORMATION pEaInfo = (PFILE_FULL_EA_INFORMATION)RxContext->Info.Buffer;
    ULONG BufferLength = RxContext->Info.LengthRemaining;
    ULONG UserEaListLength = RxContext->QueryEa.UserEaListLength;
    PUCHAR UserEaList = RxContext->QueryEa.UserEaList;
    PFILE_GET_EA_INFORMATION pGetEaInfo = (PFILE_GET_EA_INFORMATION)UserEaList;
    PMRX_NET_ROOT pNetRoot = capFcb->pNetRoot;
    VBoxMRxGetNetRootExtension(pNetRoot,pNetRootExtension);

    PAGED_CODE();

    Log(("VBOXSF: VBoxMRxQueryEaInformation: Ea buffer len remaining is %d\n", RxContext->Info.LengthRemaining));

    return (Status);
}

NTSTATUS VBoxMRxSetEaInformation (IN OUT PRX_CONTEXT RxContext)
/*++

 Routine Description:

 This routine sets the EA information for this FCB

 Arguments:

 RxContext - the RDBSS context

 Return Value:

 RXSTATUS - The return status for the operation

 Notes:


 --*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(RxContext);

    Log(("VBOXSF: VBoxMRxSetEaInformation: Not implemented!\n"));
    return STATUS_NOT_IMPLEMENTED;
}

