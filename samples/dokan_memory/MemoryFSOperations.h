#pragma once

#include <dokan/dokan.h>

extern DOKAN_OPERATIONS MemoryFSOperations;

#define fs_instance                     \
  reinterpret_cast<MemoryFSFileNodes*>( \
      DokanFileInfo->DokanOptions->GlobalContext)

NTSTATUS ZwCreateFile(LPCWSTR FileName,
                      PDOKAN_IO_SECURITY_CONTEXT SecurityContext,
                      ACCESS_MASK DesiredAccess, ULONG FileAttributes,
                      ULONG ShareAccess, ULONG CreateDisposition,
                      ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo);

void Cleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo);

void CloseFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo);

NTSTATUS ReadFile(LPCWSTR FileName, LPVOID Buffer, DWORD BufferLength,
                  LPDWORD ReadLength, LONGLONG Offset,
                  PDOKAN_FILE_INFO DokanFileInfo);

NTSTATUS WriteFile(LPCWSTR FileName, LPCVOID Buffer, DWORD NumberOfBytesToWrite,
                   LPDWORD NumberOfBytesWritten, LONGLONG Offset,
                   PDOKAN_FILE_INFO DokanFileInfo);

NTSTATUS FlushFileBuffers(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo);

NTSTATUS GetFileInformation(LPCWSTR FileName,
                            LPBY_HANDLE_FILE_INFORMATION Buffer,
                            PDOKAN_FILE_INFO DokanFileInfo);
NTSTATUS FindFiles(LPCWSTR FileName, PFillFindData FillFindData,
                   PDOKAN_FILE_INFO DokanFileInfo);

NTSTATUS FindFilesWithPattern(LPCWSTR PathName, LPCWSTR SearchPattern,
                              PFillFindData FillFindData,
                              PDOKAN_FILE_INFO DokanFileInfo);

NTSTATUS SetFileAttributes(LPCWSTR FileName, DWORD FileAttributes,
                           PDOKAN_FILE_INFO DokanFileInfo);

NTSTATUS SetFileTime(LPCWSTR FileName, CONST FILETIME* CreationTime,
                     CONST FILETIME* LastAccessTime,
                     CONST FILETIME* LastWriteTime,
                     PDOKAN_FILE_INFO DokanFileInfo);

NTSTATUS DeleteFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo);

NTSTATUS DeleteDirectory(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo);

NTSTATUS MoveFile(LPCWSTR FileName, LPCWSTR NewFileName, BOOL ReplaceIfExisting,
                  PDOKAN_FILE_INFO DokanFileInfo);

NTSTATUS SetEndOfFile(LPCWSTR FileName, LONGLONG ByteOffset,
                      PDOKAN_FILE_INFO DokanFileInfo);

NTSTATUS SetAllocationSize(LPCWSTR FileName, LONGLONG AllocSize,
                           PDOKAN_FILE_INFO DokanFileInfo);

NTSTATUS LockFile(LPCWSTR FileName, LONGLONG ByteOffset, LONGLONG Length,
                  PDOKAN_FILE_INFO DokanFileInfo);

NTSTATUS UnlockFile(LPCWSTR FileName, LONGLONG ByteOffset, LONGLONG Length,
                    PDOKAN_FILE_INFO DokanFileInfo);

NTSTATUS GetDiskFreeSpace(PULONGLONG FreeBytesAvailable,
                          PULONGLONG TotalNumberOfBytes,
                          PULONGLONG TotalNumberOfFreeBytes,
                          PDOKAN_FILE_INFO DokanFileInfo);

NTSTATUS
GetVolumeInformation(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize,
                     LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength,
                     LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer,
                     DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo);

NTSTATUS Mounted(PDOKAN_FILE_INFO DokanFileInfo);

NTSTATUS Unmounted(PDOKAN_FILE_INFO DokanFileInfo);

NTSTATUS GetFileSecurity(LPCWSTR FileName,
                         PSECURITY_INFORMATION SecurityInformation,
                         PSECURITY_DESCRIPTOR SecurityDescriptor,
                         ULONG BufferLength, PULONG LengthNeeded,
                         PDOKAN_FILE_INFO DokanFileInfo);

NTSTATUS SetFileSecurity(LPCWSTR FileName,
                         PSECURITY_INFORMATION SecurityInformation,
                         PSECURITY_DESCRIPTOR SecurityDescriptor,
                         ULONG BufferLength, PDOKAN_FILE_INFO DokanFileInfo);

NTSTATUS FindStreams(LPCWSTR FileName, PFillFindStreamData FillFindStreamData,
                     PDOKAN_FILE_INFO DokanFileInfo);