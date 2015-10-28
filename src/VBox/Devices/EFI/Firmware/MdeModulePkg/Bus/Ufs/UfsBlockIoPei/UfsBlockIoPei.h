/** @file

  Copyright (c) 2014, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _UFS_BLOCK_IO_PEI_H_
#define _UFS_BLOCK_IO_PEI_H_

#include <PiPei.h>

#include <Ppi/UfsHostController.h>
#include <Ppi/BlockIo.h>

#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/PeiServicesLib.h>

#include <IndustryStandard/Scsi.h>

#include "UfsHci.h"
#include "UfsHcMem.h"

#define UFS_PEIM_HC_SIG             SIGNATURE_32 ('U', 'F', 'S', 'H')

#define UFS_PEIM_MAX_LUNS           8

typedef struct {
  UINT8    Lun[UFS_PEIM_MAX_LUNS];
  UINT16   BitMask:12;              // Bit 0~7 is for common luns. Bit 8~11 is reserved for those well known luns
  UINT16   Rsvd:4;
} UFS_PEIM_EXPOSED_LUNS;

typedef struct {
  ///
  /// The timeout, in 100 ns units, to use for the execution of this SCSI
  /// Request Packet. A Timeout value of 0 means that this function
  /// will wait indefinitely for the SCSI Request Packet to execute. If
  /// Timeout is greater than zero, then this function will return
  /// EFI_TIMEOUT if the time required to execute the SCSI
  /// Request Packet is greater than Timeout.
  ///
  UINT64 Timeout;
  ///
  /// A pointer to the data buffer to transfer between the SCSI
  /// controller and the SCSI device for read and bidirectional commands.
  ///
  VOID   *InDataBuffer;
  ///
  /// A pointer to the data buffer to transfer between the SCSI
  /// controller and the SCSI device for write or bidirectional commands.
  ///
  VOID   *OutDataBuffer;
  ///
  /// A pointer to the sense data that was generated by the execution of
  /// the SCSI Request Packet.
  ///
  VOID   *SenseData;
  ///
  /// A pointer to buffer that contains the Command Data Block to
  /// send to the SCSI device specified by Target and Lun.
  ///
  VOID   *Cdb;
  ///
  /// On Input, the size, in bytes, of InDataBuffer. On output, the
  /// number of bytes transferred between the SCSI controller and the SCSI device.
  ///
  UINT32 InTransferLength;
  ///
  /// On Input, the size, in bytes of OutDataBuffer. On Output, the
  /// Number of bytes transferred between SCSI Controller and the SCSI device.
  ///
  UINT32 OutTransferLength;
  ///
  /// The length, in bytes, of the buffer Cdb. The standard values are 6,
  /// 10, 12, and 16, but other values are possible if a variable length CDB is used.
  ///
  UINT8  CdbLength;
  ///
  /// The direction of the data transfer. 0 for reads, 1 for writes. A
  /// value of 2 is Reserved for Bi-Directional SCSI commands.
  ///
  UINT8  DataDirection;
  ///
  /// On input, the length in bytes of the SenseData buffer. On
  /// output, the number of bytes written to the SenseData buffer.
  ///
  UINT8  SenseDataLength;
} UFS_SCSI_REQUEST_PACKET;

typedef struct _UFS_PEIM_HC_PRIVATE_DATA {  
  UINT32                            Signature;
  EFI_HANDLE                        Controller;

  UFS_PEIM_MEM_POOL                 *Pool;

  EFI_PEI_RECOVERY_BLOCK_IO_PPI     BlkIoPpi;
  EFI_PEI_PPI_DESCRIPTOR            BlkIoPpiList;
  EFI_PEI_BLOCK_IO_MEDIA            Media[UFS_PEIM_MAX_LUNS];

  UINTN                             UfsHcBase;
  UINT32                            Capabilities;

  UINT8                             TaskTag;

  VOID                              *UtpTrlBase;
  UINT8                             Nutrs;
  VOID                              *UtpTmrlBase;
  UINT8                             Nutmrs;

  UFS_PEIM_EXPOSED_LUNS             Luns;
} UFS_PEIM_HC_PRIVATE_DATA;

#define UFS_TIMEOUT                 MultU64x32((UINT64)(3), 10000000)

#define ROUNDUP8(x) (((x) % 8 == 0) ? (x) : ((x) / 8 + 1) * 8)

#define IS_ALIGNED(addr, size)      (((UINTN) (addr) & (size - 1)) == 0)

#define GET_UFS_PEIM_HC_PRIVATE_DATA_FROM_THIS(a) CR (a, UFS_PEIM_HC_PRIVATE_DATA, BlkIoPpi, UFS_PEIM_HC_SIG)

#define UFS_SCSI_OP_LENGTH_SIX      0x6
#define UFS_SCSI_OP_LENGTH_TEN      0xa
#define UFS_SCSI_OP_LENGTH_SIXTEEN  0x10

typedef struct _UFS_DEVICE_MANAGEMENT_REQUEST_PACKET {
  UINT64                            Timeout;
  VOID                              *InDataBuffer;
  VOID                              *OutDataBuffer;
  UINT8                             Opcode;
  UINT8                             DescId;
  UINT8                             Index;
  UINT8                             Selector;
  UINT32                            InTransferLength;
  UINT32                            OutTransferLength;
  UINT8                             DataDirection;
  UINT8                             Ocs;
} UFS_DEVICE_MANAGEMENT_REQUEST_PACKET;

/**
  Sends a UFS-supported SCSI Request Packet to a UFS device that is attached to the UFS host controller.

  @param[in]      Private       The pointer to the UFS_PEIM_HC_PRIVATE_DATA data structure.
  @param[in]      Lun           The LUN of the UFS device to send the SCSI Request Packet.
  @param[in, out] Packet        A pointer to the SCSI Request Packet to send to a specified Lun of the
                                UFS device.

  @retval EFI_SUCCESS           The SCSI Request Packet was sent by the host. For bi-directional
                                commands, InTransferLength bytes were transferred from
                                InDataBuffer. For write and bi-directional commands,
                                OutTransferLength bytes were transferred by
                                OutDataBuffer.
  @retval EFI_DEVICE_ERROR      A device error occurred while attempting to send the SCSI Request
                                Packet.
  @retval EFI_OUT_OF_RESOURCES  The resource for transfer is not available.
  @retval EFI_TIMEOUT           A timeout occurred while waiting for the SCSI Request Packet to execute.

**/
EFI_STATUS
UfsExecScsiCmds (
  IN     UFS_PEIM_HC_PRIVATE_DATA      *Private,
  IN     UINT8                         Lun,
  IN OUT UFS_SCSI_REQUEST_PACKET       *Packet
  );

/**
  Initialize the UFS host controller.

  @param[in] Private                 The pointer to the UFS_PEIM_HC_PRIVATE_DATA data structure.

  @retval EFI_SUCCESS                The Ufs Host Controller is initialized successfully.
  @retval Others                     A device error occurred while initializing the controller.

**/
EFI_STATUS
UfsControllerInit (
  IN  UFS_PEIM_HC_PRIVATE_DATA       *Private
  );

/**
  Stop the UFS host controller.

  @param[in] Private                 The pointer to the UFS_PEIM_HC_PRIVATE_DATA data structure.

  @retval EFI_SUCCESS                The Ufs Host Controller is stopped successfully.
  @retval Others                     A device error occurred while stopping the controller.

**/
EFI_STATUS
UfsControllerStop (
  IN  UFS_PEIM_HC_PRIVATE_DATA       *Private
  );

/**
  Set specified flag to 1 on a UFS device.

  @param[in]  Private           The pointer to the UFS_PEIM_HC_PRIVATE_DATA data structure.
  @param[in]  FlagId            The ID of flag to be set.

  @retval EFI_SUCCESS           The flag was set successfully.
  @retval EFI_DEVICE_ERROR      A device error occurred while attempting to set the flag.
  @retval EFI_TIMEOUT           A timeout occurred while waiting for the completion of setting the flag.

**/
EFI_STATUS
UfsSetFlag (
  IN  UFS_PEIM_HC_PRIVATE_DATA     *Private,
  IN  UINT8                        FlagId
  );

/**
  Read or write specified device descriptor of a UFS device.

  @param[in]      Private       The pointer to the UFS_PEIM_HC_PRIVATE_DATA data structure.
  @param[in]      Read          The boolean variable to show r/w direction.
  @param[in]      DescId        The ID of device descriptor.
  @param[in]      Index         The Index of device descriptor.
  @param[in]      Selector      The Selector of device descriptor.
  @param[in, out] Descriptor    The buffer of device descriptor to be read or written.
  @param[in]      DescSize      The size of device descriptor buffer.

  @retval EFI_SUCCESS           The device descriptor was read/written successfully.
  @retval EFI_DEVICE_ERROR      A device error occurred while attempting to r/w the device descriptor.
  @retval EFI_TIMEOUT           A timeout occurred while waiting for the completion of r/w the device descriptor.

**/
EFI_STATUS
UfsRwDeviceDesc (
  IN     UFS_PEIM_HC_PRIVATE_DATA     *Private,
  IN     BOOLEAN                      Read,
  IN     UINT8                        DescId,
  IN     UINT8                        Index,
  IN     UINT8                        Selector,
  IN OUT VOID                         *Descriptor,
  IN     UINT32                       DescSize
  );

/**
  Sends NOP IN cmd to a UFS device for initialization process request.
  For more details, please refer to UFS 2.0 spec Figure 13.3.

  @param[in]  Private           The pointer to the UFS_PEIM_HC_PRIVATE_DATA data structure.

  @retval EFI_SUCCESS           The NOP IN command was sent by the host. The NOP OUT response was
                                received successfully.
  @retval EFI_DEVICE_ERROR      A device error occurred while attempting to execute NOP IN command.
  @retval EFI_OUT_OF_RESOURCES  The resource for transfer is not available.
  @retval EFI_TIMEOUT           A timeout occurred while waiting for the NOP IN command to execute.

**/
EFI_STATUS
UfsExecNopCmds (
  IN  UFS_PEIM_HC_PRIVATE_DATA       *Private
  );

/**
  Gets the count of block I/O devices that one specific block driver detects.

  This function is used for getting the count of block I/O devices that one 
  specific block driver detects.  To the PEI ATAPI driver, it returns the number
  of all the detected ATAPI devices it detects during the enumeration process. 
  To the PEI legacy floppy driver, it returns the number of all the legacy 
  devices it finds during its enumeration process. If no device is detected, 
  then the function will return zero.  
  
  @param[in]  PeiServices          General-purpose services that are available 
                                   to every PEIM.
  @param[in]  This                 Indicates the EFI_PEI_RECOVERY_BLOCK_IO_PPI 
                                   instance.
  @param[out] NumberBlockDevices   The number of block I/O devices discovered.

  @retval     EFI_SUCCESS          The operation performed successfully.

**/
EFI_STATUS
EFIAPI
UfsBlockIoPeimGetDeviceNo (
  IN  EFI_PEI_SERVICES               **PeiServices,
  IN  EFI_PEI_RECOVERY_BLOCK_IO_PPI  *This,
  OUT UINTN                          *NumberBlockDevices
  );

/**
  Gets a block device's media information.

  This function will provide the caller with the specified block device's media 
  information. If the media changes, calling this function will update the media 
  information accordingly.

  @param[in]  PeiServices   General-purpose services that are available to every
                            PEIM
  @param[in]  This          Indicates the EFI_PEI_RECOVERY_BLOCK_IO_PPI instance.
  @param[in]  DeviceIndex   Specifies the block device to which the function wants 
                            to talk. Because the driver that implements Block I/O 
                            PPIs will manage multiple block devices, the PPIs that 
                            want to talk to a single device must specify the 
                            device index that was assigned during the enumeration
                            process. This index is a number from one to 
                            NumberBlockDevices.
  @param[out] MediaInfo     The media information of the specified block media.  
                            The caller is responsible for the ownership of this 
                            data structure.

  @par Note: 
      The MediaInfo structure describes an enumeration of possible block device 
      types.  This enumeration exists because no device paths are actually passed 
      across interfaces that describe the type or class of hardware that is publishing 
      the block I/O interface. This enumeration will allow for policy decisions
      in the Recovery PEIM, such as "Try to recover from legacy floppy first, 
      LS-120 second, CD-ROM third." If there are multiple partitions abstracted 
      by a given device type, they should be reported in ascending order; this 
      order also applies to nested partitions, such as legacy MBR, where the 
      outermost partitions would have precedence in the reporting order. The 
      same logic applies to systems such as IDE that have precedence relationships 
      like "Master/Slave" or "Primary/Secondary". The master device should be 
      reported first, the slave second.
  
  @retval EFI_SUCCESS        Media information about the specified block device 
                             was obtained successfully.
  @retval EFI_DEVICE_ERROR   Cannot get the media information due to a hardware 
                             error.

**/
EFI_STATUS
EFIAPI
UfsBlockIoPeimGetMediaInfo (
  IN  EFI_PEI_SERVICES               **PeiServices,
  IN  EFI_PEI_RECOVERY_BLOCK_IO_PPI  *This,
  IN  UINTN                          DeviceIndex,
  OUT EFI_PEI_BLOCK_IO_MEDIA         *MediaInfo
  );

/**
  Reads the requested number of blocks from the specified block device.

  The function reads the requested number of blocks from the device. All the 
  blocks are read, or an error is returned. If there is no media in the device,
  the function returns EFI_NO_MEDIA.

  @param[in]  PeiServices   General-purpose services that are available to 
                            every PEIM.
  @param[in]  This          Indicates the EFI_PEI_RECOVERY_BLOCK_IO_PPI instance.
  @param[in]  DeviceIndex   Specifies the block device to which the function wants 
                            to talk. Because the driver that implements Block I/O 
                            PPIs will manage multiple block devices, PPIs that 
                            want to talk to a single device must specify the device 
                            index that was assigned during the enumeration process. 
                            This index is a number from one to NumberBlockDevices.
  @param[in]  StartLBA      The starting logical block address (LBA) to read from
                            on the device
  @param[in]  BufferSize    The size of the Buffer in bytes. This number must be
                            a multiple of the intrinsic block size of the device.
  @param[out] Buffer        A pointer to the destination buffer for the data.
                            The caller is responsible for the ownership of the 
                            buffer.
                         
  @retval EFI_SUCCESS             The data was read correctly from the device.
  @retval EFI_DEVICE_ERROR        The device reported an error while attempting 
                                  to perform the read operation.
  @retval EFI_INVALID_PARAMETER   The read request contains LBAs that are not 
                                  valid, or the buffer is not properly aligned.
  @retval EFI_NO_MEDIA            There is no media in the device.
  @retval EFI_BAD_BUFFER_SIZE     The BufferSize parameter is not a multiple of
                                  the intrinsic block size of the device.

**/
EFI_STATUS
EFIAPI
UfsBlockIoPeimReadBlocks (
  IN  EFI_PEI_SERVICES               **PeiServices,
  IN  EFI_PEI_RECOVERY_BLOCK_IO_PPI  *This,
  IN  UINTN                          DeviceIndex,
  IN  EFI_PEI_LBA                    StartLBA,
  IN  UINTN                          BufferSize,
  OUT VOID                           *Buffer
  );

/**
  Initialize the memory management pool for the host controller.
  
  @param  Private               The Ufs Peim driver private data.

  @retval EFI_SUCCESS           The memory pool is initialized.
  @retval Others                Fail to init the memory pool.

**/
EFI_STATUS
UfsPeimInitMemPool (
  IN  UFS_PEIM_HC_PRIVATE_DATA      *Private
  );

/**
  Allocate some memory from the host controller's memory pool
  which can be used to communicate with host controller.
  
  @param  Pool      The host controller's memory pool.
  @param  Size      Size of the memory to allocate.

  @return The allocated memory or NULL.

**/
VOID *
UfsPeimAllocateMem (
  IN  UFS_PEIM_MEM_POOL        *Pool,
  IN  UINTN                    Size
  );

/**
  Free the allocated memory back to the memory pool.

  @param  Pool           The memory pool of the host controller.
  @param  Mem            The memory to free.
  @param  Size           The size of the memory to free.

**/
VOID
UfsPeimFreeMem (
  IN UFS_PEIM_MEM_POOL    *Pool,
  IN VOID                 *Mem,
  IN UINTN                Size
  );

#endif
