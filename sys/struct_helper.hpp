/*
  Dokan : user-mode file system library for Windows

  Copyright (C) 2020 Google, Inc.

  http://dokan-dev.github.io

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free
Software Foundation; either version 3 of the License, or (at your option) any
later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef STRUCT_HELPER_H_
#define STRUCT_HELPER_H_

#include "public.h"
#include <ntifs.h>

// The goal of this defines are to simplify IRP
// DeviceIOControl Buffer usage and safety size check

// Exit type in case of failure
#define DOKAN_EXIT_NONE(Irp, Status, InformationSize)
#define DOKAN_EXIT_LEAVE(Irp, Status, InformationSize)                         \
  Irp->IoStatus.Information = InformationSize;                                 \
  status = Status;                                                             \
  __leave;
#define DOKAN_EXIT_BREAK(Irp, Status, InformationSize)                         \
  Irp->IoStatus.Information = InformationSize;                                 \
  status = Status;                                                             \
  break;
#define DOKAN_EXIT_RETURN(Irp, Status, InformationSize) return Status;

// Generic type calcul size
#define GENERIC_SIZE_COMPARE(Buffer, BufferLen) (sizeof(*Buffer) > BufferLen)

// Main Get DeviceIOControl Buffer from IRP
#define GET_IRP_BUFFER_EX(Irp, Buffer, BufferIOType, SizeCompare, Exit,        \
                          Status, InformationSize)                             \
  {                                                                            \
    ULONG irpBufferLen =                                                       \
        IoGetCurrentIrpStackLocation(Irp)                                      \
            ->Parameters.DeviceIoControl.BufferIOType##BufferLength;           \
    Buffer = Irp->AssociatedIrp.SystemBuffer;                                  \
    ASSERT(Buffer != NULL);                                                    \
    if (SizeCompare(Buffer, irpBufferLen)) {                                   \
      DDbgPrint("  Invalid " #BufferIOType " Buffer length \n");               \
      Buffer = NULL;                                                           \
      Exit(Irp, Status, InformationSize);                                      \
    }                                                                          \
  }
#define GET_IRP_BUFFER(Irp, Buffer, BufferIOType, CompareSize)                 \
  GET_IRP_BUFFER_EX(Irp, Buffer, BufferIOType, CompareSize, DOKAN_EXIT_NONE,   \
                    0, 0)
// Outputbuffer
#define GET_IRP_OUPUTBUFFER_EX(Irp, Buffer, NeededSize, Exit, Status,          \
                               InformationSize)                                \
  {                                                                            \
    ULONG irpBufferLen = IoGetCurrentIrpStackLocation(Irp)                     \
                             ->Parameters.DeviceIoControl.OutputBufferLength;  \
    Buffer = Irp->AssociatedIrp.SystemBuffer;                                  \
    ASSERT(Buffer != NULL);                                                    \
    if (NeededSize > irpBufferLen) {                                           \
      DDbgPrint("  Invalid Output Buffer length \n");                          \
      Buffer = NULL;                                                           \
      Exit(Irp, Status, InformationSize);                                      \
    }                                                                          \
  }
#define GET_IRP_OUPUTBUFFER(Irp, Buffer, NeededSize, Exit)                     \
  GET_IRP_OUPUTBUFFER_EX(Irp, Buffer, NeededSize, Exit,                        \
                         STATUS_BUFFER_TOO_SMALL, 0)                                \

// Specific buffer calcul size
#define CUSTOM_SIZE_COMPARE(Buffer, BufferLen, LengthMember)                   \
  (sizeof(*Buffer) > BufferLen ||                                              \
   (sizeof(*Buffer) + Buffer->##LengthMember > BufferLen))

#define BUFFERLEN_SIZE_COMPARE(Buffer, BufferLen)                              \
  CUSTOM_SIZE_COMPARE(Buffer, BufferLen, LengthMember)
#define PATHNAME_REQUEST_SIZE_COMPARE(Buffer, BufferLen)                       \
  CUSTOM_SIZE_COMPARE(Buffer, BufferLen, PathNameLength)
#define LEN_SIZE_COMPARE(Buffer, BufferLen)                                    \
  CUSTOM_SIZE_COMPARE(Buffer, BufferLen, Length)
#define NAMELEN_SIZE_COMPARE(Buffer, BufferLen)                                \
  CUSTOM_SIZE_COMPARE(Buffer, BufferLen, NameLength)

#define LEN_MAXIMUMLEN_SIZE_COMPARE(Buffer, BufferLen)                         \
  (sizeof(*Buffer) > BufferLen ||                                              \
   (sizeof(*Buffer) + Buffer->Length > BufferLen) ||                           \
   (sizeof(*Buffer) + Buffer->MaximumLength > BufferLen) ||                    \
   Buffer->Length > Buffer->MaximumLength)

// Generic Get DeviceIOControl Buffer from IRP
#define GET_IRP_GENERIC_BUFFER(Irp, Buffer, BufferIOType)                      \
  GET_IRP_BUFFER(Irp, Buffer, BufferIOType, GENERIC_SIZE_COMPARE)
// Leave
#define GET_IRP_GENERIC_BUFFER_LEAVE2(Irp, Buffer, BufferIOType, Status,       \
                                      InformationSize)                         \
  GET_IRP_BUFFER_EX(Irp, Buffer, BufferIOType, GENERIC_SIZE_COMPARE,           \
                    DOKAN_EXIT_LEAVE, Status, InformationSize)
#define GET_IRP_GENERIC_BUFFER_LEAVE(Irp, Buffer, BufferIOType)                \
  GET_IRP_GENERIC_BUFFER_LEAVE2(Irp, Buffer, BufferIOType,                     \
                                STATUS_BUFFER_TOO_SMALL, 0)
// Break
#define GET_IRP_GENERIC_BUFFER_BREAK2(Irp, Buffer, BufferIOType, Status,       \
                                      InformationSize)                         \
  GET_IRP_BUFFER_EX(Irp, Buffer, BufferIOType, GENERIC_SIZE_COMPARE,           \
                    DOKAN_EXIT_BREAK, Status, InformationSize)
#define GET_IRP_GENERIC_BUFFER_BREAK(Irp, Buffer, BufferIOType)                \
  GET_IRP_GENERIC_BUFFER_BREAK2(Irp, Buffer, BufferIOType,                     \
                                STATUS_BUFFER_TOO_SMALL, 0)
// Return
#define GET_IRP_GENERIC_BUFFER_RETURN(Irp, Buffer, BufferIOType)               \
  GET_IRP_BUFFER_EX(Irp, Buffer, BufferIOType, GENERIC_SIZE_COMPARE,           \
                    DOKAN_EXIT_RETURN, STATUS_BUFFER_TOO_SMALL, 0)

// Get DeviceIOControl Buffer from IRP for EVENT_INFORMATION
//#define GET_IRP_EVENT_INFORMATION_BUFFER(Irp, Buffer, BufferIOType)            \
//  GET_IRP_BUFFER(Irp, Buffer, BufferIOType, EVENT_INFORMATION_SIZE_COMPARE)
//#define GET_IRP_EVENT_INFORMATION_BUFFER_LEAVE(Irp, Buffer, BufferIOType,      \
//                                              Status, InformationSize)            \
//  GET_IRP_BUFFER_EX(Irp, Buffer, BufferIOType, EVENT_INFORMATION_SIZE_COMPARE, \
//                    DOKAN_EXIT_LEAVE, Status, InformationSize)

// Get DeviceIOControl Buffer from IRP for MOUNTDEV_NAME
#define GET_IRP_MOUNTDEV_NAME_BUFFER(Irp, Buffer, BufferIOType)                \
  GET_IRP_BUFFER(Irp, Buffer, BufferIOType, NAMELEN_SIZE_COMPARE)
#define GET_IRP_MOUNTDEV_NAME_BUFFER_BREAK(Irp, Buffer, BufferIOType)          \
  GET_IRP_BUFFER_EX(Irp, Buffer, BufferIOType, NAMELEN_SIZE_COMPARE,           \
                    DOKAN_EXIT_BREAK, STATUS_BUFFER_TOO_SMALL, 0)

// Get DeviceIOControl Buffer from IRP for QUERY_PATH_REQUEST

// Get DeviceIOControl Buffer from IRP for DOKAN_NOTIFY_PATH_INTERMEDIATE
#define GET_IRP_DOKAN_NOTIFY_PATH_INTERMEDIATE_BUFFER_RETURN(Irp, Buffer,      \
                                                             BufferIOType)     \
  GET_IRP_BUFFER_EX(Irp, Buffer, BufferIOType, LEN_SIZE_COMPARE,               \
                    DOKAN_EXIT_RETURN, STATUS_BUFFER_TOO_SMALL, 0)

// Get DeviceIOControl Buffer from IRP for DOKAN_UNICODE_STRING_INTERMEDIATE
#define GET_IRP_DOKAN_UNICODE_STRING_INTERMEDIATE_BUFFER_RETURN(Irp, Buffer,   \
                                                                BufferIOType)  \
  GET_IRP_BUFFER_EX(Irp, Buffer, BufferIOType, LEN_MAXIMUMLEN_SIZE_COMPARE,    \
                    DOKAN_EXIT_RETURN, STATUS_BUFFER_TOO_SMALL, 0)

// Get DeviceIOControl Buffer from IRP for MOUNTDEV_SUGGESTED_LINK_NAME
#define GET_IRP_MOUNTDEV_SUGGESTED_LINK_NAME_BREAK2(Irp, Buffer, BufferIOType, \
                                                    Status, InformationSize)   \
  GET_IRP_BUFFER_EX(Irp, Buffer, BufferIOType, NAMELEN_SIZE_COMPARE,           \
                    DOKAN_EXIT_BREAK, Status, InformationSize)

#endif