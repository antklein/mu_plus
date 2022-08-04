/**
  A library for overriding the memory bins for FHR.

  Copyright (c) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryBinOverrideLib.h>

#include <Library/FhrLib.h>
#include <Fhr.h>

STATIC BOOLEAN      mFhrInformationInitialized = FALSE;
STATIC BOOLEAN      mIsFhrResume               = FALSE;
STATIC FHR_FW_DATA  *mFhrData                  = NULL;

VOID
InitializeFhrInformation (
  VOID
  )
{
  EFI_HOB_GUID_TYPE  *GuidHob;
  FHR_HOB            *FhrHob;

  //
  // Check if this is an FHR resume.
  //

  GuidHob = GetFirstGuidHob (&gFhrHobGuid);
  if (GuidHob != NULL) {
    FhrHob                     = (FHR_HOB *)GET_GUID_HOB_DATA (GuidHob);
    mIsFhrResume               = FhrHob->IsFhrBoot;
    mFhrData                   = (FHR_FW_DATA *)FhrHob->FhrReservedBase;
    mFhrInformationInitialized = TRUE;
  } else {
    DEBUG ((DEBUG_ERROR, "[FHR] Failed to find FHR hob in MemoryBinOverrideLib.\n"));
  }
}

VOID
EFIAPI
ReportRuntimeMemoryBinLocation (
  IN EFI_MEMORY_TYPE       Type,
  IN EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN UINT64                NumberOfPages
  )
{
  if (!FHR_IS_RUNTIME_MEMORY (Type)) {
    return;
  }

  if (!mFhrInformationInitialized) {
    InitializeFhrInformation ();
  }

  if (mFhrData == NULL) {
    return;
  }

  //
  // No need to save the range if this is an FHR, just log it.
  //

  if (mIsFhrResume) {
    DEBUG ((DEBUG_INFO, "[FHR] Reported memory bin. Base: 0x%llx Pages 0x%llx Type: %d\n", BaseAddress, NumberOfPages, Type));
    return;
  }

  if (mFhrData->MemoryBinCount >= FHR_MAX_MEMORY_BINS) {
    ASSERT (FALSE);
    DEBUG ((DEBUG_ERROR, "[FHR] Not enough memory bins in array!\n", __FUNCTION__));
    return;
  }

  DEBUG ((DEBUG_INFO, "[FHR] Saving memory bin. Base: 0x%llx Pages 0x%llx Type: %d\n", BaseAddress, NumberOfPages, Type));
  mFhrData->MemoryBins[mFhrData->MemoryBinCount].Type          = Type;
  mFhrData->MemoryBins[mFhrData->MemoryBinCount].BaseAddress   = BaseAddress;
  mFhrData->MemoryBins[mFhrData->MemoryBinCount].NumberOfPages = (UINT32)NumberOfPages;
  mFhrData->MemoryBinCount++;
}

VOID
EFIAPI
CheckMemoryBinOverride (
  IN EFI_MEMORY_TYPE        Type,
  OUT EFI_PHYSICAL_ADDRESS  *BaseAddress,
  IN OUT UINT32             *NumberOfPages,
  IN OUT EFI_ALLOCATE_TYPE  *AllocationType
  )
{
  UINT32                Index;
  EFI_PHYSICAL_ADDRESS  MinReserved;

  if (!mFhrInformationInitialized) {
    InitializeFhrInformation ();
  }

  if ((mFhrData == NULL) || !mIsFhrResume) {
    return;
  }

  DEBUG ((DEBUG_INFO, "[FHR] Searching for bin for type %d.\n", Type));
  MinReserved = MAX_UINTN;
  for (Index = 0; Index < mFhrData->MemoryBinCount; Index++) {
    if (mFhrData->MemoryBins[Index].Type == Type) {
      *BaseAddress   = mFhrData->MemoryBins[Index].BaseAddress;
      *NumberOfPages = mFhrData->MemoryBins[Index].NumberOfPages;
      DEBUG ((DEBUG_INFO, "[FHR] Found Base: 0x%llx Pages 0x%lx\n", *BaseAddress, *NumberOfPages));
      *AllocationType = AllocateAddress;
      return;
    } else if (mFhrData->MemoryBins[Index].BaseAddress < MinReserved) {
      MinReserved = mFhrData->MemoryBins[Index].BaseAddress;
    }
  }

  //
  // TODO: Reconsider. this is a bit hacky. makes memory order dependence for FHR region.
  //

  *BaseAddress    = MinReserved - 1;
  *AllocationType = AllocateMaxAddress;
}