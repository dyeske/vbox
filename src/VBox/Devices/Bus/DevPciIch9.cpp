/* $Id$ */
/** @file
 * DevPCI - ICH9 southbridge PCI bus emulation Device.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
 *   Header Files                                                              *
 *******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_PCI
/* Hack to get PCIDEVICEINT declare at the right point - include "PCIInternal.h". */
#define PCI_INCLUDE_PRIVATE
#include <VBox/pci.h>
#include <VBox/msi.h>
#include <VBox/pdmdev.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#ifdef IN_RING3
#include <iprt/alloc.h>
#endif

#include "../Builtins.h"

#include "MsiCommon.h"

/**
 * PCI Bus instance.
 */
typedef struct PCIBus
{
    /** Bus number. */
    int32_t             iBus;
    /** Number of bridges attached to the bus. */
    uint32_t            cBridges;

    /** Array of PCI devices. We assume 32 slots, each with 8 functions. */
    R3PTRTYPE(PPCIDEVICE)   apDevices[256];
    /** Array of bridges attached to the bus. */
    R3PTRTYPE(PPCIDEVICE *) papBridgesR3;

    /** R3 pointer to the device instance. */
    PPDMDEVINSR3        pDevInsR3;
    /** Pointer to the PCI R3  helpers. */
    PCPDMPCIHLPR3       pPciHlpR3;

    /** R0 pointer to the device instance. */
    PPDMDEVINSR0        pDevInsR0;
    /** Pointer to the PCI R0 helpers. */
    PCPDMPCIHLPR0       pPciHlpR0;

    /** RC pointer to the device instance. */
    PPDMDEVINSRC        pDevInsRC;
    /** Pointer to the PCI RC helpers. */
    PCPDMPCIHLPRC       pPciHlpRC;

    /** The PCI device for the PCI bridge. */
    PCIDEVICE           aPciDev;

} PCIBUS, *PPCIBUS;


/** @def PCI_APIC_IRQ_PINS
 * Number of pins for interrupts if the APIC is used.
 */
#define PCI_APIC_IRQ_PINS 8

/**
 * PCI Globals - This is the host-to-pci bridge and the root bus.
 */
typedef struct
{
    /** R3 pointer to the device instance. */
    PPDMDEVINSR3        pDevInsR3;
    /** R0 pointer to the device instance. */
    PPDMDEVINSR0        pDevInsR0;
    /** RC pointer to the device instance. */
    PPDMDEVINSRC        pDevInsRC;

#if HC_ARCH_BITS == 64
    uint32_t            Alignment0;
#endif

    /** PCI bus which is attached to the host-to-PCI bridge. */
    PCIBUS              aPciBus;


    /** I/O APIC irq levels */
    volatile uint32_t   uaPciApicIrqLevels[PCI_APIC_IRQ_PINS];

#if 1 /* Will be moved into the BIOS soon. */
    /** The next I/O port address which the PCI BIOS will use. */
    uint32_t            uPciBiosIo;
    /** The next MMIO address which the PCI BIOS will use. */
    uint32_t            uPciBiosMmio;
    /** Actual bus number. */
    uint8_t             uBus;
#endif
    /* Physical address of PCI config space MMIO region */
    uint64_t            u64PciConfigMMioAddress;
    /* Length of PCI config space MMIO region */
    uint64_t            u64PciConfigMMioLength;


    /** Config register. */
    uint32_t            uConfigReg;
} PCIGLOBALS, *PPCIGLOBALS;


typedef struct {
    uint8_t  iBus;
    uint8_t  iDeviceFunc;
    uint16_t iRegister;
} PciAddress;

/*******************************************************************************
 *   Defined Constants And Macros                                              *
 *******************************************************************************/

/** @def VBOX_ICH9PCI_SAVED_STATE_VERSION
 * Saved state version of the ICH9 PCI bus device.
 */
#define VBOX_ICH9PCI_SAVED_STATE_VERSION_NOMSI 1
#define VBOX_ICH9PCI_SAVED_STATE_VERSION_MSI   2
#define VBOX_ICH9PCI_SAVED_STATE_VERSION_CURRENT VBOX_ICH9PCI_SAVED_STATE_VERSION_MSI

/** Converts a bus instance pointer to a device instance pointer. */
#define PCIBUS_2_DEVINS(pPciBus)        ((pPciBus)->CTX_SUFF(pDevIns))
/** Converts a device instance pointer to a PCIGLOBALS pointer. */
#define DEVINS_2_PCIGLOBALS(pDevIns)    ((PPCIGLOBALS)(PDMINS_2_DATA(pDevIns, PPCIGLOBALS)))
/** Converts a device instance pointer to a PCIBUS pointer. */
#define DEVINS_2_PCIBUS(pDevIns)        ((PPCIBUS)(&PDMINS_2_DATA(pDevIns, PPCIGLOBALS)->aPciBus))
/** Converts a pointer to a PCI root bus instance to a PCIGLOBALS pointer.
 */
#define PCIROOTBUS_2_PCIGLOBALS(pPciBus)    ( (PPCIGLOBALS)((uintptr_t)(pPciBus) - RT_OFFSETOF(PCIGLOBALS, aPciBus)) )


/** @def PCI_LOCK
 * Acquires the PDM lock. This is a NOP if locking is disabled. */
/** @def PCI_UNLOCK
 * Releases the PDM lock. This is a NOP if locking is disabled. */
#define PCI_LOCK(pDevIns, rc) \
    do { \
        int rc2 = DEVINS_2_PCIBUS(pDevIns)->CTX_SUFF(pPciHlp)->pfnLock((pDevIns), rc); \
        if (rc2 != VINF_SUCCESS) \
            return rc2; \
    } while (0)
#define PCI_UNLOCK(pDevIns) \
    DEVINS_2_PCIBUS(pDevIns)->CTX_SUFF(pPciHlp)->pfnUnlock(pDevIns)

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

RT_C_DECLS_BEGIN

PDMBOTHCBDECL(void) ich9pciSetIrq(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel);
PDMBOTHCBDECL(void) ich9pcibridgeSetIrq(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel);
PDMBOTHCBDECL(int)  ich9pciIOPortAddressWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int)  ich9pciIOPortAddressRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int)  ich9pciIOPortDataWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int)  ich9pciIOPortDataRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int)  ich9pciMcfgMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
PDMBOTHCBDECL(int)  ich9pciMcfgMMIORead (PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb);

RT_C_DECLS_END

/* Prototypes */
static void ich9pciSetIrqInternal(PPCIGLOBALS pGlobals, uint8_t uDevFn, PPCIDEVICE pPciDev, int iIrq, int iLevel);
#ifdef IN_RING3
static int ich9pciRegisterInternal(PPCIBUS pBus, int iDev, PPCIDEVICE pPciDev, const char *pszName);
static void ich9pciUpdateMappings(PCIDevice *pDev);
static DECLCALLBACK(uint32_t) ich9pciConfigReadDev(PCIDevice *aDev, uint32_t u32Address, unsigned len);
DECLINLINE(PPCIDEVICE) ich9pciFindBridge(PPCIBUS pBus, uint8_t iBus);
static void ich9pciBiosInitDevice(PPCIGLOBALS pGlobals, uint8_t uBus, uint8_t uDevFn, uint8_t cBridgeDepth, uint8_t *paBridgePositions);
#endif

// See 7.2.2. PCI Express Enhanced Configuration Mechanism for details of address
// mapping, we take n=6 approach
DECLINLINE(void) ich9pciPhysToPciAddr(PPCIGLOBALS pGlobals, RTGCPHYS GCPhysAddr, PciAddress* pPciAddr)
{
    pPciAddr->iBus          = (GCPhysAddr >> 20) & ((1<<6)       - 1);
    pPciAddr->iDeviceFunc   = (GCPhysAddr >> 12) & ((1<<(5+3))   - 1); // 5 bits - device, 3 bits - function
    pPciAddr->iRegister     = (GCPhysAddr >>  0) & ((1<<(6+4+2)) - 1); // 6 bits - register, 4 bits - extended register, 2 bits -Byte Enable
}

DECLINLINE(void) ich9pciStateToPciAddr(PPCIGLOBALS pGlobals, RTGCPHYS addr, PciAddress* pPciAddr)
{
    pPciAddr->iBus         = (pGlobals->uConfigReg >> 16) & 0xff;
    pPciAddr->iDeviceFunc  = (pGlobals->uConfigReg >> 8) & 0xff;
    pPciAddr->iRegister    = (pGlobals->uConfigReg & 0xfc) | (addr & 3);
}

PDMBOTHCBDECL(void) ich9pciSetIrq(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel)
{
    ich9pciSetIrqInternal(PDMINS_2_DATA(pDevIns, PPCIGLOBALS), pPciDev->devfn, pPciDev, iIrq, iLevel);
}

PDMBOTHCBDECL(void) ich9pcibridgeSetIrq(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel)
{
    /*
     * The PCI-to-PCI bridge specification defines how the interrupt pins
     * are routed from the secondary to the primary bus (see chapter 9).
     * iIrq gives the interrupt pin the pci device asserted.
     * We change iIrq here according to the spec and call the SetIrq function
     * of our parent passing the device which asserted the interrupt instead of the device of the bridge.
     */
    PPCIBUS    pBus          = PDMINS_2_DATA(pDevIns, PPCIBUS);
    PPCIDEVICE pPciDevBus    = pPciDev;
    int        iIrqPinBridge = iIrq;
    uint8_t    uDevFnBridge  = 0;

    /* Walk the chain until we reach the host bus. */
    do
    {
        uDevFnBridge  = pBus->aPciDev.devfn;
        iIrqPinBridge = ((pPciDevBus->devfn >> 3) + iIrqPinBridge) & 3;

        /* Get the parent. */
        pBus = pBus->aPciDev.Int.s.CTX_SUFF(pBus);
        pPciDevBus = &pBus->aPciDev;
    } while (pBus->iBus != 0);

    AssertMsgReturnVoid(pBus->iBus == 0, ("This is not the host pci bus iBus=%d\n", pBus->iBus));
    ich9pciSetIrqInternal(PCIROOTBUS_2_PCIGLOBALS(pBus), uDevFnBridge, pPciDev, iIrqPinBridge, iLevel);
}

/**
 * Port I/O Handler for PCI address OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the OUT operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int)  ich9pciIOPortAddressWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    Log(("ich9pciIOPortAddressWrite: Port=%#x u32=%#x cb=%d\n", Port, u32, cb));
    NOREF(pvUser);
    if (cb == 4)
    {
        PPCIGLOBALS pThis = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);

        PCI_LOCK(pDevIns, VINF_IOM_HC_IOPORT_WRITE);
        pThis->uConfigReg = u32 & ~3; /* Bits 0-1 are reserved and we silently clear them */
        PCI_UNLOCK(pDevIns);
    }

    return VINF_SUCCESS;
}

/**
 * Port I/O Handler for PCI address IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
PDMBOTHCBDECL(int)  ich9pciIOPortAddressRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    NOREF(pvUser);
    if (cb == 4)
    {
        PPCIGLOBALS pThis = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
        PCI_LOCK(pDevIns, VINF_IOM_HC_IOPORT_READ);
        *pu32 = pThis->uConfigReg;
        PCI_UNLOCK(pDevIns);
        Log(("pciIOPortAddressRead: Port=%#x cb=%d -> %#x\n", Port, cb, *pu32));
        return VINF_SUCCESS;
    }

    Log(("ich9pciIOPortAddressRead: Port=%#x cb=%d VERR_IOM_IOPORT_UNUSED\n", Port, cb));

    return VERR_IOM_IOPORT_UNUSED;
}

static int ich9pciDataWriteAddr(PPCIGLOBALS pGlobals, PciAddress* pAddr,
                                uint32_t val, int cb, int rcReschedule)
{
    int rc = VINF_SUCCESS;

    if (pAddr->iRegister > 0xff)
    {
        LogRel(("PCI: attempt to write extended register: %x (%d) <- val\n", pAddr->iRegister, cb, val));
        goto out;
    }

    if (pAddr->iBus != 0)
    {
        if (pGlobals->aPciBus.cBridges)
        {
#ifdef IN_RING3 /** @todo do lookup in R0/RC too! */
            PPCIDEVICE pBridgeDevice = ich9pciFindBridge(&pGlobals->aPciBus, pAddr->iBus);
            if (pBridgeDevice)
            {
                AssertPtr(pBridgeDevice->Int.s.pfnBridgeConfigWrite);
                pBridgeDevice->Int.s.pfnBridgeConfigWrite(pBridgeDevice->pDevIns, pAddr->iBus, pAddr->iDeviceFunc, pAddr->iRegister, val, cb);
            }
            else
            {
                // do nothing, bridge not found
            }
#else
            rc = rcReschedule;
            goto out;
#endif
        }
    }
    else
    {
        if (pGlobals->aPciBus.apDevices[pAddr->iDeviceFunc])
        {
#ifdef IN_RING3
            R3PTRTYPE(PCIDevice *) aDev = pGlobals->aPciBus.apDevices[pAddr->iDeviceFunc];
            Log(("ich9pciConfigWrite: %s: addr=%02x val=%08x len=%d\n", aDev->name, pAddr->iRegister, val, cb));
            aDev->Int.s.pfnConfigWrite(aDev, pAddr->iRegister, val, cb);
#else
            rc = rcReschedule;
            goto out;
#endif
        }
    }

  out:
    Log2(("ich9pciDataWriteAddr: %02x:%02x:%02x reg %x(%d) %x %Rrc\n",
          pAddr->iBus, pAddr->iDeviceFunc >> 3, pAddr->iDeviceFunc & 0x7, pAddr->iRegister,
          cb, val, rc));

    return rc;
}

static int ich9pciDataWrite(PPCIGLOBALS pGlobals, uint32_t addr, uint32_t val, int len)
{
    PciAddress aPciAddr;

    LogFlow(("ich9pciDataWrite: config=%08x val=%08x len=%d\n", pGlobals->uConfigReg, val, len));

    if (!(pGlobals->uConfigReg & (1 << 31)))
        return VINF_SUCCESS;

    if ((pGlobals->uConfigReg & 0x3) != 0)
        return VINF_SUCCESS;

    /* Compute destination device */
    ich9pciStateToPciAddr(pGlobals, addr, &aPciAddr);

    return ich9pciDataWriteAddr(pGlobals, &aPciAddr, val, len, VINF_IOM_HC_IOPORT_WRITE);
}

static void ich9pciNoMem(void* ptr, int cb)
{
    for (int i = 0; i < cb; i++)
        ((uint8_t*)ptr)[i] = 0xff;
}

/**
 * Port I/O Handler for PCI data OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the OUT operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int)  ich9pciIOPortDataWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    Log(("pciIOPortDataWrite: Port=%#x u32=%#x cb=%d\n", Port, u32, cb));
    NOREF(pvUser);
    int rc = VINF_SUCCESS;
    if (!(Port % cb))
    {
        PCI_LOCK(pDevIns, VINF_IOM_HC_IOPORT_WRITE);
        rc = ich9pciDataWrite(PDMINS_2_DATA(pDevIns, PPCIGLOBALS), Port, u32, cb);
        PCI_UNLOCK(pDevIns);
    }
    else
        AssertMsgFailed(("Unaligned write to port %#x u32=%#x cb=%d\n", Port, u32, cb));
    return rc;
}

static int ich9pciDataReadAddr(PPCIGLOBALS pGlobals, PciAddress* pPciAddr, int cb,
                               uint32_t *pu32, int rcReschedule)
{
    int rc = VINF_SUCCESS;

    if (pPciAddr->iRegister > 0xff)
    {
        LogRel(("PCI: attempt to read extended register: %x\n", pPciAddr->iRegister));
        ich9pciNoMem(pu32, cb);
        goto out;
    }


    if (pPciAddr->iBus != 0)
    {
        if (pGlobals->aPciBus.cBridges)
        {
#ifdef IN_RING3 /** @todo do lookup in R0/RC too! */
            PPCIDEVICE pBridgeDevice = ich9pciFindBridge(&pGlobals->aPciBus, pPciAddr->iBus);
            if (pBridgeDevice)
            {
                AssertPtr(pBridgeDevice->Int.s.pfnBridgeConfigRead);
                *pu32 = pBridgeDevice->Int.s.pfnBridgeConfigRead(pBridgeDevice->pDevIns, pPciAddr->iBus, pPciAddr->iDeviceFunc, pPciAddr->iRegister, cb);
            }
            else
                ich9pciNoMem(pu32, cb);
#else
            rc = rcReschedule;
            goto out;
#endif
        } else
            ich9pciNoMem(pu32, cb);
    }
    else
    {
        if (pGlobals->aPciBus.apDevices[pPciAddr->iDeviceFunc])
        {
#ifdef IN_RING3
            R3PTRTYPE(PCIDevice *) aDev = pGlobals->aPciBus.apDevices[pPciAddr->iDeviceFunc];
            *pu32 = aDev->Int.s.pfnConfigRead(aDev, pPciAddr->iRegister, cb);
            Log(("ich9pciDataReadAddr: %s: addr=%02x val=%08x len=%d\n", aDev->name, pPciAddr->iRegister, *pu32, cb));
#else
            rc = rcReschedule;
            goto out;
#endif
        }
        else
            ich9pciNoMem(pu32, cb);
    }

  out:
    Log2(("ich9pciDataReadAddr: %02x:%02x:%02x reg %x(%d) gave %x %Rrc\n",
          pPciAddr->iBus, pPciAddr->iDeviceFunc >> 3, pPciAddr->iDeviceFunc & 0x7, pPciAddr->iRegister,
          cb, *pu32, rc));

    return rc;
}

static int ich9pciDataRead(PPCIGLOBALS pGlobals, uint32_t addr, int cb, uint32_t *pu32)
{
    PciAddress aPciAddr;

    LogFlow(("ich9pciDataRead: config=%x cb=%d\n",  pGlobals->uConfigReg, cb));

    *pu32 = 0xffffffff;

    if (!(pGlobals->uConfigReg & (1 << 31)))
        return VINF_SUCCESS;

    if ((pGlobals->uConfigReg & 0x3) != 0)
        return VINF_SUCCESS;

    /* Compute destination device */
    ich9pciStateToPciAddr(pGlobals, addr, &aPciAddr);

    return ich9pciDataReadAddr(pGlobals, &aPciAddr, cb, pu32, VINF_IOM_HC_IOPORT_READ);
}

/**
 * Port I/O Handler for PCI data IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
PDMBOTHCBDECL(int)  ich9pciIOPortDataRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    NOREF(pvUser);
    if (!(Port % cb))
    {
        PCI_LOCK(pDevIns, VINF_IOM_HC_IOPORT_READ);
        int rc = ich9pciDataRead(PDMINS_2_DATA(pDevIns, PPCIGLOBALS), Port, cb, pu32);
        PCI_UNLOCK(pDevIns);
        Log(("pciIOPortDataRead: Port=%#x cb=%#x -> %#x (%Rrc)\n", Port, cb, *pu32, rc));
        return rc;
    }
    AssertMsgFailed(("Unaligned read from port %#x cb=%d\n", Port, cb));
    return VERR_IOM_IOPORT_UNUSED;
}

/* Compute mapping of PCI slot and IRQ number to APIC interrupt line */
DECLINLINE(int) ich9pciSlot2ApicIrq(uint8_t uSlot, int irq_num)
{
    return (irq_num + uSlot) & 7;
}

/* Add one more level up request on APIC input line */
DECLINLINE(void) ich9pciApicLevelUp(PPCIGLOBALS pGlobals, int irq_num)
{
    ASMAtomicIncU32(&pGlobals->uaPciApicIrqLevels[irq_num]);
}

/* Remove one level up request on APIC input line */
DECLINLINE(void) ich9pciApicLevelDown(PPCIGLOBALS pGlobals, int irq_num)
{
    ASMAtomicDecU32(&pGlobals->uaPciApicIrqLevels[irq_num]);
}

static void ich9pciApicSetIrq(PPCIBUS pBus, uint8_t uDevFn, PCIDevice *pPciDev, int irq_num1, int iLevel, int iForcedIrq)
{
    /* This is only allowed to be called with a pointer to the root bus. */
    AssertMsg(pBus->iBus == 0, ("iBus=%u\n", pBus->iBus));

    if (iForcedIrq == -1)
    {
        int apic_irq, apic_level;
        PPCIGLOBALS pGlobals = PCIROOTBUS_2_PCIGLOBALS(pBus);
        int irq_num = ich9pciSlot2ApicIrq(uDevFn >> 3, irq_num1);

        if ((iLevel & PDM_IRQ_LEVEL_HIGH) == PDM_IRQ_LEVEL_HIGH)
            ich9pciApicLevelUp(pGlobals, irq_num);
        else if ((iLevel & PDM_IRQ_LEVEL_HIGH) == PDM_IRQ_LEVEL_LOW)
            ich9pciApicLevelDown(pGlobals, irq_num);

        apic_irq = irq_num + 0x10;
        apic_level = pGlobals->uaPciApicIrqLevels[irq_num] != 0;
        Log3(("ich9pciApicSetIrq: %s: irq_num1=%d level=%d apic_irq=%d apic_level=%d irq_num1=%d\n",
              R3STRING(pPciDev->name), irq_num1, iLevel, apic_irq, apic_level, irq_num));
        pBus->CTX_SUFF(pPciHlp)->pfnIoApicSetIrq(pBus->CTX_SUFF(pDevIns), apic_irq, apic_level);

        if ((iLevel & PDM_IRQ_LEVEL_FLIP_FLOP) == PDM_IRQ_LEVEL_FLIP_FLOP)
        {
            /**
             *  we raised it few lines above, as PDM_IRQ_LEVEL_FLIP_FLOP has
             * PDM_IRQ_LEVEL_HIGH bit set
             */
            ich9pciApicLevelDown(pGlobals, irq_num);
            pPciDev->Int.s.uIrqPinState = PDM_IRQ_LEVEL_LOW;
            apic_level = pGlobals->uaPciApicIrqLevels[irq_num] != 0;
            Log3(("ich9pciApicSetIrq: %s: irq_num1=%d level=%d apic_irq=%d apic_level=%d irq_num1=%d (flop)\n",
                  R3STRING(pPciDev->name), irq_num1, iLevel, apic_irq, apic_level, irq_num));
            pBus->CTX_SUFF(pPciHlp)->pfnIoApicSetIrq(pBus->CTX_SUFF(pDevIns), apic_irq, apic_level);
        }
    } else {
        Log3(("ich9pciApicSetIrq: (forced) %s: irq_num1=%d level=%d acpi_irq=%d\n",
              R3STRING(pPciDev->name), irq_num1, iLevel, iForcedIrq));
        pBus->CTX_SUFF(pPciHlp)->pfnIoApicSetIrq(pBus->CTX_SUFF(pDevIns), iForcedIrq, iLevel);
    }
}

static void ich9pciSetIrqInternal(PPCIGLOBALS pGlobals, uint8_t uDevFn, PPCIDEVICE pPciDev, int iIrq, int iLevel)
{

    if (PCIDevIsIntxDisabled(pPciDev))
    {
        if (MsiIsEnabled(pPciDev))
        {
            PPDMDEVINS pDevIns = pGlobals->aPciBus.CTX_SUFF(pDevIns);
            MsiNotify(pDevIns, pGlobals->aPciBus.CTX_SUFF(pPciHlp), pPciDev, iIrq, iLevel);
        }

        if (MsixIsEnabled(pPciDev))
        {
            PPDMDEVINS pDevIns = pGlobals->aPciBus.CTX_SUFF(pDevIns);
            MsixNotify(pDevIns, pGlobals->aPciBus.CTX_SUFF(pPciHlp), pPciDev, iIrq, iLevel);
        }
        return;
    }

    PPCIBUS     pBus =     &pGlobals->aPciBus;
    const bool  fIsAcpiDevice = PCIDevGetDeviceId(pPciDev) == 0x7113;

    /* Check if the state changed. */
    if (pPciDev->Int.s.uIrqPinState != iLevel)
    {
        pPciDev->Int.s.uIrqPinState = (iLevel & PDM_IRQ_LEVEL_HIGH);

        /* Send interrupt to I/O APIC only now. */
        if (fIsAcpiDevice)
            /*
             * ACPI needs special treatment since SCI is hardwired and
             * should not be affected by PCI IRQ routing tables at the
             * same time SCI IRQ is shared in PCI sense hence this
             * kludge (i.e. we fetch the hardwired value from ACPIs
             * PCI device configuration space).
             */
            ich9pciApicSetIrq(pBus, uDevFn, pPciDev, -1, iLevel, PCIDevGetInterruptLine(pPciDev));
        else
            ich9pciApicSetIrq(pBus, uDevFn, pPciDev, iIrq, iLevel, -1);
    }
}

PDMBOTHCBDECL(int)  ich9pciMcfgMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    PPCIGLOBALS pGlobals = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
    PciAddress aDest;
    uint32_t u32 = 0;

    Log2(("ich9pciMcfgMMIOWrite: %RGp(%d) \n", GCPhysAddr, cb));

    PCI_LOCK(pDevIns, VINF_IOM_HC_MMIO_WRITE);

    ich9pciPhysToPciAddr(pGlobals, GCPhysAddr, &aDest);

    switch (cb)
    {
        case 1:
            u32 = *(uint8_t*)pv;
            break;
        case 2:
            u32 = *(uint16_t*)pv;
            break;
        case 4:
            u32 = *(uint32_t*)pv;
            break;
        default:
            Assert(false);
            break;
    }
    int rc = ich9pciDataWriteAddr(pGlobals, &aDest, u32, cb, VINF_IOM_HC_MMIO_WRITE);
    PCI_UNLOCK(pDevIns);

    return rc;
}

PDMBOTHCBDECL(int)  ich9pciMcfgMMIORead (PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    PPCIGLOBALS pGlobals = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
    PciAddress  aDest;
    uint32_t    rv;

    LogFlow(("ich9pciMcfgMMIORead: %RGp(%d) \n", GCPhysAddr, cb));

    PCI_LOCK(pDevIns, VINF_IOM_HC_MMIO_READ);

    ich9pciPhysToPciAddr(pGlobals, GCPhysAddr, &aDest);

    int rc = ich9pciDataReadAddr(pGlobals, &aDest, cb, &rv, VINF_IOM_HC_MMIO_READ);

    if (RT_SUCCESS(rc))
    {
        switch (cb)
        {
            case 1:
                *(uint8_t*)pv   = (uint8_t)rv;
                break;
            case 2:
                *(uint16_t*)pv  = (uint16_t)rv;
                break;
            case 4:
                *(uint32_t*)pv  = (uint32_t)rv;
                break;
            default:
                Assert(false);
                break;
        }
    }
    PCI_UNLOCK(pDevIns);

    return rc;
}

#ifdef IN_RING3

DECLINLINE(PPCIDEVICE) ich9pciFindBridge(PPCIBUS pBus, uint8_t iBus)
{
    /* Search for a fitting bridge. */
    for (uint32_t iBridge = 0; iBridge < pBus->cBridges; iBridge++)
    {
        /*
         * Examine secondary and subordinate bus number.
         * If the target bus is in the range we pass the request on to the bridge.
         */
        PPCIDEVICE pBridgeTemp = pBus->papBridgesR3[iBridge];
        AssertMsg(pBridgeTemp && PCIIsPci2PciBridge(pBridgeTemp),
                  ("Device is not a PCI bridge but on the list of PCI bridges\n"));

        if (   iBus >= PCIDevGetByte(pBridgeTemp, VBOX_PCI_SECONDARY_BUS)
            && iBus <= PCIDevGetByte(pBridgeTemp, VBOX_PCI_SUBORDINATE_BUS))
            return pBridgeTemp;
    }

    /* Nothing found. */
    return NULL;
}

DECLINLINE(uint32_t) ich9pciGetRegionReg(int iRegion)
{
    return (iRegion == VBOX_PCI_ROM_SLOT) ?
            VBOX_PCI_ROM_ADDRESS : (VBOX_PCI_BASE_ADDRESS_0 + iRegion * 4);
}

#define INVALID_PCI_ADDRESS ~0U

static void ich9pciUpdateMappings(PCIDevice* pDev)
{
    PPCIBUS pBus = pDev->Int.s.CTX_SUFF(pBus);
    uint32_t uLast, uNew;

    int iCmd = PCIDevGetCommand(pDev);
    for (int iRegion = 0; iRegion < PCI_NUM_REGIONS; iRegion++)
    {
        PCIIORegion* pRegion = &pDev->Int.s.aIORegions[iRegion];
        uint32_t uConfigReg = ich9pciGetRegionReg(iRegion);
        int32_t  iRegionSize =  pRegion->size;
        int rc;

        if (iRegionSize == 0)
            continue;

        AssertMsg((pRegion->type & PCI_ADDRESS_SPACE_BAR64) == 0, ("64-bit BARs not yet implemented\n"));

        if (pRegion->type & PCI_ADDRESS_SPACE_IO)
        {
            /* port IO region */
            if (iCmd & PCI_COMMAND_IOACCESS)
            {
                /* IO access allowed */
                uNew = ich9pciConfigReadDev(pDev, uConfigReg, 4);
                uNew &= ~(iRegionSize - 1);
                uLast = uNew + iRegionSize - 1;
                /* only 64K ioports on PC */
                if (uLast <= uNew || uNew == 0 || uLast >= 0x10000)
                    uNew = INVALID_PCI_ADDRESS;
            } else
                uNew = INVALID_PCI_ADDRESS;
        }
        else
        {
            /* MMIO region */
            if (iCmd & PCI_COMMAND_MEMACCESS)
            {
                uNew = ich9pciConfigReadDev(pDev, uConfigReg, 4);
                /* the ROM slot has a specific enable bit */
                if (iRegion == PCI_ROM_SLOT && !(uNew & 1))
                    uNew = INVALID_PCI_ADDRESS;
                else
                {
                    uNew &= ~(iRegionSize - 1);
                    uLast = uNew + iRegionSize - 1;
                    /* NOTE: we do not support wrapping */
                    /* XXX: as we cannot support really dynamic
                       mappings, we handle specific values as invalid
                       mappings. */
                    if (uLast <= uNew || uNew == 0 || uLast == INVALID_PCI_ADDRESS)
                        uNew = INVALID_PCI_ADDRESS;
                }
            } else
                uNew = INVALID_PCI_ADDRESS;
        }
        /* now do the real mapping */
        if (uNew != pRegion->addr)
        {
            if (pRegion->addr != INVALID_PCI_ADDRESS)
            {
                if (pRegion->type & PCI_ADDRESS_SPACE_IO)
                {
                    /* Port IO */
                    rc = PDMDevHlpIOPortDeregister(pDev->pDevIns, pRegion->addr, pRegion->size);
                    AssertRC(rc);
                }
                else
                {
                    RTGCPHYS GCPhysBase = pRegion->addr;
                    if (pBus->pPciHlpR3->pfnIsMMIO2Base(pBus->pDevInsR3, pDev->pDevIns, GCPhysBase))
                    {
                        /* unmap it. */
                        rc = pRegion->map_func(pDev, iRegion, NIL_RTGCPHYS, pRegion->size, (PCIADDRESSSPACE)(pRegion->type));
                        AssertRC(rc);
                        rc = PDMDevHlpMMIO2Unmap(pDev->pDevIns, iRegion, GCPhysBase);
                    }
                    else
                        rc = PDMDevHlpMMIODeregister(pDev->pDevIns, GCPhysBase, pRegion->size);
                    AssertMsgRC(rc, ("rc=%Rrc d=%s i=%d GCPhysBase=%RGp size=%#x\n", rc, pDev->name, iRegion, GCPhysBase, pRegion->size));
                }
            }
            pRegion->addr = uNew;
            if (pRegion->addr != INVALID_PCI_ADDRESS)
            {
                /* finally, map the region */
                rc = pRegion->map_func(pDev, iRegion,
                                       pRegion->addr, pRegion->size,
                                       (PCIADDRESSSPACE)(pRegion->type));
                AssertRC(rc);
            }
        }
    }
}

static DECLCALLBACK(int) ich9pciRegister(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, const char *pszName, int iDev)
{
    PPCIBUS     pBus = DEVINS_2_PCIBUS(pDevIns);

    /*
     * Check input.
     */
    if (    !pszName
        ||  !pPciDev
        ||  iDev >= (int)RT_ELEMENTS(pBus->apDevices)
        )
    {
        AssertMsgFailed(("Invalid argument! pszName=%s pPciDev=%p iDev=%d\n", pszName, pPciDev, iDev));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Register the device.
     */
    return ich9pciRegisterInternal(pBus, iDev, pPciDev, pszName);
}


static DECLCALLBACK(int) ich9pciRegisterMsi(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PPDMMSIREG pMsiReg)
{
    int rc;

    rc = MsiInit(pPciDev, pMsiReg);
    if (rc != VINF_SUCCESS)
        return rc;

    rc = MsixInit(pPciDev->Int.s.CTX_SUFF(pBus)->CTX_SUFF(pPciHlp), pPciDev, pMsiReg);
    if (rc != VINF_SUCCESS)
        return rc;

    return rc;
}


static DECLCALLBACK(int) ich9pcibridgeRegister(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, const char *pszName, int iDev)
{

    PPCIBUS pBus = PDMINS_2_DATA(pDevIns, PPCIBUS);

    /*
     * Check input.
     */
    if (    !pszName
        ||  !pPciDev
        ||  iDev >= (int)RT_ELEMENTS(pBus->apDevices))
    {
        AssertMsgFailed(("Invalid argument! pszName=%s pPciDev=%p iDev=%d\n", pszName, pPciDev, iDev));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Register the device.
     */
    return ich9pciRegisterInternal(pBus, iDev, pPciDev, pszName);
}

static DECLCALLBACK(int) ich9pciIORegionRegister(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iRegion, uint32_t cbRegion, PCIADDRESSSPACE enmType, PFNPCIIOREGIONMAP pfnCallback)
{
    /*
     * Validate.
     */
    AssertMsgReturn(   enmType == PCI_ADDRESS_SPACE_MEM
                    || enmType == PCI_ADDRESS_SPACE_IO
                    || enmType == PCI_ADDRESS_SPACE_MEM_PREFETCH,
                    ("Invalid enmType=%#x? Or was this a bitmask after all...\n", enmType),
                    VERR_INVALID_PARAMETER);
    AssertMsgReturn((unsigned)iRegion < PCI_NUM_REGIONS,
                    ("Invalid iRegion=%d PCI_NUM_REGIONS=%d\n", iRegion, PCI_NUM_REGIONS),
                    VERR_INVALID_PARAMETER);
    int iLastSet = ASMBitLastSetU32(cbRegion);
    AssertMsgReturn(    iLastSet != 0
                    &&  RT_BIT_32(iLastSet - 1) == cbRegion,
                    ("Invalid cbRegion=%#x iLastSet=%#x (not a power of 2 or 0)\n", cbRegion, iLastSet),
                    VERR_INVALID_PARAMETER);

    /*
     * Register the I/O region.
     */
    PPCIIOREGION pRegion = &pPciDev->Int.s.aIORegions[iRegion];
    pRegion->addr        = INVALID_PCI_ADDRESS;
    pRegion->size        = cbRegion;
    pRegion->type        = enmType;
    pRegion->map_func    = pfnCallback;

    /* Set type in the config space. */
    uint32_t u32Address = ich9pciGetRegionReg(iRegion);
    uint32_t u32Value   =   (enmType == PCI_ADDRESS_SPACE_MEM_PREFETCH ? (1 << 3) : 0)
                          | (enmType == PCI_ADDRESS_SPACE_IO ? 1 : 0);
    PCIDevSetDWord(pPciDev, u32Address, u32Value);

    return VINF_SUCCESS;
}

static DECLCALLBACK(void) ich9pciSetConfigCallbacks(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PFNPCICONFIGREAD pfnRead, PPFNPCICONFIGREAD ppfnReadOld,
                                                    PFNPCICONFIGWRITE pfnWrite, PPFNPCICONFIGWRITE ppfnWriteOld)
{
    if (ppfnReadOld)
        *ppfnReadOld = pPciDev->Int.s.pfnConfigRead;
    pPciDev->Int.s.pfnConfigRead  = pfnRead;

    if (ppfnWriteOld)
        *ppfnWriteOld = pPciDev->Int.s.pfnConfigWrite;
    pPciDev->Int.s.pfnConfigWrite = pfnWrite;
}

/**
 * Saves a state of the PCI device.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the PCI Bus.
 * @param   pPciDev         Pointer to PCI device.
 * @param   pSSM            The handle to save the state to.
 */
static DECLCALLBACK(int) ich9pciGenericSaveExec(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PSSMHANDLE pSSM)
{
    return SSMR3PutMem(pSSM, &pPciDev->config[0], sizeof(pPciDev->config));
}

static int ich9pciR3CommonSaveExec(PPCIBUS pBus, PSSMHANDLE pSSM)
{
    /*
     * Iterate thru all the devices.
     */
    for (uint32_t i = 0; i < RT_ELEMENTS(pBus->apDevices); i++)
    {
        PPCIDEVICE pDev = pBus->apDevices[i];
        if (pDev)
        {
            /* Device position */
            SSMR3PutU32(pSSM, i);
            /* PCI config registers */
            SSMR3PutMem(pSSM, pDev->config, sizeof(pDev->config));

            /* Device flags */
            int rc = SSMR3PutU32(pSSM, pDev->Int.s.uFlags);
            if (RT_FAILURE(rc))
                return rc;

            /* IRQ pin state */
            rc = SSMR3PutS32(pSSM, pDev->Int.s.uIrqPinState);
            if (RT_FAILURE(rc))
                return rc;

            /* MSI info */
            rc = SSMR3PutU8(pSSM, pDev->Int.s.u8MsiCapOffset);
            if (RT_FAILURE(rc))
                return rc;
            rc = SSMR3PutU8(pSSM, pDev->Int.s.u8MsiCapSize);
            if (RT_FAILURE(rc))
                return rc;

            /* MSI-X info */
            rc = SSMR3PutU8(pSSM, pDev->Int.s.u8MsixCapOffset);
            if (RT_FAILURE(rc))
                return rc;
            rc = SSMR3PutU8(pSSM, pDev->Int.s.u8MsixCapSize);
            if (RT_FAILURE(rc))
                return rc;
            /* Save MSI-X page state */
            if (pDev->Int.s.u8MsixCapOffset != 0)
            {
                Assert(pDev->Int.s.pMsixPageR3 != NULL);
                SSMR3PutMem(pSSM, pDev->Int.s.pMsixPageR3, 0x1000);
                if (RT_FAILURE(rc))
                    return rc;
            }
        }
    }
    return SSMR3PutU32(pSSM, UINT32_MAX); /* terminator */
}

static DECLCALLBACK(int) ich9pciR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PPCIGLOBALS pThis = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);

    /*
     * Bus state data.
     */
    SSMR3PutU32(pSSM, pThis->uConfigReg);

    /*
     * Save IRQ states.
     */
    for (int i = 0; i < PCI_APIC_IRQ_PINS; i++)
        SSMR3PutU32(pSSM, pThis->uaPciApicIrqLevels[i]);

    SSMR3PutU32(pSSM, ~0);        /* separator */

    return ich9pciR3CommonSaveExec(&pThis->aPciBus, pSSM);
}


static DECLCALLBACK(int) ich9pcibridgeR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PPCIBUS pThis = PDMINS_2_DATA(pDevIns, PPCIBUS);
    return ich9pciR3CommonSaveExec(pThis, pSSM);
}


static void ich9pcibridgeConfigWrite(PPDMDEVINSR3 pDevIns, uint8_t iBus, uint8_t iDevice, uint32_t u32Address, uint32_t u32Value, unsigned cb)
{
    PPCIBUS pBus = PDMINS_2_DATA(pDevIns, PPCIBUS);

    LogFlowFunc((": pDevIns=%p iBus=%d iDevice=%d u32Address=%u u32Value=%u cb=%d\n", pDevIns, iBus, iDevice, u32Address, u32Value, cb));

    /* If the current bus is not the target bus search for the bus which contains the device. */
    if (iBus != PCIDevGetByte(&pBus->aPciDev, VBOX_PCI_SECONDARY_BUS))
    {
        PPCIDEVICE pBridgeDevice = ich9pciFindBridge(pBus, iBus);
        if (pBridgeDevice)
        {
            AssertPtr(pBridgeDevice->Int.s.pfnBridgeConfigWrite);
            pBridgeDevice->Int.s.pfnBridgeConfigWrite(pBridgeDevice->pDevIns, iBus, iDevice, u32Address, u32Value, cb);
        }
    }
    else
    {
        /* This is the target bus, pass the write to the device. */
        PPCIDEVICE pPciDev = pBus->apDevices[iDevice];
        if (pPciDev)
        {
            Log(("%s: %s: addr=%02x val=%08x len=%d\n", __FUNCTION__, pPciDev->name, u32Address, u32Value, cb));
            pPciDev->Int.s.pfnConfigWrite(pPciDev, u32Address, u32Value, cb);
        }
    }
}

static uint32_t ich9pcibridgeConfigRead(PPDMDEVINSR3 pDevIns, uint8_t iBus, uint8_t iDevice, uint32_t u32Address, unsigned cb)
{
    PPCIBUS pBus = PDMINS_2_DATA(pDevIns, PPCIBUS);
    uint32_t u32Value;

    LogFlowFunc((": pDevIns=%p iBus=%d iDevice=%d u32Address=%u cb=%d\n", pDevIns, iBus, iDevice, u32Address, cb));

    /* If the current bus is not the target bus search for the bus which contains the device. */
    if (iBus != PCIDevGetByte(&pBus->aPciDev, VBOX_PCI_SECONDARY_BUS))
    {
        PPCIDEVICE pBridgeDevice = ich9pciFindBridge(pBus, iBus);
        if (pBridgeDevice)
        {
            AssertPtr( pBridgeDevice->Int.s.pfnBridgeConfigRead);
            u32Value = pBridgeDevice->Int.s.pfnBridgeConfigRead(pBridgeDevice->pDevIns, iBus, iDevice, u32Address, cb);
        }
        else
            ich9pciNoMem(&u32Value, 4);
    }
    else
    {
        /* This is the target bus, pass the read to the device. */
        PPCIDEVICE pPciDev = pBus->apDevices[iDevice];
        if (pPciDev)
        {
            u32Value = pPciDev->Int.s.pfnConfigRead(pPciDev, u32Address, cb);
            Log(("%s: %s: u32Address=%02x u32Value=%08x cb=%d\n", __FUNCTION__, pPciDev->name, u32Address, u32Value, cb));
        }
        else
            ich9pciNoMem(&u32Value, 4);
    }

    return u32Value;
}


/**
 * Common routine for restoring the config registers of a PCI device.
 *
 * @param   pDev                The PCI device.
 * @param   pbSrcConfig         The configuration register values to be loaded.
 * @param   fIsBridge           Whether this is a bridge device or not.
 */
static void pciR3CommonRestoreConfig(PPCIDEVICE pDev, uint8_t const *pbSrcConfig, bool fIsBridge)
{
    /*
     * This table defines the fields for normal devices and bridge devices, and
     * the order in which they need to be restored.
     */
    static const struct PciField
    {
        uint8_t     off;
        uint8_t     cb;
        uint8_t     fWritable;
        uint8_t     fBridge;
        const char *pszName;
    } s_aFields[] =
    {
        /* off,cb,fW,fB, pszName */
        { VBOX_PCI_VENDOR_ID, 2, 0, 3, "VENDOR_ID" },
        { VBOX_PCI_DEVICE_ID, 2, 0, 3, "DEVICE_ID" },
        { VBOX_PCI_STATUS, 2, 1, 3, "STATUS" },
        { VBOX_PCI_REVISION_ID, 1, 0, 3, "REVISION_ID" },
        { VBOX_PCI_CLASS_PROG, 1, 0, 3, "CLASS_PROG" },
        { VBOX_PCI_CLASS_SUB, 1, 0, 3, "CLASS_SUB" },
        { VBOX_PCI_CLASS_BASE, 1, 0, 3, "CLASS_BASE" },
        { VBOX_PCI_CACHE_LINE_SIZE, 1, 1, 3, "CACHE_LINE_SIZE" },
        { VBOX_PCI_LATENCY_TIMER, 1, 1, 3, "LATENCY_TIMER" },
        { VBOX_PCI_HEADER_TYPE, 1, 0, 3, "HEADER_TYPE" },
        { VBOX_PCI_BIST, 1, 1, 3, "BIST" },
        { VBOX_PCI_BASE_ADDRESS_0, 4, 1, 3, "BASE_ADDRESS_0" },
        { VBOX_PCI_BASE_ADDRESS_1, 4, 1, 3, "BASE_ADDRESS_1" },
        { VBOX_PCI_BASE_ADDRESS_2, 4, 1, 1, "BASE_ADDRESS_2" },
        { VBOX_PCI_PRIMARY_BUS, 1, 1, 2, "PRIMARY_BUS" },       // fWritable = ??
        { VBOX_PCI_SECONDARY_BUS, 1, 1, 2, "SECONDARY_BUS" },     // fWritable = ??
        { VBOX_PCI_SUBORDINATE_BUS, 1, 1, 2, "SUBORDINATE_BUS" },   // fWritable = ??
        { VBOX_PCI_SEC_LATENCY_TIMER, 1, 1, 2, "SEC_LATENCY_TIMER" }, // fWritable = ??
        { VBOX_PCI_BASE_ADDRESS_3, 4, 1, 1, "BASE_ADDRESS_3" },
        { VBOX_PCI_IO_BASE, 1, 1, 2, "IO_BASE" },           // fWritable = ??
        { VBOX_PCI_IO_LIMIT, 1, 1, 2, "IO_LIMIT" },          // fWritable = ??
        { VBOX_PCI_SEC_STATUS, 2, 1, 2, "SEC_STATUS" },        // fWritable = ??
        { VBOX_PCI_BASE_ADDRESS_4, 4, 1, 1, "BASE_ADDRESS_4" },
        { VBOX_PCI_MEMORY_BASE, 2, 1, 2, "MEMORY_BASE" },       // fWritable = ??
        { VBOX_PCI_MEMORY_LIMIT, 2, 1, 2, "MEMORY_LIMIT" },      // fWritable = ??
        { VBOX_PCI_BASE_ADDRESS_5, 4, 1, 1, "BASE_ADDRESS_5" },
        { VBOX_PCI_PREF_MEMORY_BASE, 2, 1, 2, "PREF_MEMORY_BASE" },  // fWritable = ??
        { VBOX_PCI_PREF_MEMORY_LIMIT, 2, 1, 2, "PREF_MEMORY_LIMIT" }, // fWritable = ??
        { VBOX_PCI_CARDBUS_CIS, 4, 1, 1, "CARDBUS_CIS" },       // fWritable = ??
        { VBOX_PCI_PREF_BASE_UPPER32, 4, 1, 2, "PREF_BASE_UPPER32" }, // fWritable = ??
        { VBOX_PCI_SUBSYSTEM_VENDOR_ID, 2, 0, 1, "SUBSYSTEM_VENDOR_ID" },// fWritable = !?
        { VBOX_PCI_PREF_LIMIT_UPPER32, 4, 1, 2, "PREF_LIMIT_UPPER32" },// fWritable = ??
        { VBOX_PCI_SUBSYSTEM_ID, 2, 0, 1, "SUBSYSTEM_ID" },      // fWritable = !?
        { VBOX_PCI_ROM_ADDRESS, 4, 1, 1, "ROM_ADDRESS" },       // fWritable = ?!
        { VBOX_PCI_IO_BASE_UPPER16, 2, 1, 2, "IO_BASE_UPPER16" },   // fWritable = ?!
        { VBOX_PCI_IO_LIMIT_UPPER16, 2, 1, 2, "IO_LIMIT_UPPER16" },  // fWritable = ?!
        { VBOX_PCI_CAPABILITY_LIST, 4, 0, 3, "CAPABILITY_LIST" },   // fWritable = !? cb=!?
        { VBOX_PCI_RESERVED_38, 4, 1, 1, "RESERVED_38" },               // ???
        { VBOX_PCI_ROM_ADDRESS_BR, 4, 1, 2, "ROM_ADDRESS_BR" },    // fWritable = !? cb=!? fBridge=!?
        { VBOX_PCI_INTERRUPT_LINE, 1, 1, 3, "INTERRUPT_LINE" },    // fBridge=??
        { VBOX_PCI_INTERRUPT_PIN, 1, 0, 3, "INTERRUPT_PIN" },     // fBridge=??
        { VBOX_PCI_MIN_GNT, 1, 0, 1, "MIN_GNT" },
        { VBOX_PCI_BRIDGE_CONTROL, 2, 1, 2, "BRIDGE_CONTROL" },    // fWritable = !?
        { VBOX_PCI_MAX_LAT, 1, 0, 1, "MAX_LAT" },
        /* The COMMAND register must come last as it requires the *ADDRESS*
           registers to be restored before we pretent to change it from 0 to
           whatever value the guest assigned it. */
        { VBOX_PCI_COMMAND, 2, 1, 3, "COMMAND" },
    };

#ifdef RT_STRICT
    /* Check that we've got full register coverage. */
    uint32_t bmDevice[0x40 / 32];
    uint32_t bmBridge[0x40 / 32];
    RT_ZERO(bmDevice);
    RT_ZERO(bmBridge);
    for (uint32_t i = 0; i < RT_ELEMENTS(s_aFields); i++)
    {
        uint8_t off = s_aFields[i].off;
        uint8_t cb  = s_aFields[i].cb;
        uint8_t f   = s_aFields[i].fBridge;
        while (cb-- > 0)
        {
            if (f & 1) AssertMsg(!ASMBitTest(bmDevice, off), ("%#x\n", off));
            if (f & 2) AssertMsg(!ASMBitTest(bmBridge, off), ("%#x\n", off));
            if (f & 1) ASMBitSet(bmDevice, off);
            if (f & 2) ASMBitSet(bmBridge, off);
            off++;
        }
    }
    for (uint32_t off = 0; off < 0x40; off++)
    {
        AssertMsg(ASMBitTest(bmDevice, off), ("%#x\n", off));
        AssertMsg(ASMBitTest(bmBridge, off), ("%#x\n", off));
    }
#endif

    /*
     * Loop thru the fields covering the 64 bytes of standard registers.
     */
    uint8_t const fBridge = fIsBridge ? 2 : 1;
    uint8_t *pbDstConfig = &pDev->config[0];
    for (uint32_t i = 0; i < RT_ELEMENTS(s_aFields); i++)
        if (s_aFields[i].fBridge & fBridge)
        {
            uint8_t const   off = s_aFields[i].off;
            uint8_t const   cb  = s_aFields[i].cb;
            uint32_t        u32Src;
            uint32_t        u32Dst;
            switch (cb)
            {
                case 1:
                    u32Src = pbSrcConfig[off];
                    u32Dst = pbDstConfig[off];
                    break;
                case 2:
                    u32Src = *(uint16_t const *)&pbSrcConfig[off];
                    u32Dst = *(uint16_t const *)&pbDstConfig[off];
                    break;
                case 4:
                    u32Src = *(uint32_t const *)&pbSrcConfig[off];
                    u32Dst = *(uint32_t const *)&pbDstConfig[off];
                    break;
                default:
                    AssertFailed();
                    continue;
            }

            if (    u32Src != u32Dst
                ||  off == VBOX_PCI_COMMAND)
            {
                if (u32Src != u32Dst)
                {
                    if (!s_aFields[i].fWritable)
                        LogRel(("PCI: %8s/%u: %2u-bit field %s: %x -> %x - !READ ONLY!\n",
                                pDev->name, pDev->pDevIns->iInstance, cb*8, s_aFields[i].pszName, u32Dst, u32Src));
                    else
                        LogRel(("PCI: %8s/%u: %2u-bit field %s: %x -> %x\n",
                                pDev->name, pDev->pDevIns->iInstance, cb*8, s_aFields[i].pszName, u32Dst, u32Src));
                }
                if (off == VBOX_PCI_COMMAND)
                    PCIDevSetCommand(pDev, 0); /* For remapping, see ich9pciR3CommonLoadExec. */
                pDev->Int.s.pfnConfigWrite(pDev, off, u32Src, cb);
            }
        }

    /*
     * The device dependent registers.
     *
     * We will not use ConfigWrite here as we have no clue about the size
     * of the registers, so the device is responsible for correctly
     * restoring functionality governed by these registers.
     */
    for (uint32_t off = 0x40; off < sizeof(pDev->config); off++)
        if (pbDstConfig[off] != pbSrcConfig[off])
        {
            LogRel(("PCI: %8s/%u: register %02x: %02x -> %02x\n",
                    pDev->name, pDev->pDevIns->iInstance, off, pbDstConfig[off], pbSrcConfig[off])); /** @todo make this Log() later. */
            pbDstConfig[off] = pbSrcConfig[off];
        }
}

/**
 * Common worker for ich9pciR3LoadExec and ich9pcibridgeR3LoadExec.
 *
 * @returns VBox status code.
 * @param   pBus                The bus which data is being loaded.
 * @param   pSSM                The saved state handle.
 * @param   uVersion            The data version.
 * @param   uPass               The pass.
 */
static DECLCALLBACK(int) ich9pciR3CommonLoadExec(PPCIBUS pBus, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    uint32_t    u32;
    uint32_t    i;
    int         rc;

    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    /*
     * Iterate thru all the devices and write 0 to the COMMAND register so
     * that all the memory is unmapped before we start restoring the saved
     * mapping locations.
     *
     * The register value is restored afterwards so we can do proper
     * LogRels in pciR3CommonRestoreConfig.
     */
    for (i = 0; i < RT_ELEMENTS(pBus->apDevices); i++)
    {
        PPCIDEVICE pDev = pBus->apDevices[i];
        if (pDev)
        {
            uint16_t u16 = PCIDevGetCommand(pDev);
            pDev->Int.s.pfnConfigWrite(pDev, VBOX_PCI_COMMAND, 0, 2);
            PCIDevSetCommand(pDev, u16);
            Assert(PCIDevGetCommand(pDev) == u16);
        }
    }

    void* pvMsixPage = RTMemTmpAllocZ(0x1000);
    /*
     * Iterate all the devices.
     */
    for (i = 0;; i++)
    {
        PPCIDEVICE  pDev;
        PCIDEVICE   DevTmp;

        /* index / terminator */
        rc = SSMR3GetU32(pSSM, &u32);
        if (RT_FAILURE(rc))
            return rc;
        if (u32 == (uint32_t)~0)
            break;
        if (    u32 >= RT_ELEMENTS(pBus->apDevices)
            ||  u32 < i)
        {
            AssertMsgFailed(("u32=%#x i=%#x\n", u32, i));
            goto out;
        }

        /* skip forward to the device checking that no new devices are present. */
        for (; i < u32; i++)
        {
            pDev = pBus->apDevices[i];
            if (pDev)
            {
                LogRel(("New device in slot %#x, %s (vendor=%#06x device=%#06x)\n", i, pDev->name,
                        PCIDevGetVendorId(pDev), PCIDevGetDeviceId(pDev)));
                if (SSMR3HandleGetAfter(pSSM) != SSMAFTER_DEBUG_IT)
                    return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("New device in slot %#x, %s (vendor=%#06x device=%#06x)"),
                                            i, pDev->name, PCIDevGetVendorId(pDev), PCIDevGetDeviceId(pDev));
            }
        }

        /* get the data */
        DevTmp.Int.s.uFlags = 0;
        DevTmp.Int.s.u8MsiCapOffset = 0;
        DevTmp.Int.s.u8MsiCapSize = 0;
        DevTmp.Int.s.u8MsixCapOffset = 0;
        DevTmp.Int.s.u8MsixCapSize = 0;
        DevTmp.Int.s.uIrqPinState = ~0; /* Invalid value in case we have an older saved state to force a state change in pciSetIrq. */
        SSMR3GetMem(pSSM, DevTmp.config, sizeof(DevTmp.config));

        rc = SSMR3GetU32(pSSM, &DevTmp.Int.s.uFlags);
        if (RT_FAILURE(rc))
            goto out;

        rc = SSMR3GetS32(pSSM, &DevTmp.Int.s.uIrqPinState);
        if (RT_FAILURE(rc))
            goto out;

        rc = SSMR3GetU8(pSSM, &DevTmp.Int.s.u8MsiCapOffset);
        if (RT_FAILURE(rc))
            goto out;

        rc = SSMR3GetU8(pSSM, &DevTmp.Int.s.u8MsiCapSize);
        if (RT_FAILURE(rc))
            goto out;

        rc = SSMR3GetU8(pSSM, &DevTmp.Int.s.u8MsixCapOffset);
        if (RT_FAILURE(rc))
            goto out;

        rc = SSMR3GetU8(pSSM, &DevTmp.Int.s.u8MsixCapSize);
        if (RT_FAILURE(rc))
            goto out;

        /* Load MSI-X page state */
        if (DevTmp.Int.s.u8MsixCapOffset != 0)
        {
            Assert(pvMsixPage != NULL);
            SSMR3GetMem(pSSM, pvMsixPage, 0x1000);
            if (RT_FAILURE(rc))
                goto out;
        }

        /* check that it's still around. */
        pDev = pBus->apDevices[i];
        if (!pDev)
        {
            LogRel(("Device in slot %#x has been removed! vendor=%#06x device=%#06x\n", i,
                    PCIDevGetVendorId(&DevTmp), PCIDevGetDeviceId(&DevTmp)));
            if (SSMR3HandleGetAfter(pSSM) != SSMAFTER_DEBUG_IT)
                return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Device in slot %#x has been removed! vendor=%#06x device=%#06x"),
                                        i, PCIDevGetVendorId(&DevTmp), PCIDevGetDeviceId(&DevTmp));
            continue;
        }

        /* match the vendor id assuming that this will never be changed. */
        if (    PCIDevGetVendorId(&DevTmp) != PCIDevGetVendorId(pDev))
            return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Device in slot %#x (%s) vendor id mismatch! saved=%.4Rhxs current=%.4Rhxs"),
                                     i, pDev->name, PCIDevGetVendorId(&DevTmp), PCIDevGetVendorId(pDev));

        /* commit the loaded device config. */
        pciR3CommonRestoreConfig(pDev, &DevTmp.config[0], false ); /** @todo fix bridge fun! */

        pDev->Int.s.uIrqPinState = DevTmp.Int.s.uIrqPinState;
        pDev->Int.s.u8MsiCapOffset  = DevTmp.Int.s.u8MsiCapOffset;
        pDev->Int.s.u8MsiCapSize    = DevTmp.Int.s.u8MsiCapSize;
        pDev->Int.s.u8MsixCapOffset = DevTmp.Int.s.u8MsixCapOffset;
        pDev->Int.s.u8MsixCapSize   = DevTmp.Int.s.u8MsixCapSize;
        if (DevTmp.Int.s.u8MsixCapSize != 0)
        {
            Assert(pDev->Int.s.pMsixPageR3 != NULL);
            memcpy(pDev->Int.s.pMsixPageR3, pvMsixPage, 0x1000);
        }
    }

  out:
    if (pvMsixPage)
        RTMemTmpFree(pvMsixPage);

    return rc;
}

/**
 * Loads a saved PCI device state.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the PCI Bus.
 * @param   pPciDev         Pointer to PCI device.
 * @param   pSSM            The handle to the saved state.
 */
static DECLCALLBACK(int) ich9pciGenericLoadExec(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PSSMHANDLE pSSM)
{
    return SSMR3GetMem(pSSM, &pPciDev->config[0], sizeof(pPciDev->config));
}

static DECLCALLBACK(int) ich9pciR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PPCIGLOBALS pThis = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
    PPCIBUS     pBus  = &pThis->aPciBus;
    uint32_t    u32;
    int         rc;

    /* We ignore this version as there's no saved state with it anyway */
    if (uVersion == VBOX_ICH9PCI_SAVED_STATE_VERSION_NOMSI)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    if (uVersion > VBOX_ICH9PCI_SAVED_STATE_VERSION_MSI)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    /*
     * Bus state data.
     */
    SSMR3GetU32(pSSM, &pThis->uConfigReg);

    /*
     * Load IRQ states.
     */
    for (int i = 0; i < PCI_APIC_IRQ_PINS; i++)
        SSMR3GetU32(pSSM, (uint32_t*)&pThis->uaPciApicIrqLevels[i]);

    /* separator */
    rc = SSMR3GetU32(pSSM, &u32);
    if (RT_FAILURE(rc))
        return rc;
    if (u32 != (uint32_t)~0)
        AssertMsgFailedReturn(("u32=%#x\n", u32), rc);

    return ich9pciR3CommonLoadExec(pBus, pSSM, uVersion, uPass);
}

static DECLCALLBACK(int) ich9pcibridgeR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PPCIBUS pThis = PDMINS_2_DATA(pDevIns, PPCIBUS);
    if (uVersion > VBOX_ICH9PCI_SAVED_STATE_VERSION_MSI)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    return ich9pciR3CommonLoadExec(pThis, pSSM, uVersion, uPass);
}

static uint32_t ich9pciConfigRead(PPCIGLOBALS pGlobals, uint8_t uBus, uint8_t uDevFn, uint32_t addr, uint32_t len)
{
    /* Will only work in LSB case */
    uint32_t   u32Val;
    PciAddress aPciAddr;

    aPciAddr.iBus = uBus;
    aPciAddr.iDeviceFunc = uDevFn;
    aPciAddr.iRegister = addr;

    /* cannot be rescheduled, as already in R3 */
    int rc = ich9pciDataReadAddr(pGlobals, &aPciAddr, len, &u32Val, VERR_INTERNAL_ERROR);
    AssertRC(rc);
    return u32Val;
}

static void ich9pciConfigWrite(PPCIGLOBALS pGlobals, uint8_t uBus, uint8_t uDevFn, uint32_t addr, uint32_t val, uint32_t len)
{
    PciAddress aPciAddr;

    aPciAddr.iBus = uBus;
    aPciAddr.iDeviceFunc = uDevFn;
    aPciAddr.iRegister = addr;

    /* cannot be rescheduled, as already in R3 */
    int rc = ich9pciDataWriteAddr(pGlobals, &aPciAddr, val, len, VERR_INTERNAL_ERROR);
    AssertRC(rc);
}

static void ich9pciSetRegionAddress(PPCIGLOBALS pGlobals, uint8_t uBus, uint8_t uDevFn, int iRegion, uint32_t addr)
{
    uint32_t uReg = ich9pciGetRegionReg(iRegion);

    /* Read memory type first. */
    uint8_t uResourceType = ich9pciConfigRead(pGlobals, uBus, uDevFn, uReg, 1);
    /* Read command register. */
    uint16_t uCmd = ich9pciConfigRead(pGlobals, uBus, uDevFn, VBOX_PCI_COMMAND, 2);

    if ( iRegion == PCI_ROM_SLOT )
        uCmd |= PCI_COMMAND_MEMACCESS;
    else if ((uResourceType & PCI_ADDRESS_SPACE_IO) == PCI_ADDRESS_SPACE_IO)
        uCmd |= PCI_COMMAND_IOACCESS; /* Enable I/O space access. */
    else /* The region is MMIO. */
        uCmd |= PCI_COMMAND_MEMACCESS; /* Enable MMIO access. */

    /* Write address of the device. */
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, uReg, addr, 4);

    /* enable memory mappings */
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_COMMAND, uCmd, 2);
}


static void ich9pciBiosInitBridge(PPCIGLOBALS pGlobals, uint8_t uBus, uint8_t uDevFn, uint8_t cBridgeDepth, uint8_t *paBridgePositions)
{
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_SECONDARY_BUS, pGlobals->uBus, 1);
    /* Temporary until we know how many other bridges are behind this one. */
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_SUBORDINATE_BUS, 0xff, 1);

    /* Add position of this bridge into the array. */
    paBridgePositions[cBridgeDepth+1] = (uDevFn >> 3);

    /*
     * The I/O range for the bridge must be aligned to a 4KB boundary.
     * This does not change anything really as the access to the device is not going
     * through the bridge but we want to be compliant to the spec.
     */
    if ((pGlobals->uPciBiosIo % 4096) != 0)
    {
        pGlobals->uPciBiosIo = RT_ALIGN_32(pGlobals->uPciBiosIo, 4*1024);
        Log(("%s: Aligned I/O start address. New address %#x\n", __FUNCTION__, pGlobals->uPciBiosIo));
    }
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_IO_BASE, (pGlobals->uPciBiosIo >> 8) & 0xf0, 1);

    /* The MMIO range for the bridge must be aligned to a 1MB boundary. */
    if ((pGlobals->uPciBiosMmio % (1024 * 1024)) != 0)
    {
        pGlobals->uPciBiosMmio = RT_ALIGN_32(pGlobals->uPciBiosMmio, 1024*1024);
        Log(("%s: Aligned MMIO start address. New address %#x\n", __FUNCTION__, pGlobals->uPciBiosMmio));
    }
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_MEMORY_BASE, (pGlobals->uPciBiosMmio >> 16) & UINT32_C(0xffff0), 2);

    /* Save values to compare later to. */
    uint32_t u32IoAddressBase = pGlobals->uPciBiosIo;
    uint32_t u32MMIOAddressBase = pGlobals->uPciBiosMmio;

    /* Init devices behind the bridge and possibly other bridges as well. */
    for (int iDev = 0; iDev <= 255; iDev++)
        ich9pciBiosInitDevice(pGlobals, uBus + 1, iDev, cBridgeDepth + 1, paBridgePositions);

    /* The number of bridges behind the this one is now available. */
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_SUBORDINATE_BUS, pGlobals->uBus, 1);

    /*
     * Set I/O limit register. If there is no device with I/O space behind the bridge
     * we set a lower value than in the base register.
     * The result with a real bridge is that no I/O transactions are passed to the secondary
     * interface. Again this doesn't really matter here but we want to be compliant to the spec.
     */
    if ((u32IoAddressBase != pGlobals->uPciBiosIo) && ((pGlobals->uPciBiosIo % 4096) != 0))
    {
        /* The upper boundary must be one byte less than a 4KB boundary. */
        pGlobals->uPciBiosIo = RT_ALIGN_32(pGlobals->uPciBiosIo, 4*1024);
    }

    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_IO_LIMIT, ((pGlobals->uPciBiosIo >> 8) & 0xf0) - 1, 1);

    /* Same with the MMIO limit register but with 1MB boundary here. */
    if ((u32MMIOAddressBase != pGlobals->uPciBiosMmio) && ((pGlobals->uPciBiosMmio % (1024 * 1024)) != 0))
    {
        /* The upper boundary must be one byte less than a 1MB boundary. */
        pGlobals->uPciBiosMmio = RT_ALIGN_32(pGlobals->uPciBiosMmio, 1024*1024);
    }
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_MEMORY_LIMIT, ((pGlobals->uPciBiosMmio >> 16) & UINT32_C(0xfff0)) - 1, 2);

    /*
     * Set the prefetch base and limit registers. We currently have no device with a prefetchable region
     * which may be behind a bridge. That's why it is unconditionally disabled here atm by writing a higher value into
     * the base register than in the limit register.
     */
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_PREF_MEMORY_BASE, 0xfff0, 2);
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_PREF_MEMORY_LIMIT, 0x0, 2);
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_PREF_BASE_UPPER32, 0x00, 4);
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_PREF_LIMIT_UPPER32, 0x00, 4);
}

static void ich9pciBiosInitDevice(PPCIGLOBALS pGlobals, uint8_t uBus, uint8_t uDevFn, uint8_t cBridgeDepth, uint8_t *paBridgePositions)
{
    uint32_t *paddr;
    uint16_t uDevClass, uVendor, uDevice;
    uint8_t uCmd;

    uDevClass  = ich9pciConfigRead(pGlobals, uBus, uDevFn, VBOX_PCI_CLASS_DEVICE, 2);
    uVendor    = ich9pciConfigRead(pGlobals, uBus, uDevFn, VBOX_PCI_VENDOR_ID, 2);
    uDevice    = ich9pciConfigRead(pGlobals, uBus, uDevFn, VBOX_PCI_DEVICE_ID, 2);

    /* If device is present */
    if (uVendor == 0xffff)
        return;

    switch (uDevClass)
    {
        case 0x0101:
            /* IDE controller */
            ich9pciConfigWrite(pGlobals, uBus, uDevFn, 0x40, 0x8000, 2); /* enable IDE0 */
            ich9pciConfigWrite(pGlobals, uBus, uDevFn, 0x42, 0x8000, 2); /* enable IDE1 */
            goto default_map;
            break;
        case 0x0300:
            /* VGA controller */
            if (uVendor != 0x80ee)
                goto default_map;
            /* VGA: map frame buffer to default Bochs VBE address */
            ich9pciSetRegionAddress(pGlobals, uBus, uDevFn, 0, 0xE0000000);
            /*
             * Legacy VGA I/O ports are implicitly decoded by a VGA class device. But
             * only the framebuffer (i.e., a memory region) is explicitly registered via
             * ich9pciSetRegionAddress, so I/O decoding must be enabled manually.
             */
            uCmd = ich9pciConfigRead(pGlobals, uBus, uDevFn, VBOX_PCI_COMMAND, 1);
            ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_COMMAND,
                               /* Enable I/O space access. */
                               uCmd | PCI_COMMAND_IOACCESS,
                               1);
            break;
       case 0x0604:
            /* PCI-to-PCI bridge. */
            ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_PRIMARY_BUS, uBus, 1);

            AssertMsg(pGlobals->uBus < 255, ("Too many bridges on the bus\n"));
            pGlobals->uBus++;
            ich9pciBiosInitBridge(pGlobals, uBus, uDevFn, cBridgeDepth, paBridgePositions);
            break;
        default:
        default_map:
        {
            /* default memory mappings */
            /*
             * We ignore ROM region here.
             */
            for (int iRegion = 0; iRegion < (PCI_NUM_REGIONS-1); iRegion++)
            {
                uint32_t u32Address = ich9pciGetRegionReg(iRegion);

                /* Calculate size - we write all 1s into the BAR, and then evaluate which bits
                   are cleared. . */
                uint8_t u8ResourceType = ich9pciConfigRead(pGlobals, uBus, uDevFn, u32Address, 1);
                ich9pciConfigWrite(pGlobals, uBus, uDevFn, u32Address, UINT32_C(0xffffffff), 4);
                uint32_t u32Size = ich9pciConfigRead(pGlobals, uBus, uDevFn, u32Address, 4);
                /* Clear resource information depending on resource type. */
                if ((u8ResourceType & PCI_COMMAND_IOACCESS) == PCI_COMMAND_IOACCESS) /* I/O */
                    u32Size &= ~(0x01);
                else                        /* MMIO */
                    u32Size &= ~(0x0f);

                bool fIsPio = ((u8ResourceType & PCI_COMMAND_IOACCESS) == PCI_COMMAND_IOACCESS);
                /*
                 * Invert all bits and add 1 to get size of the region.
                 * (From PCI implementation note)
                 */
                if (fIsPio && (u32Size & UINT32_C(0xffff0000)) == 0)
                    u32Size = (~(u32Size | UINT32_C(0xffff0000))) + 1;
                else
                    u32Size = (~u32Size) + 1;

                Log(("%s: Size of region %u for device %d on bus %d is %u\n", __FUNCTION__, iRegion, uDevFn, uBus, u32Size));

                if (u32Size)
                {
                    paddr = fIsPio ? &pGlobals->uPciBiosIo : &pGlobals->uPciBiosMmio;
                    *paddr = (*paddr + u32Size - 1) & ~(u32Size - 1);
                    Log(("%s: Start address of %s region %u is %#x\n", __FUNCTION__, (fIsPio ? "I/O" : "MMIO"), iRegion, *paddr));
                    ich9pciSetRegionAddress(pGlobals, uBus, uDevFn, iRegion, *paddr);
                    *paddr += u32Size;
                    Log(("%s: New address is %#x\n", __FUNCTION__, *paddr));
                }
            }
            break;
        }
    }
}

static DECLCALLBACK(int) ich9pciFakePCIBIOS(PPDMDEVINS pDevIns)
{
    unsigned    i;
    uint8_t     elcr[2] = {0, 0};
    PPCIGLOBALS pGlobals = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
    PVM         pVM = PDMDevHlpGetVM(pDevIns);
    Assert(pVM);

    /*
     * Set the start addresses.
     */
    pGlobals->uPciBiosIo  = 0xd000;
    pGlobals->uPciBiosMmio = UINT32_C(0xf0000000);
    pGlobals->uBus = 0;

    /*
     * Init the devices.
     */
    for (i = 0; i < 256; i++)
    {
        uint8_t aBridgePositions[256];

        memset(aBridgePositions, 0, sizeof(aBridgePositions));
        Log2(("PCI: Initializing device %d (%#x)\n",
              i, 0x80000000 | (i << 8)));
        ich9pciBiosInitDevice(pGlobals, 0, i, 0, aBridgePositions);
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(uint32_t) ich9pciConfigReadDev(PCIDevice *aDev, uint32_t u32Address, unsigned len)
{
    if ((u32Address + len) > 256 && (u32Address + len) < 4096)
    {
        AssertMsgReturn(false, ("Read from extended registers falled back to generic code\n"), 0);
    }

    if (   PCIIsMsiCapable(aDev)
        && (u32Address >= aDev->Int.s.u8MsiCapOffset)
        && (u32Address <  aDev->Int.s.u8MsiCapOffset + aDev->Int.s.u8MsiCapSize)
       )
    {
        return MsiPciConfigRead(aDev->Int.s.CTX_SUFF(pBus)->CTX_SUFF(pDevIns), aDev, u32Address, len);
    }

    if (   PCIIsMsixCapable(aDev)
        && (u32Address >= aDev->Int.s.u8MsixCapOffset)
        && (u32Address <  aDev->Int.s.u8MsixCapOffset + aDev->Int.s.u8MsixCapSize)
       )
    {
        return MsixPciConfigRead(aDev->Int.s.CTX_SUFF(pBus)->CTX_SUFF(pDevIns), aDev, u32Address, len);
    }

    AssertMsgReturn(u32Address + len <= 256, ("Read after end of PCI config space\n"),
                    0);
    switch (len)
    {
        case 1:
            return PCIDevGetByte(aDev,  u32Address);
        case 2:
            return PCIDevGetWord(aDev,  u32Address);
        case 4:
            return PCIDevGetDWord(aDev, u32Address);
        default:
            Assert(false);
            return 0;
    }
}

DECLINLINE(void) ich9pciWriteBarByte(PCIDevice *aDev, int iRegion, int iOffset, uint8_t u8Val)
{
    PCIIORegion * pRegion = &aDev->Int.s.aIORegions[iRegion];

    int iRegionSize = pRegion->size;

    Log3(("ich9pciWriteBarByte: region=%d off=%d val=%x size=%d\n",
         iRegion, iOffset, u8Val, iRegionSize));

    /* Region doesn't exist */
    if (iRegionSize == 0)
        return;

    uint32_t uAddr = ich9pciGetRegionReg(iRegion) + iOffset;
    /* Region size must be power of two */
    Assert((iRegionSize & (iRegionSize - 1)) == 0);
    uint8_t uMask = (((uint32_t)iRegionSize - 1) >> (iOffset*8) ) & 0xff;

    if (iOffset == 0)
    {
        uMask |= (pRegion->type & PCI_ADDRESS_SPACE_IO) ?
                (1 << 2) - 1 /* 2 lowest bits for IO region */ :
                (1 << 4) - 1 /* 4 lowest bits for memory region, also ROM enable bit for ROM region */;

    }

    uint8_t u8Old = PCIDevGetByte(aDev, uAddr) & uMask;
    u8Val = (u8Old & uMask) | (u8Val & ~uMask);

    Log3(("ich9pciWriteBarByte: was %x writing %x\n", u8Old, u8Val));

    PCIDevSetByte(aDev, uAddr, u8Val);
}
/**
 * See paragraph 7.5 of PCI Express specification (p. 349) for definition of
 * registers and their writability policy.
 */
static DECLCALLBACK(void) ich9pciConfigWriteDev(PCIDevice *aDev, uint32_t u32Address,
                                                uint32_t val, unsigned len)
{
    Assert(len <= 4);

    if ((u32Address + len) > 256 && (u32Address + len) < 4096)
    {
        AssertMsgReturnVoid(false, ("Write to extended registers falled back to generic code\n"));
    }

    AssertMsgReturnVoid(u32Address + len <= 256, ("Write after end of PCI config space\n"));

    if (   PCIIsMsiCapable(aDev)
        && (u32Address >= aDev->Int.s.u8MsiCapOffset)
        && (u32Address <  aDev->Int.s.u8MsiCapOffset + aDev->Int.s.u8MsiCapSize)
       )
    {
        MsiPciConfigWrite(aDev->Int.s.CTX_SUFF(pBus)->CTX_SUFF(pDevIns),
                          aDev->Int.s.CTX_SUFF(pBus)->CTX_SUFF(pPciHlp),
                          aDev, u32Address, val, len);
        return;
    }

    if (   PCIIsMsixCapable(aDev)
        && (u32Address >= aDev->Int.s.u8MsixCapOffset)
        && (u32Address <  aDev->Int.s.u8MsixCapOffset + aDev->Int.s.u8MsixCapSize)
       )
    {
        MsixPciConfigWrite(aDev->Int.s.CTX_SUFF(pBus)->CTX_SUFF(pDevIns),
                           aDev->Int.s.CTX_SUFF(pBus)->CTX_SUFF(pPciHlp),
                           aDev, u32Address, val, len);
        return;
    }

    uint32_t addr = u32Address;
    bool fUpdateMappings = false;
    bool fP2PBridge = false;
    for (uint32_t i = 0; i < len; i++)
    {
        bool fWritable = false;
        bool fRom = false;
        switch (PCIDevGetHeaderType(aDev))
        {
            case 0x00: /* normal device */
            case 0x80: /* multi-function device */
                switch (addr)
                {
                    /* Read-only registers  */
                    case VBOX_PCI_VENDOR_ID: case VBOX_PCI_VENDOR_ID+1:
                    case VBOX_PCI_DEVICE_ID: case VBOX_PCI_DEVICE_ID+1:
                    case VBOX_PCI_REVISION_ID:
                    case VBOX_PCI_CLASS_PROG:
                    case VBOX_PCI_CLASS_SUB:
                    case VBOX_PCI_CLASS_BASE:
                    case VBOX_PCI_HEADER_TYPE:
                    case VBOX_PCI_SUBSYSTEM_VENDOR_ID: case VBOX_PCI_SUBSYSTEM_VENDOR_ID+1:
                    case VBOX_PCI_SUBSYSTEM_ID: case VBOX_PCI_SUBSYSTEM_ID+1:
                    case VBOX_PCI_ROM_ADDRESS: case VBOX_PCI_ROM_ADDRESS+1: case VBOX_PCI_ROM_ADDRESS+2: case VBOX_PCI_ROM_ADDRESS+3:
                    case VBOX_PCI_CAPABILITY_LIST:
                    case VBOX_PCI_INTERRUPT_PIN:
                        fWritable = false;
                        break;
                    /* Others can be written */
                    default:
                        fWritable = true;
                        break;
                }
                break;
            case 0x01: /* PCI-PCI bridge */
                fP2PBridge = true;
                switch (addr)
                {
                    /* Read-only registers */
                    case VBOX_PCI_VENDOR_ID: case VBOX_PCI_VENDOR_ID+1:
                    case VBOX_PCI_DEVICE_ID: case VBOX_PCI_DEVICE_ID+1:
                    case VBOX_PCI_REVISION_ID:
                    case VBOX_PCI_CLASS_PROG:
                    case VBOX_PCI_CLASS_SUB:
                    case VBOX_PCI_CLASS_BASE:
                    case VBOX_PCI_HEADER_TYPE:
                    case VBOX_PCI_ROM_ADDRESS_BR: case VBOX_PCI_ROM_ADDRESS_BR+1: case VBOX_PCI_ROM_ADDRESS_BR+2: case VBOX_PCI_ROM_ADDRESS_BR+3:
                    case VBOX_PCI_INTERRUPT_PIN:
                        fWritable = false;
                        break;
                    default:
                        fWritable = true;
                        break;
                }
                break;
            default:
                AssertMsgFailed(("Unknown header type %x\n", PCIDevGetHeaderType(aDev)));
                fWritable = false;
                break;
        }

        uint8_t u8Val = (uint8_t)val;
        switch (addr)
        {
            case VBOX_PCI_COMMAND: /* Command register, bits 0-7. */
                fUpdateMappings = true;
                PCIDevSetByte(aDev, addr, u8Val);
                break;
            case VBOX_PCI_COMMAND+1: /* Command register, bits 8-15. */
                /* don't change reserved bits (11-15) */
                u8Val &= UINT32_C(~0xf8);
                fUpdateMappings = true;
                PCIDevSetByte(aDev, addr, u8Val);
                break;
            case VBOX_PCI_STATUS:  /* Status register, bits 0-7. */
                /* don't change read-only bits => actually all lower bits are read-only */
                u8Val &= UINT32_C(~0xff);
                /* status register, low part: clear bits by writing a '1' to the corresponding bit */
                aDev->config[addr] &= ~u8Val;
                break;
            case VBOX_PCI_STATUS+1:  /* Status register, bits 8-15. */
                /* don't change read-only bits */
                u8Val &= UINT32_C(~0x06);
                /* status register, high part: clear bits by writing a '1' to the corresponding bit */
                aDev->config[addr] &= ~u8Val;
                break;
            case VBOX_PCI_ROM_ADDRESS:    case VBOX_PCI_ROM_ADDRESS   +1: case VBOX_PCI_ROM_ADDRESS   +2: case VBOX_PCI_ROM_ADDRESS   +3:
                fRom = true;
            case VBOX_PCI_BASE_ADDRESS_0: case VBOX_PCI_BASE_ADDRESS_0+1: case VBOX_PCI_BASE_ADDRESS_0+2: case VBOX_PCI_BASE_ADDRESS_0+3:
            case VBOX_PCI_BASE_ADDRESS_1: case VBOX_PCI_BASE_ADDRESS_1+1: case VBOX_PCI_BASE_ADDRESS_1+2: case VBOX_PCI_BASE_ADDRESS_1+3:
            case VBOX_PCI_BASE_ADDRESS_2: case VBOX_PCI_BASE_ADDRESS_2+1: case VBOX_PCI_BASE_ADDRESS_2+2: case VBOX_PCI_BASE_ADDRESS_2+3:
            case VBOX_PCI_BASE_ADDRESS_3: case VBOX_PCI_BASE_ADDRESS_3+1: case VBOX_PCI_BASE_ADDRESS_3+2: case VBOX_PCI_BASE_ADDRESS_3+3:
            case VBOX_PCI_BASE_ADDRESS_4: case VBOX_PCI_BASE_ADDRESS_4+1: case VBOX_PCI_BASE_ADDRESS_4+2: case VBOX_PCI_BASE_ADDRESS_4+3:
            case VBOX_PCI_BASE_ADDRESS_5: case VBOX_PCI_BASE_ADDRESS_5+1: case VBOX_PCI_BASE_ADDRESS_5+2: case VBOX_PCI_BASE_ADDRESS_5+3:
            {
                /* We check that, as same PCI register numbers as BARs may mean different registers for bridges */
                if (fP2PBridge)
                    goto default_case;
                else
                {
                    int iRegion = fRom ? VBOX_PCI_ROM_SLOT : (addr - VBOX_PCI_BASE_ADDRESS_0) >> 2;
                    int iOffset = addr & 0x3;
                    ich9pciWriteBarByte(aDev, iRegion, iOffset, u8Val);
                    fUpdateMappings = true;
                }
                break;
            }
            default:
            default_case:
                if (fWritable)
                    PCIDevSetByte(aDev, addr, u8Val);
        }
        addr++;
        val >>= 8;
    }

    if (fUpdateMappings)
        /* if the command/base address register is modified, we must modify the mappings */
        ich9pciUpdateMappings(aDev);
}

/* Slot/functions assignment per table at p. 12 of ICH9 family spec update */
static const struct {
    const char* pszName;
    int32_t     iSlot;
    int32_t     iFunction;
} PciSlotAssignments[] = {
    /* The only override that have to be here, as host controller is added in the way invisible to bus slot assignment management,
       maybe to be changed in the future. */
    {
        "i82801",   30, 0 /* Host Controller */
    },
};

static bool assignPosition(PPCIBUS pBus, PPCIDEVICE pPciDev, const char *pszName, int iDevFn, PciAddress* aPosition)
{
    aPosition->iBus = 0;
    aPosition->iDeviceFunc = iDevFn;
    aPosition->iRegister = 0; /* N/A */

    /* Hardcoded slots/functions, per chipset spec */
    for (size_t i = 0; i < RT_ELEMENTS(PciSlotAssignments); i++)
    {
        if (!strcmp(pszName, PciSlotAssignments[i].pszName))
        {
            PCISetRequestedDevfunc(pPciDev);
            aPosition->iDeviceFunc =
                    (PciSlotAssignments[i].iSlot << 3) + PciSlotAssignments[i].iFunction;
            return true;
        }
    }

    /* Explicit slot request */
    if (iDevFn >=0 && iDevFn < (int)RT_ELEMENTS(pBus->apDevices))
        return true;

    int iStartPos = 0;

    /* Otherwise when assigning a slot, we need to make sure all its functions are available */
    for (int iPos = iStartPos; iPos < (int)RT_ELEMENTS(pBus->apDevices); iPos += 8)
    {
        if (        !pBus->apDevices[iPos]
                &&  !pBus->apDevices[iPos + 1]
                &&  !pBus->apDevices[iPos + 2]
                &&  !pBus->apDevices[iPos + 3]
                &&  !pBus->apDevices[iPos + 4]
                &&  !pBus->apDevices[iPos + 5]
                &&  !pBus->apDevices[iPos + 6]
                &&  !pBus->apDevices[iPos + 7])
        {
            PCIClearRequestedDevfunc(pPciDev);
            aPosition->iDeviceFunc = iPos;
            return true;
        }
    }

    return false;
}

static bool hasHardAssignedDevsInSlot(PPCIBUS pBus, int iSlot)
{
    PCIDevice** aSlot = &pBus->apDevices[iSlot << 3];

    return     (aSlot[0] && PCIIsRequestedDevfunc(aSlot[0]))
            || (aSlot[1] && PCIIsRequestedDevfunc(aSlot[1]))
            || (aSlot[2] && PCIIsRequestedDevfunc(aSlot[2]))
            || (aSlot[3] && PCIIsRequestedDevfunc(aSlot[3]))
            || (aSlot[4] && PCIIsRequestedDevfunc(aSlot[4]))
            || (aSlot[5] && PCIIsRequestedDevfunc(aSlot[5]))
            || (aSlot[6] && PCIIsRequestedDevfunc(aSlot[6]))
            || (aSlot[7] && PCIIsRequestedDevfunc(aSlot[7]))
           ;
}

static int ich9pciRegisterInternal(PPCIBUS pBus, int iDev, PPCIDEVICE pPciDev, const char *pszName)
{
    PciAddress aPosition = {0, 0, 0};

    /*
     * Find device position
     */
    if (!assignPosition(pBus, pPciDev, pszName, iDev, &aPosition))
    {
        AssertMsgFailed(("Couldn't asssign position!\n"));
        return VERR_PDM_TOO_PCI_MANY_DEVICES;
    }

    AssertMsgReturn(aPosition.iBus == 0,
                    ("Assigning behind the bridge not implemented yet\n"),
                    VERR_PDM_TOO_PCI_MANY_DEVICES);


    iDev = aPosition.iDeviceFunc;
    /*
     * Check if we can really take this slot, possibly by relocating
     * its current habitant, if it wasn't hard assigned too.
     */
    if (PCIIsRequestedDevfunc(pPciDev) &&
        pBus->apDevices[iDev]          &&
        PCIIsRequestedDevfunc(pBus->apDevices[iDev]))
    {
        AssertReleaseMsgFailed(("Configuration error:'%s' and '%s' are both configured as device %d\n",
                                 pszName, pBus->apDevices[iDev]->name, iDev));
        return VERR_INTERNAL_ERROR;
    }

    if (pBus->apDevices[iDev])
    {
        /* if we got here, we shall (and usually can) relocate the device */
        bool assigned = assignPosition(pBus, pBus->apDevices[iDev], pBus->apDevices[iDev]->name, -1, &aPosition);
        AssertMsgReturn(aPosition.iBus == 0,
                        ("Assigning behind the bridge not implemented yet\n"),
                        VERR_PDM_TOO_PCI_MANY_DEVICES);
        int iRelDev = aPosition.iDeviceFunc;
        if (!assigned || iRelDev == iDev)
        {
            AssertMsgFailed(("Couldn't find free spot!\n"));
            return VERR_PDM_TOO_PCI_MANY_DEVICES;
        }
        /* Copy device function by function to its new position */
        for (int i = 0; i < 8; i++)
        {
            if (!pBus->apDevices[iDev + i])
                continue;
            Log(("PCI: relocating '%s' from slot %#x to %#x\n", pBus->apDevices[iDev + i]->name, iDev + i, iRelDev + i));
            pBus->apDevices[iRelDev + i] = pBus->apDevices[iDev + i];
            pBus->apDevices[iRelDev + i]->devfn = iRelDev + i;
            pBus->apDevices[iDev + i] = NULL;
        }
    }

    /*
     * Fill in device information.
     */
    pPciDev->devfn                  = iDev;
    pPciDev->name                   = pszName;
    pPciDev->Int.s.pBusR3           = pBus;
    pPciDev->Int.s.pBusR0           = MMHyperR3ToR0(PDMDevHlpGetVM(pBus->CTX_SUFF(pDevIns)), pBus);
    pPciDev->Int.s.pBusRC           = MMHyperR3ToRC(PDMDevHlpGetVM(pBus->CTX_SUFF(pDevIns)), pBus);
    pPciDev->Int.s.pfnConfigRead    = ich9pciConfigReadDev;
    pPciDev->Int.s.pfnConfigWrite   = ich9pciConfigWriteDev;
    pBus->apDevices[iDev]           = pPciDev;
    if (PCIIsPci2PciBridge(pPciDev))
    {
        AssertMsg(pBus->cBridges < RT_ELEMENTS(pBus->apDevices), ("Number of bridges exceeds the number of possible devices on the bus\n"));
        AssertMsg(pPciDev->Int.s.pfnBridgeConfigRead && pPciDev->Int.s.pfnBridgeConfigWrite,
                  ("device is a bridge but does not implement read/write functions\n"));
        pBus->papBridgesR3[pBus->cBridges] = pPciDev;
        pBus->cBridges++;
    }

    Log(("PCI: Registered device %d function %d (%#x) '%s'.\n",
         iDev >> 3, iDev & 7, 0x80000000 | (iDev << 8), pszName));

    return VINF_SUCCESS;
}

static void printIndent(PCDBGFINFOHLP pHlp, int iIndent)
{
    for (int i = 0; i < iIndent; i++)
    {
        pHlp->pfnPrintf(pHlp, "    ");
    }
}

static void ich9pciBusInfo(PPCIBUS pBus, PCDBGFINFOHLP pHlp, int iIndent)
{
    for (uint32_t iDev = 0; iDev < RT_ELEMENTS(pBus->apDevices); iDev++)
    {
        PPCIDEVICE pPciDev = pBus->apDevices[iDev];
        if (pPciDev != NULL)
        {
            printIndent(pHlp, iIndent);
            pHlp->pfnPrintf(pHlp, "%02x:%02x:%02x %s: %04x-%04x%s%s\n",
                            pBus->iBus, (iDev >> 3) & 0xff, iDev & 0x7,
                            pPciDev->name,
                            PCIDevGetVendorId(pPciDev), PCIDevGetDeviceId(pPciDev),
                            PCIIsMsiCapable(pPciDev)  ? " MSI" : "",
                            PCIIsMsixCapable(pPciDev) ? " MSI-X" : ""
                            );
        }
    }

    if (pBus->cBridges > 0)
    {
        printIndent(pHlp, iIndent);
        pHlp->pfnPrintf(pHlp, "Registered %d bridges, subordinate buses info follows\n", pBus->cBridges);
        for (uint32_t iBridge = 0; iBridge < pBus->cBridges; iBridge++)
        {
            PPCIBUS pBusSub = PDMINS_2_DATA(pBus->papBridgesR3[iBridge]->pDevIns, PPCIBUS);
            ich9pciBusInfo(pBusSub, pHlp, iIndent + 1);
        }
    }
}

/**
 * Info handler, device version.
 *
 * @param   pDevIns     Device instance which registered the info.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
static DECLCALLBACK(void) ich9pciInfo(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PPCIBUS pBus = DEVINS_2_PCIBUS(pDevIns);

    ich9pciBusInfo(pBus, pHlp, 0);
}


static DECLCALLBACK(int) ich9pciConstruct(PPDMDEVINS pDevIns,
                                          int        iInstance,
                                          PCFGMNODE  pCfg)
{
    int rc;
    Assert(iInstance == 0);

    /*
     * Validate and read configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg,
                              "IOAPIC\0"
                              "GCEnabled\0"
                              "R0Enabled\0"
                              "McfgBase\0"
                              "McfgLength\0"
                              ))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    /* query whether we got an IOAPIC */
    bool fUseIoApic;
    rc = CFGMR3QueryBoolDef(pCfg, "IOAPIC", &fUseIoApic, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query boolean value \"IOAPIC\""));

    /* check if RC code is enabled. */
    bool fGCEnabled;
    rc = CFGMR3QueryBoolDef(pCfg, "GCEnabled", &fGCEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query boolean value \"GCEnabled\""));

    /* check if R0 code is enabled. */
    bool fR0Enabled;
    rc = CFGMR3QueryBoolDef(pCfg, "R0Enabled", &fR0Enabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query boolean value \"R0Enabled\""));

    Log(("PCI: fUseIoApic=%RTbool fGCEnabled=%RTbool fR0Enabled=%RTbool\n", fUseIoApic, fGCEnabled, fR0Enabled));

    /*
     * Init data.
     */
    PPCIGLOBALS pGlobals = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
    PPCIBUS     pBus     = &pGlobals->aPciBus;
    /* Zero out everything */
    memset(pGlobals, 0, sizeof(*pGlobals));
    /* And fill values */
    if (!fUseIoApic)
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Must use IO-APIC with ICH9 chipset"));
    rc = CFGMR3QueryU64Def(pCfg, "McfgBase", &pGlobals->u64PciConfigMMioAddress, 0);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to read \"McfgBase\""));
    rc = CFGMR3QueryU64Def(pCfg, "McfgLength", &pGlobals->u64PciConfigMMioLength, 0);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to read \"McfgLength\""));

    pGlobals->pDevInsR3 = pDevIns;
    pGlobals->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pGlobals->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);

    pGlobals->aPciBus.pDevInsR3 = pDevIns;
    pGlobals->aPciBus.pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pGlobals->aPciBus.pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    pGlobals->aPciBus.papBridgesR3 = (PPCIDEVICE *)PDMDevHlpMMHeapAllocZ(pDevIns, sizeof(PPCIDEVICE) * RT_ELEMENTS(pGlobals->aPciBus.apDevices));

    /*
     * Register bus
     */
    PDMPCIBUSREG PciBusReg;
    PciBusReg.u32Version              = PDM_PCIBUSREG_VERSION;
    PciBusReg.pfnRegisterR3           = ich9pciRegister;
    PciBusReg.pfnRegisterMsiR3        = ich9pciRegisterMsi;
    PciBusReg.pfnIORegionRegisterR3   = ich9pciIORegionRegister;
    PciBusReg.pfnSetConfigCallbacksR3 = ich9pciSetConfigCallbacks;
    PciBusReg.pfnSetIrqR3             = ich9pciSetIrq;
    PciBusReg.pfnSaveExecR3           = ich9pciGenericSaveExec;
    PciBusReg.pfnLoadExecR3           = ich9pciGenericLoadExec;
    PciBusReg.pfnFakePCIBIOSR3        = ich9pciFakePCIBIOS;
    PciBusReg.pszSetIrqRC             = fGCEnabled ? "ich9pciSetIrq" : NULL;
    PciBusReg.pszSetIrqR0             = fR0Enabled ? "ich9pciSetIrq" : NULL;
    rc = PDMDevHlpPCIBusRegister(pDevIns, &PciBusReg, &pBus->pPciHlpR3);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Failed to register ourselves as a PCI Bus"));
    if (pBus->pPciHlpR3->u32Version != PDM_PCIHLPR3_VERSION)
        return PDMDevHlpVMSetError(pDevIns, VERR_VERSION_MISMATCH, RT_SRC_POS,
                                   N_("PCI helper version mismatch; got %#x expected %#x"),
                                   pBus->pPciHlpR3->u32Version, PDM_PCIHLPR3_VERSION);

    pBus->pPciHlpRC = pBus->pPciHlpR3->pfnGetRCHelpers(pDevIns);
    pBus->pPciHlpR0 = pBus->pPciHlpR3->pfnGetR0Helpers(pDevIns);

    /*
     * Fill in PCI configs and add them to the bus.
     */

    /**
     * We emulate 82801IB ICH9 IO chip used in Q35,
     * see http://ark.intel.com/Product.aspx?id=31892
     *
     * Stepping   S-Spec   Top Marking
     *
     *   A2        SLA9M    NH82801IB
     */
    PCIDevSetVendorId(  &pBus->aPciDev, 0x8086); /* Intel */
    PCIDevSetDeviceId(  &pBus->aPciDev, 0x244e); /* Desktop */
    PCIDevSetRevisionId(&pBus->aPciDev,   0x92); /* rev. A2 */
    PCIDevSetClassSub(  &pBus->aPciDev,   0x00); /* Host/PCI bridge */
    PCIDevSetClassBase( &pBus->aPciDev,   0x06); /* bridge */
    PCIDevSetHeaderType(&pBus->aPciDev,   0x00); /* normal device */

    pBus->aPciDev.pDevIns               = pDevIns;
    /* We register Host<->PCI controller on the bus */
    ich9pciRegisterInternal(pBus, -1, &pBus->aPciDev, "i82801");

    /*
     * Register I/O ports and save state.
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, 0x0cf8, 1, NULL, ich9pciIOPortAddressWrite, ich9pciIOPortAddressRead, NULL, NULL, "ICH9 (PCI)");
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns, 0x0cfc, 4, NULL, ich9pciIOPortDataWrite, ich9pciIOPortDataRead, NULL, NULL, "ICH9 (PCI)");
    if (RT_FAILURE(rc))
        return rc;
    if (fGCEnabled)
    {
        rc = PDMDevHlpIOPortRegisterRC(pDevIns, 0x0cf8, 1, NIL_RTGCPTR, "ich9pciIOPortAddressWrite", "ich9pciIOPortAddressRead", NULL, NULL, "ICH9 (PCI)");
        if (RT_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterRC(pDevIns, 0x0cfc, 4, NIL_RTGCPTR, "ich9pciIOPortDataWrite", "ich9pciIOPortDataRead", NULL, NULL, "ICH9 (PCI)");
        if (RT_FAILURE(rc))
            return rc;
    }
    if (fR0Enabled)
    {
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, 0x0cf8, 1, NIL_RTR0PTR, "ich9pciIOPortAddressWrite", "ich9pciIOPortAddressRead", NULL, NULL, "ICH9 (PCI)");
        if (RT_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, 0x0cfc, 4, NIL_RTR0PTR, "ich9pciIOPortDataWrite", "ich9pciIOPortDataRead", NULL, NULL, "ICH9 (PCI)");
        if (RT_FAILURE(rc))
            return rc;
    }

    if (pGlobals->u64PciConfigMMioAddress != 0)
    {
        rc = PDMDevHlpMMIORegister(pDevIns,
                                   pGlobals->u64PciConfigMMioAddress,
                                   pGlobals->u64PciConfigMMioLength,
                                   0,
                                   ich9pciMcfgMMIOWrite,
                                   ich9pciMcfgMMIORead,
                                   NULL /* fill */,
                                   "MCFG ranges");
        if (RT_FAILURE(rc))
        {
            AssertMsgRC(rc, ("Cannot register MCFG MMIO: %Rrc\n", rc));
            return rc;
        }

        if (fGCEnabled)
        {

            rc = PDMDevHlpMMIORegisterRC(pDevIns,
                                         pGlobals->u64PciConfigMMioAddress,
                                         pGlobals->u64PciConfigMMioLength,
                                         0,
                                         "ich9pciMcfgMMIOWrite",
                                         "ich9pciMcfgMMIORead",
                                         NULL /* fill */);
            if (RT_FAILURE(rc))
            {
                AssertMsgRC(rc, ("Cannot register MCFG MMIO (GC): %Rrc\n", rc));
                return rc;
            }
        }


        if (fR0Enabled)
        {

            rc = PDMDevHlpMMIORegisterR0(pDevIns,
                                         pGlobals->u64PciConfigMMioAddress,
                                         pGlobals->u64PciConfigMMioLength,
                                         0,
                                         "ich9pciMcfgMMIOWrite",
                                         "ich9pciMcfgMMIORead",
                                         NULL /* fill */);
            if (RT_FAILURE(rc))
            {
                AssertMsgRC(rc, ("Cannot register MCFG MMIO (R0): %Rrc\n", rc));
                return rc;
            }
        }
    }

    rc = PDMDevHlpSSMRegisterEx(pDevIns, VBOX_ICH9PCI_SAVED_STATE_VERSION_CURRENT,
                                sizeof(*pBus) + 16*128, "pgm",
                                NULL, NULL, NULL,
                                NULL, ich9pciR3SaveExec, NULL,
                                NULL, ich9pciR3LoadExec, NULL);
    if (RT_FAILURE(rc))
        return rc;


    /** @todo: other chipset devices shall be registered too */
    /** @todo: what to with bridges? */

    PDMDevHlpDBGFInfoRegister(pDevIns, "pci", "Display PCI bus status. (no arguments)", ich9pciInfo);

    return VINF_SUCCESS;
}

static void ich9pciResetDevice(PPCIDEVICE pDev)
{
    pDev->config[VBOX_PCI_COMMAND] &= ~(VBOX_PCI_COMMAND_IO | VBOX_PCI_COMMAND_MEMORY |
                                        VBOX_PCI_COMMAND_MASTER);

    if (!PCIIsPci2PciBridge(pDev))
    {
        PCIDevSetByte(pDev, VBOX_PCI_CACHE_LINE_SIZE, 0x0);
        PCIDevSetByte(pDev, VBOX_PCI_INTERRUPT_LINE,  0x0);
    }
    /* Regions ? */
}


/**
 * @copydoc FNPDMDEVRESET
 */
static DECLCALLBACK(void) ich9pciReset(PPDMDEVINS pDevIns)
{
    PPCIGLOBALS pGlobals = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
    PPCIBUS     pBus     = &pGlobals->aPciBus;

    /* Relocate RC pointers for the attached pci devices. */
    for (uint32_t i = 0; i < RT_ELEMENTS(pBus->apDevices); i++)
    {
        if (pBus->apDevices[i])
            ich9pciResetDevice(pBus->apDevices[i]);
    }
}

/**
 * @copydoc FNPDMDEVRELOCATE
 */
static DECLCALLBACK(void) ich9pciRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    PPCIGLOBALS pGlobals = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
    PPCIBUS     pBus     = &pGlobals->aPciBus;
    pGlobals->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);

    pBus->pPciHlpRC = pBus->pPciHlpR3->pfnGetRCHelpers(pDevIns);
    pBus->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);

    /* Relocate RC pointers for the attached pci devices. */
    for (uint32_t i = 0; i < RT_ELEMENTS(pBus->apDevices); i++)
    {
        if (pBus->apDevices[i])
        {
            pBus->apDevices[i]->Int.s.pBusRC += offDelta;
            if (pBus->apDevices[i]->Int.s.pMsixPageRC)
                pBus->apDevices[i]->Int.s.pMsixPageRC += offDelta;
        }
    }

}

/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int)   ich9pcibridgeConstruct(PPDMDEVINS pDevIns,
                                                  int        iInstance,
                                                  PCFGMNODE  pCfg)
{
    int rc;

    /*
     * Validate and read configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, "GCEnabled\0" "R0Enabled\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    /* check if RC code is enabled. */
    bool fGCEnabled;
    rc = CFGMR3QueryBoolDef(pCfg, "GCEnabled", &fGCEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query boolean value \"GCEnabled\""));

    /* check if R0 code is enabled. */
    bool fR0Enabled;
    rc = CFGMR3QueryBoolDef(pCfg, "R0Enabled", &fR0Enabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query boolean value \"R0Enabled\""));
    Log(("PCI: fGCEnabled=%RTbool fR0Enabled=%RTbool\n", fGCEnabled, fR0Enabled));

    /*
     * Init data and register the PCI bus.
     */
    PPCIBUS pBus = PDMINS_2_DATA(pDevIns, PPCIBUS);
    pBus->pDevInsR3 = pDevIns;
    pBus->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pBus->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    pBus->papBridgesR3 = (PPCIDEVICE *)PDMDevHlpMMHeapAllocZ(pDevIns, sizeof(PPCIDEVICE) * RT_ELEMENTS(pBus->apDevices));

    PDMPCIBUSREG PciBusReg;
    PciBusReg.u32Version              = PDM_PCIBUSREG_VERSION;
    PciBusReg.pfnRegisterR3           = ich9pcibridgeRegister;
    PciBusReg.pfnRegisterMsiR3        = ich9pciRegisterMsi;
    PciBusReg.pfnIORegionRegisterR3   = ich9pciIORegionRegister;
    PciBusReg.pfnSetConfigCallbacksR3 = ich9pciSetConfigCallbacks;
    PciBusReg.pfnSetIrqR3             = ich9pcibridgeSetIrq;
    PciBusReg.pfnSaveExecR3           = ich9pciGenericSaveExec;
    PciBusReg.pfnLoadExecR3           = ich9pciGenericLoadExec;
    PciBusReg.pfnFakePCIBIOSR3        = NULL; /* Only needed for the first bus. */
    PciBusReg.pszSetIrqRC             = fGCEnabled ? "ich9pcibridgeSetIrq" : NULL;
    PciBusReg.pszSetIrqR0             = fR0Enabled ? "ich9pcibridgeSetIrq" : NULL;
    rc = PDMDevHlpPCIBusRegister(pDevIns, &PciBusReg, &pBus->pPciHlpR3);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Failed to register ourselves as a PCI Bus"));
    if (pBus->pPciHlpR3->u32Version != PDM_PCIHLPR3_VERSION)
        return PDMDevHlpVMSetError(pDevIns, VERR_VERSION_MISMATCH, RT_SRC_POS,
                                   N_("PCI helper version mismatch; got %#x expected %#x"),
                                   pBus->pPciHlpR3->u32Version, PDM_PCIHLPR3_VERSION);

    pBus->pPciHlpRC = pBus->pPciHlpR3->pfnGetRCHelpers(pDevIns);
    pBus->pPciHlpR0 = pBus->pPciHlpR3->pfnGetR0Helpers(pDevIns);

    /*
     * Fill in PCI configs and add them to the bus.
     */
    PCIDevSetVendorId(  &pBus->aPciDev, 0x8086); /* Intel */
    PCIDevSetDeviceId(  &pBus->aPciDev, 0x2448); /* 82801 Mobile PCI bridge. */
    PCIDevSetRevisionId(&pBus->aPciDev,   0xf2);
    PCIDevSetClassSub(  &pBus->aPciDev,   0x04); /* pci2pci */
    PCIDevSetClassBase( &pBus->aPciDev,   0x06); /* PCI_bridge */
    PCIDevSetClassProg( &pBus->aPciDev,   0x01); /* Supports subtractive decoding. */
    PCIDevSetHeaderType(&pBus->aPciDev,   0x01); /* Single function device which adheres to the PCI-to-PCI bridge spec. */
    PCIDevSetCommand(   &pBus->aPciDev,   0x00);
    PCIDevSetStatus(    &pBus->aPciDev,   0x20); /* 66MHz Capable. */
    PCIDevSetInterruptLine(&pBus->aPciDev, 0x00); /* This device does not assert interrupts. */

    /*
     * This device does not generate interrupts. Interrupt delivery from
     * devices attached to the bus is unaffected.
     */
    PCIDevSetInterruptPin (&pBus->aPciDev, 0x00);

    pBus->aPciDev.pDevIns                    = pDevIns;

    /* Bridge-specific data */
    PCISetPci2PciBridge(&pBus->aPciDev);
    pBus->aPciDev.Int.s.pfnBridgeConfigRead  = ich9pcibridgeConfigRead;
    pBus->aPciDev.Int.s.pfnBridgeConfigWrite = ich9pcibridgeConfigWrite;

    /*
     * Register this PCI bridge. The called function will take care on which bus we will get registered.
     */
    rc = PDMDevHlpPCIRegister (pDevIns, &pBus->aPciDev);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * The iBus property doesn't really represent the bus number
     * because the guest and the BIOS can choose different bus numbers
     * for them.
     * The bus number is mainly for the setIrq function to indicate
     * when the host bus is reached which will have iBus = 0.
     * That's why the + 1.
     */
    pBus->iBus = iInstance + 1;

    /*
     * Register SSM handlers. We use the same saved state version as for the host bridge
     * to make changes easier.
     */
    rc = PDMDevHlpSSMRegisterEx(pDevIns, VBOX_ICH9PCI_SAVED_STATE_VERSION_CURRENT,
                                sizeof(*pBus) + 16*128,
                                "pgm" /* before */,
                                NULL, NULL, NULL,
                                NULL, ich9pcibridgeR3SaveExec, NULL,
                                NULL, ich9pcibridgeR3LoadExec, NULL);
    if (RT_FAILURE(rc))
        return rc;


    return VINF_SUCCESS;
}

/**
 * @copydoc FNPDMDEVRESET
 */
static DECLCALLBACK(void) ich9pcibridgeReset(PPDMDEVINS pDevIns)
{
    PPCIBUS pBus = PDMINS_2_DATA(pDevIns, PPCIBUS);

    /* Reset config space to default values. */
    PCIDevSetByte(&pBus->aPciDev, VBOX_PCI_PRIMARY_BUS, 0);
    PCIDevSetByte(&pBus->aPciDev, VBOX_PCI_SECONDARY_BUS, 0);
    PCIDevSetByte(&pBus->aPciDev, VBOX_PCI_SUBORDINATE_BUS, 0);
}


/**
 * @copydoc FNPDMDEVRELOCATE
 */
static DECLCALLBACK(void) ich9pcibridgeRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    PPCIBUS pBus = PDMINS_2_DATA(pDevIns, PPCIBUS);
    pBus->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);

    /* Relocate RC pointers for the attached pci devices. */
    for (uint32_t i = 0; i < RT_ELEMENTS(pBus->apDevices); i++)
    {
        if (pBus->apDevices[i])
            pBus->apDevices[i]->Int.s.pBusRC += offDelta;
    }

}

/**
 * The PCI bus device registration structure.
 */
const PDMDEVREG g_DevicePciIch9 =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "ich9pci",
    /* szRCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "ICH9 PCI bridge",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_BUS_PCI | PDM_DEVREG_CLASS_BUS_ISA,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(PCIGLOBALS),
    /* pfnConstruct */
    ich9pciConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    ich9pciRelocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    ich9pciReset,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnQueryInterface */
    NULL,
    /* pfnInitComplete */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};

/**
 * The device registration structure
 * for the PCI-to-PCI bridge.
 */
const PDMDEVREG g_DevicePciIch9Bridge =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "ich9pcibridge",
    /* szRCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "ICH9 PCI to PCI bridge",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_BUS_PCI,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(PCIBUS),
    /* pfnConstruct */
    ich9pcibridgeConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    ich9pcibridgeRelocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    ich9pcibridgeReset,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnQueryInterface */
    NULL,
    /* pfnInitComplete */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};

#endif /* IN_RING3 */
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */
