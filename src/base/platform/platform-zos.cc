// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Platform-specific code for zOS/Unix goes here. For the POSIX-compatible
// parts, the implementation is in platform-posix.cc.

#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>

// Ubuntu Dapper requires memory pages to be marked as
// executable. Otherwise, OS raises an exception when executing code
// in that page.
#include <errno.h>
#include <fcntl.h>      // open
#include <stdarg.h>
#include <strings.h>    // index
#undef index
#include <sys/mman.h>   // mmap & munmap
#include <sys/stat.h>   // open
#include <sys/types.h>  // mmap & munmap
#include <unistd.h>     // sysconf

// GLibc on ARM defines mcontext_t has a typedef for 'struct sigcontext'.
// Old versions of the C library <signal.h> didn't define the type.
#if defined(__ANDROID__) && !defined(__BIONIC_HAVE_UCONTEXT_T) && \
    (defined(__arm__) || defined(__aarch64__)) && \
    !defined(__BIONIC_HAVE_STRUCT_SIGCONTEXT)
#include <asm/sigcontext.h>  // NOLINT
#endif

#if defined(LEAK_SANITIZER)
#include <sanitizer/lsan_interface.h>
#endif

#include <cmath>

#undef MAP_TYPE

#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/s390/semaphore-zos.h"

#define MAP_FAILED ((void *)-1L)

namespace v8 {
namespace base {


#ifdef __arm__

bool OS::ArmUsingHardFloat() {
  // GCC versions 4.6 and above define __ARM_PCS or __ARM_PCS_VFP to specify
  // the Floating Point ABI used (PCS stands for Procedure Call Standard).
  // We use these as well as a couple of other defines to statically determine
  // what FP ABI used.
  // GCC versions 4.4 and below don't support hard-fp.
  // GCC versions 4.5 may support hard-fp without defining __ARM_PCS or
  // __ARM_PCS_VFP.

#define GCC_VERSION (__GNUC__ * 10000                                          \
                     + __GNUC_MINOR__ * 100                                    \
                     + __GNUC_PATCHLEVEL__)
#if GCC_VERSION >= 40600
#if defined(__ARM_PCS_VFP)
  return true;
#else
  return false;
#endif

#elif GCC_VERSION < 40500
  return false;

#else
#if defined(__ARM_PCS_VFP)
  return true;
#elif defined(__ARM_PCS) || defined(__SOFTFP__) || defined(__SOFTFP) || \
      !defined(__VFP_FP__)
  return false;
#else
#error "\x59\x6f\x75\x72\x20\x76\x65\x72\x73\x69\x6f\x6e\x20\x6f\x66\x20\x47\x43\x43\x20\x64\x6f\x65\x73\x20\x6e\x6f\x74\x20\x72\x65\x70\x6f\x72\x74\x20\x74\x68\x65\x20\x46\x50\x20\x41\x42\x49\x20\x63\x6f\x6d\x70\x69\x6c\x65\x64\x20\x66\x6f\x72\x2e"          \
       "\x50\x6c\x65\x61\x73\x65\x20\x72\x65\x70\x6f\x72\x74\x20\x69\x74\x20\x6f\x6e\x20\x74\x68\x69\x73\x20\x69\x73\x73\x75\x65"                                        \
       "\x68\x74\x74\x70\x3a\x2f\x2f\x63\x6f\x64\x65\x2e\x67\x6f\x6f\x67\x6c\x65\x2e\x63\x6f\x6d\x2f\x70\x2f\x76\x38\x2f\x69\x73\x73\x75\x65\x73\x2f\x64\x65\x74\x61\x69\x6c\x3f\x69\x64\x3d\x32\x31\x34\x30"

#endif
#endif
#undef GCC_VERSION
}

#endif  // def __arm__


#define asm __asm__ volatile

static void * anon_mmap(void * addr, size_t len) {
   int retcode;
   char * p;
#pragma convert("ibm-1047")
#if defined(__64BIT__)
  __asm(" SYSSTATE ARCHLVL=2,AMODE64=YES\n"
        " STORAGE OBTAIN,LENGTH=(%2),BNDRY=PAGE,COND=YES,ADDR=(%0),RTCD=(%1),"
        "LOC=(31,64)\n"
        :"=r"(p),"=r"(retcode): "r"(len): "r0","r1","r14","r15");
#else
  __asm(" SYSSTATE ARCHLVL=2\n"
        " STORAGE OBTAIN,LENGTH=(%2),BNDRY=PAGE,COND=YES,ADDR=(%0),RTCD=(%1)\n"
        :"=r"(p),"=r"(retcode): "r"(len): "r0","r1","r14","r15");
#endif
#pragma convert(pop)
   return (retcode == 0) ? p : MAP_FAILED;
}


static int anon_munmap(void * addr, size_t len) {
   int retcode;
#pragma convert("ibm-1047")
#if defined (__64BIT__)
  __asm(" SYSSTATE ARCHLVL=2,AMODE64=YES\n"
          " STORAGE RELEASE,LENGTH=(%2),ADDR=(%1),RTCD=(%0),COND=YES\n"
          :"=r"(retcode): "r"(addr), "r"(len) : "r0","r1","r14","r15");
#else
  __asm(" SYSSTATE ARCHLVL=2\n"
          " STORAGE RELEASE,LENGTH=(%2),ADDR=(%1),RTCD=(%0),COND=YES\n"
          :"=r"(retcode): "r"(addr), "r"(len) : "r0","r1","r14","r15");
#endif
#pragma convert(pop)
   return retcode;
}


void OS::Free(void* address, const size_t size) {
  // TODO(1240712): munmap has a return value which is ignored here.
  int result = anon_munmap(address, size);
  USE(result);
  DCHECK(result == 0);
}


void OS::ConvertToASCII(char * str) {
  size_t length =  __e2a_s(str);
  DCHECK_NE(length, -1);
}


const char* OS::LocalTimezone(double time, TimezoneCache* cache) {

  if (isnan(time)) return "";
  time_t tv = static_cast<time_t>(std::floor(time/msPerSecond));
  struct tm* t = localtime(&tv);
  if (NULL == t) return "";
  double offset_secs = LocalTimeOffset(cache);
  int offset_hrs  = (int)offset_secs/3600;
  if ( offset_hrs == 0)
     return "\x47\x4d\x54";
  else if (offset_hrs > -6 && offset_hrs <= -5)
          return "\x45\x53\x54";
  //Todo(muntasir) Add the rest of the timezones
  return "";
}

double OS::LocalTimeOffset(TimezoneCache* cache) {
  time_t tv = time(NULL);
  struct tm* gmt = gmtime(&tv);
  double gm_secs = gmt->tm_sec + (gmt->tm_min * 60) + (gmt->tm_hour * 3600);
  struct tm* localt = localtime(&tv);
  double local_secs = localt->tm_sec + (localt->tm_min * 60) +
                      (localt->tm_hour * 3600);
  return (local_secs - gm_secs) * msPerSecond -
         (localt->tm_isdst > 0 ? 3600 * msPerSecond : 0);
}


void* OS::Allocate(const size_t requested,
                   size_t* allocated,
                   bool is_executable) {
  const size_t msize = RoundUp(requested, AllocateAlignment());
  int prot = PROT_READ | PROT_WRITE | (is_executable ? PROT_EXEC : 0);
  void * mbase = static_cast<void*>(anon_mmap(OS::GetRandomMmapAddr(),
                                    sizeof(char) * msize));
  *allocated = msize;
  return mbase;
}

std::vector<OS::SharedLibraryAddress> OS::GetSharedLibraryAddresses() {
  std::vector<SharedLibraryAddress> result;
  // This function assumes that the layout of the file is as follows:
  // hex_start_addr-hex_end_addr rwxp <unused data> [binary_file_name]
  // If we encounter an unexpected situation we abort scanning further entries.
  FILE* fp = fopen("\x2f\x70\x72\x6f\x63\x2f\x73\x65\x6c\x66\x2f\x6d\x61\x70\x73", "\x72");
  if (fp == NULL) return result;

  // Allocate enough room to be able to store a full file name.
  const int kLibNameLen = FILENAME_MAX + 1;
  char* lib_name = reinterpret_cast<char*>(malloc(kLibNameLen));

  // This loop will terminate once the scanning hits an EOF.
  while (true) {
    uintptr_t start, end;
    char attr_r, attr_w, attr_x, attr_p;
    // Parse the addresses and permission bits at the beginning of the line.
    if (fscanf(fp, "\x25" V8PRIxPTR "\x2d\x25" V8PRIxPTR, &start, &end) != 2) break;
    if (fscanf(fp, "\x20\x6c\x83\x6c\x83\x6c\x83\x6c\x83", &attr_r, &attr_w, &attr_x, &attr_p) != 4) break;

    int c;
    if (attr_r == '\x72' && attr_w != '\x77' && attr_x == '\x78') {
      // Found a read-only executable entry. Skip characters until we reach
      // the beginning of the filename or the end of the line.
      do {
        c = getc(fp);
      } while ((c != EOF) && (c != '\xa') && (c != '\x2f') && (c != '\x5b'));
      if (c == EOF) break;  // EOF: Was unexpected, just exit.

      // Process the filename if found.
      if ((c == '\x2f') || (c == '\x5b')) {
        // Push the '/' or '[' back into the stream to be read below.
        ungetc(c, fp);

		// Read to the end of the line. Exit if the read fails.
        if (fgets(lib_name, kLibNameLen, fp) == NULL) break;

        // Drop the newline character read by fgets. We do not need to check
        // for a zero-length string because we know that we at least read the
        // '/' or '[' character.
        lib_name[strlen(lib_name) - 1] = '\x0';
      } else {
        // No library name found, just record the raw address range.
        snprintf(lib_name, kLibNameLen,
                 "\x25\x30\x38" V8PRIxPTR "\x2d\x25\x30\x38" V8PRIxPTR, start, end);
      }
      result.push_back(SharedLibraryAddress(lib_name, start, end));
    } else {
      // Entry not describing executable data. Skip to end of line to set up
      // reading the next entry.
      do {
        c = getc(fp);
      } while ((c != EOF) && (c != '\xa'));
      if (c == EOF) break;
    }
  }
  free(lib_name);
  fclose(fp);
  return result;
}


void OS::SignalCodeMovingGC() {
  // Support for ll_prof.py.
  //
  // The Linux profiler built into the kernel logs all mmap's with
  // PROT_EXEC so that analysis tools can properly attribute ticks. We
  // do a mmap with a name known by ll_prof.py and immediately munmap
  // it. This injects a GC marker into the stream of events generated
  // by the kernel and allows us to synchronize V8 code log and the
  // kernel log.
  int size = sysconf(_SC_PAGESIZE);
  FILE* f = NULL;
  if (f == NULL) {
    OS::PrintError("\x46\x61\x69\x6c\x65\x64\x20\x74\x6f\x20\x6f\x70\x65\x6e\x20\x6c\xa2\xa", "");
    OS::Abort();
  }
  void* addr = mmap(OS::GetRandomMmapAddr(),
                    size,
#if defined(__native_client__)
                    // The Native Client port of V8 uses an interpreter,
                    // so code pages don't need PROT_EXEC.
                    PROT_READ,
#else
                    PROT_READ | PROT_EXEC,
#endif
                    MAP_PRIVATE,
                    fileno(f),
                    0);
  OS::Free(addr, size);
  fclose(f);
}

static const int kMmapFd = -1;
// Constants used for mmap.
static const int kMmapFdOffset = 0;

VirtualMemory::VirtualMemory() : address_(NULL), size_(0) { }


VirtualMemory::VirtualMemory(size_t size)
    : address_(ReserveRegion(size)), size_(size) { }


VirtualMemory::VirtualMemory(size_t size, size_t alignment)
    : address_(NULL), size_(0) {
  DCHECK(IsAligned(alignment, static_cast<intptr_t>(OS::AllocateAlignment())));

  size_t request_size = RoundUp(size + alignment,
                                static_cast<intptr_t>(OS::AllocateAlignment()));

  void* reservation = anon_mmap(OS::GetRandomMmapAddr(),
                           request_size);

  if (reservation == MAP_FAILED) return;

  uint8_t* base = static_cast<uint8_t*>(reservation);
  uint8_t* aligned_base = RoundUp(base, alignment);
  DCHECK_LE(base, aligned_base);

  // Unmap extra memory reserved before and after the desired block.
  if (aligned_base != base) {
    size_t prefix_size = static_cast<size_t>(aligned_base - base);
    OS::Free(base, prefix_size);
    request_size -= prefix_size;
  }

  size_t aligned_size = RoundUp(size, OS::AllocateAlignment());
  DCHECK_LE(aligned_size, request_size);

  if (aligned_size != request_size) {
    size_t suffix_size = request_size - aligned_size;
    OS::Free(aligned_base + aligned_size, suffix_size);
    request_size -= suffix_size;
  }

  DCHECK(aligned_size == request_size);

  address_ = static_cast<void*>(aligned_base);
  size_ = aligned_size;
#if defined(LEAK_SANITIZER)
  __lsan_register_root_region(address_, size_);
#endif
}


VirtualMemory::~VirtualMemory() {
  if (IsReserved()) {
    bool result = ReleaseRegion(address(), size());
    DCHECK(result);
    USE(result);
  }
}

bool VirtualMemory::IsReserved() {
  return address_ != NULL;
}


void VirtualMemory::Reset() {
  address_ = NULL;
  size_ = 0;
}


bool VirtualMemory::Commit(void* address, size_t size, bool is_executable) {
  return CommitRegion(address, size, is_executable);
}


bool VirtualMemory::Uncommit(void* address, size_t size) {
  return UncommitRegion(address, size);
}


bool VirtualMemory::Guard(void* address) {
  OS::Guard(address, OS::CommitPageSize());
  return true;
}


void* VirtualMemory::ReserveRegion(size_t size) {
  void* result = anon_mmap(OS::GetRandomMmapAddr(),
                      size);

  if (result == MAP_FAILED) return NULL;
#if defined(LEAK_SANITIZER)
  __lsan_register_root_region(result, size);
#endif
  return result;
}


bool VirtualMemory::CommitRegion(void* base, size_t size, bool is_executable) {
 /*mprotect can not be called on pages allocated with
   STORAGE OBTAIN, for now we will leave this operation as a
   NOP..might need to use CHANGEKEY macro to implement something
   akin to mprotect in the future*/
   return true;
}


bool VirtualMemory::UncommitRegion(void* base, size_t size) {
 /*mprotect can not be called on pages allocated with
   STORAGE OBTAIN, for now we will leave this operation as a
   NOP..might need to use CHANGEKEY macro to implement something
   akin to mprotect in the future*/
   return true;
}

bool VirtualMemory::ReleaseRegion(void* base, size_t size) {
#if defined(LEAK_SANITIZER)
  __lsan_unregister_root_region(base, size);
#endif
  return anon_munmap(base, size) == 0;
}


bool VirtualMemory::HasLazyCommits() {
  return true;
}

int OS::SNPrintFASCII(char* str, int length, const char* format, ...) {
  va_list args;
  va_start(args, format);
  int result = VSNPrintFASCII(str, length, format, args);
  va_end(args);
  return result;
}


int OS::VSNPrintFASCII(char* str,
                  int length,
                  const char* format,
                  va_list args) {
  int n = VSNPrintFASCII(str, length, format, args);
  if (n < 0 || n >= length) {
    // If the length is zero, the assignment fails.
    if (length > 0)
      str[length - 1] = '\x0';
    return -1;
  } else {
    return n;
  }
}

} }  // namespace v8::base
