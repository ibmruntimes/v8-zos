// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_MACROS_H_
#define V8_BASE_MACROS_H_

#include <stddef.h>
#include <stdint.h>

#include <cstring>

#include "src/base/build_config.h"
#include "src/base/compiler-specific.h"
#include "src/base/logging.h"


// TODO(all) Replace all uses of this macro with C++'s offsetof. To do that, we
// have to make sure that only standard-layout types and simple field
// designators are used.
#define OFFSET_OF(type, field) \
  (reinterpret_cast<intptr_t>(&(reinterpret_cast<type*>(16)->field)) - 16)


#if V8_OS_NACL

// ARRAYSIZE_UNSAFE performs essentially the same calculation as arraysize,
// but can be used on anonymous types or types defined inside
// functions.  It's less safe than arraysize as it accepts some
// (although not all) pointers.  Therefore, you should use arraysize
// whenever possible.
//
// The expression ARRAYSIZE_UNSAFE(a) is a compile-time constant of type
// size_t.
//
// ARRAYSIZE_UNSAFE catches a few type errors.  If you see a compiler error
//
//   "warning: division by zero in ..."
//
// when using ARRAYSIZE_UNSAFE, you are (wrongfully) giving it a pointer.
// You should only use ARRAYSIZE_UNSAFE on statically allocated arrays.
//
// The following comments are on the implementation details, and can
// be ignored by the users.
//
// ARRAYSIZE_UNSAFE(arr) works by inspecting sizeof(arr) (the # of bytes in
// the array) and sizeof(*(arr)) (the # of bytes in one array
// element).  If the former is divisible by the latter, perhaps arr is
// indeed an array, in which case the division result is the # of
// elements in the array.  Otherwise, arr cannot possibly be an array,
// and we generate a compiler error to prevent the code from
// compiling.
//
// Since the size of bool is implementation-defined, we need to cast
// !(sizeof(a) & sizeof(*(a))) to size_t in order to ensure the final
// result has type size_t.
//
// This macro is not perfect as it wrongfully accepts certain
// pointers, namely where the pointer size is divisible by the pointee
// size.  Since all our code has to go through a 32-bit compiler,
// where a pointer is 4 bytes, this means all pointers to a type whose
// size is 3 or greater than 4 will be (righteously) rejected.
#define ARRAYSIZE_UNSAFE(a)     \
  ((sizeof(a) / sizeof(*(a))) / \
   static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))  // NOLINT

// TODO(bmeurer): For some reason, the NaCl toolchain cannot handle the correct
// definition of arraysize() below, so we have to use the unsafe version for
// now.
#define arraysize ARRAYSIZE_UNSAFE

#else  // V8_OS_NACL

// The arraysize(arr) macro returns the # of elements in an array arr.
// The expression is a compile-time constant, and therefore can be
// used in defining new arrays, for example.  If you use arraysize on
// a pointer by mistake, you will get a compile-time error.
//
// One caveat is that arraysize() doesn't accept any array of an
// anonymous type or a type defined inside a function.  In these rare
// cases, you have to use the unsafe ARRAYSIZE_UNSAFE() macro below.  This is
// due to a limitation in C++'s template system.  The limitation might
// eventually be removed, but it hasn't happened yet.
#define arraysize(array) (sizeof(ArraySizeHelper(array)))


// This template function declaration is used in defining arraysize.
// Note that the function doesn't need an implementation, as we only
// use its type.
template <typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];


#if !V8_CC_MSVC
// That gcc wants both of these prototypes seems mysterious. VC, for
// its part, can't decide which to use (another mystery). Matching of
// template overloads: the final frontier.
template <typename T, size_t N>
char (&ArraySizeHelper(const T (&array)[N]))[N];
#endif

#endif  // V8_OS_NACL


// bit_cast<Dest,Source> is a template function that implements the
// equivalent of "*reinterpret_cast<Dest*>(&source)".  We need this in
// very low-level functions like the protobuf library and fast math
// support.
//
//   float f = 3.14159265358979;
//   int i = bit_cast<int32>(f);
//   // i = 0x40490fdb
//
// The classical address-casting method is:
//
//   // WRONG
//   float f = 3.14159265358979;            // WRONG
//   int i = * reinterpret_cast<int*>(&f);  // WRONG
//
// The address-casting method actually produces undefined behavior
// according to ISO C++ specification section 3.10 -15 -.  Roughly, this
// section says: if an object in memory has one type, and a program
// accesses it with a different type, then the result is undefined
// behavior for most values of "different type".
//
// This is true for any cast syntax, either *(int*)&f or
// *reinterpret_cast<int*>(&f).  And it is particularly true for
// conversions between integral lvalues and floating-point lvalues.
//
// The purpose of 3.10 -15- is to allow optimizing compilers to assume
// that expressions with different types refer to different memory.  gcc
// 4.0.1 has an optimizer that takes advantage of this.  So a
// non-conforming program quietly produces wildly incorrect output.
//
// The problem is not the use of reinterpret_cast.  The problem is type
// punning: holding an object in memory of one type and reading its bits
// back using a different type.
//
// The C++ standard is more subtle and complex than this, but that
// is the basic idea.
//
// Anyways ...
//
// bit_cast<> calls memcpy() which is blessed by the standard,
// especially by the example in section 3.9 .  Also, of course,
// bit_cast<> wraps up the nasty logic in one place.
//
// Fortunately memcpy() is very fast.  In optimized mode, with a
// constant size, gcc 2.95.3, gcc 4.0.1, and msvc 7.1 produce inline
// code with the minimal amount of data movement.  On a 32-bit system,
// memcpy(d,s,4) compiles to one load and one store, and memcpy(d,s,8)
// compiles to two loads and two stores.
//
// I tested this code with gcc 2.95.3, gcc 4.0.1, icc 8.1, and msvc 7.1.
//
// WARNING: if Dest or Source is a non-POD type, the result of the memcpy
// is likely to surprise you.
template <class Dest, class Source>
V8_INLINE Dest bit_cast(Source const& source) {
  static_assert(sizeof(Dest) == sizeof(Source),
                "source and dest must be same size");
  Dest dest;
  memcpy(&dest, &source, sizeof(dest));
  return dest;
}


// Put this in the private: declarations for a class to be unassignable.
#define DISALLOW_ASSIGN(TypeName) void operator=(const TypeName&)


// A macro to disallow the evil copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;      \
  void operator=(const TypeName&) = delete


// A macro to disallow all the implicit constructors, namely the
// default constructor, copy constructor and operator= functions.
//
// This should be used in the private: declarations for a class
// that wants to prevent anyone from instantiating it. This is
// especially useful for classes containing only static methods.
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName() = delete;                           \
  DISALLOW_COPY_AND_ASSIGN(TypeName)


// Newly written code should use V8_INLINE and V8_NOINLINE directly.
#define INLINE(declarator)    V8_INLINE declarator
#define NO_INLINE(declarator) V8_NOINLINE declarator


// Newly written code should use WARN_UNUSED_RESULT.
#define MUST_USE_RESULT WARN_UNUSED_RESULT


// Define V8_USE_ADDRESS_SANITIZER macros.
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define V8_USE_ADDRESS_SANITIZER 1
#endif
#endif

// Define DISABLE_ASAN macros.
#ifdef V8_USE_ADDRESS_SANITIZER
#define DISABLE_ASAN __attribute__((no_sanitize_address))
#else
#define DISABLE_ASAN
#endif


#if V8_CC_GNU
#define V8_IMMEDIATE_CRASH() __builtin_trap()
#else
#define V8_IMMEDIATE_CRASH() ((void(*)())0)()
#endif


// TODO(all) Replace all uses of this macro with static_assert, remove macro.
#define STATIC_ASSERT(test) static_assert(test, #test)


// The USE(x) template is used to silence C++ compiler warnings
// issued for (yet) unused variables (typically parameters).
template <typename T>
inline void USE(T) { }


#define IS_POWER_OF_TWO(x) ((x) != 0 && (((x) & ((x) - 1)) == 0))


// Define our own macros for writing 64-bit constants.  This is less fragile
// than defining __STDC_CONSTANT_MACROS before including <stdint.h>, and it
// works on compilers that don't have it (like MSVC).
#if V8_CC_MSVC
# define V8_UINT64_C(x)   (x ## UI64)
# define V8_INT64_C(x)    (x ## I64)
# if V8_HOST_ARCH_64_BIT
#  define V8_INTPTR_C(x)  (x ## I64)
#  define V8_PTR_PREFIX   "ll"
# else
#  define V8_INTPTR_C(x)  (x)
#  define V8_PTR_PREFIX   ""
# endif  // V8_HOST_ARCH_64_BIT
#elif V8_CC_MINGW64
# define V8_UINT64_C(x)   (x ## ULL)
# define V8_INT64_C(x)    (x ## LL)
# define V8_INTPTR_C(x)   (x ## LL)
# define V8_PTR_PREFIX    "I64"
#elif V8_HOST_ARCH_64_BIT
# if V8_OS_MACOSX || V8_OS_OPENBSD
#  define V8_UINT64_C(x)   (x ## ULL)
#  define V8_INT64_C(x)    (x ## LL)
# else
#  define V8_UINT64_C(x)   (x ## UL)
#  define V8_INT64_C(x)    (x ## L)
# endif
# define V8_INTPTR_C(x)   (x ## L)
# define V8_PTR_PREFIX    "l"
#else
# define V8_UINT64_C(x)   (x ## ULL)
# define V8_INT64_C(x)    (x ## LL)
# define V8_INTPTR_C(x)   (x)
#if V8_OS_AIX
#define V8_PTR_PREFIX "l"
#else
# define V8_PTR_PREFIX    ""
#endif
#endif

#define V8PRIxPTR V8_PTR_PREFIX "x"
#define V8PRIdPTR V8_PTR_PREFIX "d"
#define V8PRIuPTR V8_PTR_PREFIX "u"

// Fix for Mac OS X defining uintptr_t as "unsigned long":
#if V8_OS_MACOSX
#undef V8PRIxPTR
#define V8PRIxPTR "lx"
#undef V8PRIuPTR
#define V8PRIuPTR "lxu"
#endif

// GCC on S390 31-bit expands 'size_t' to 'long unsigned int'
// instead of 'int', resulting in compilation errors with %d.
// The printf format specifier needs to be %zd instead.
#if V8_HOST_ARCH_S390 && !V8_HOST_ARCH_64_BIT
#define V8_SIZET_PREFIX "z"
#else
#define V8_SIZET_PREFIX ""
#endif

// The following macro works on both 32 and 64-bit platforms.
// Usage: instead of writing 0x1234567890123456
//      write V8_2PART_UINT64_C(0x12345678,90123456);
#define V8_2PART_UINT64_C(a, b) (((static_cast<uint64_t>(a) << 32) + 0x##b##u))


// Compute the 0-relative offset of some absolute value x of type T.
// This allows conversion of Addresses and integral types into
// 0-relative int offsets.
template <typename T>
inline intptr_t OffsetFrom(T x) {
  return x - static_cast<T>(0);
}


// Compute the absolute value of type T for some 0-relative offset x.
// This allows conversion of 0-relative int offsets into Addresses and
// integral types.
template <typename T>
inline T AddressFrom(intptr_t x) {
  return static_cast<T>(static_cast<T>(0) + x);
}


// Return the largest multiple of m which is <= x.
template <typename T>
inline T RoundDown(T x, intptr_t m) {
  DCHECK(IS_POWER_OF_TWO(m));
  return AddressFrom<T>(OffsetFrom(x) & -m);
}


// Return the smallest multiple of m which is >= x.
template <typename T>
inline T RoundUp(T x, intptr_t m) {
  return RoundDown<T>(static_cast<T>(x + m - 1), m);
}


namespace v8 {
namespace base {

// TODO(yangguo): This is a poor man's replacement for std::is_fundamental,
// which requires C++11. Switch to std::is_fundamental once possible.
template <typename T>
inline bool is_fundamental() {
  return false;
}

template <>
inline bool is_fundamental<uint8_t>() {
  return true;
}

}  // namespace base
}  // namespace v8

#ifdef V8_OS_ZOS
inline const uint8_t& Ascii2Ebcdic(const char letter) {
  static unsigned char a2e[256] = {
  0,1,2,3,55,45,46,47,22,5,21,11,12,13,14,15,
  16,17,18,19,60,61,50,38,24,25,63,39,28,29,30,31,
  64,79,127,123,91,108,80,125,77,93,92,78,107,96,75,97,
  240,241,242,243,244,245,246,247,248,249,122,94,76,126,110,111,
  124,193,194,195,196,197,198,199,200,201,209,210,211,212,213,214,
  215,216,217,226,227,228,229,230,231,232,233,74,224,90,95,109,
  121,129,130,131,132,133,134,135,136,137,145,146,147,148,149,150,
  151,152,153,162,163,164,165,166,167,168,169,192,106,208,161,7,
  32,33,34,35,36,21,6,23,40,41,42,43,44,9,10,27,
  48,49,26,51,52,53,54,8,56,57,58,59,4,20,62,225,
  65,66,67,68,69,70,71,72,73,81,82,83,84,85,86,87,
  88,89,98,99,100,101,102,103,104,105,112,113,114,115,116,117,
  118,119,120,128,138,139,140,141,142,143,144,154,155,156,157,158,
  159,160,170,171,172,173,174,175,176,177,178,179,180,181,182,183,
  184,185,186,187,188,189,190,191,202,203,204,205,206,207,218,219,
  220,221,222,223,234,235,236,237,238,239,250,251,252,253,254,255
  };
  return a2e[letter];
}

inline const uint8_t& Ebcdic2Ascii(const char letter) {
  static const uint8_t e2a[256] = {
  0,1,2,3,156,9,134,127,151,141,142,11,12,13,14,15,
  16,17,18,19,157,10,8,135,24,25,146,143,28,29,30,31,
  128,129,130,131,132,10,23,27,136,137,138,139,140,5,6,7,
  144,145,22,147,148,149,150,4,152,153,154,155,20,21,158,26,
  32,160,161,162,163,164,165,166,167,168,91,46,60,40,43,33,
  38,169,170,171,172,173,174,175,176,177,93,36,42,41,59,94,
  45,47,178,179,180,181,182,183,184,185,124,44,37,95,62,63,
  186,187,188,189,190,191,192,193,194,96,58,35,64,39,61,34,
  195,97,98,99,100,101,102,103,104,105,196,197,198,199,200,201,
  202,106,107,108,109,110,111,112,113,114,203,204,205,206,207,208,
  209,126,115,116,117,118,119,120,121,122,210,211,212,213,214,215,
  216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,
  123,65,66,67,68,69,70,71,72,73,232,233,234,235,236,237,
  125,74,75,76,77,78,79,80,81,82,238,239,240,241,242,243,
  92,159,83,84,85,86,87,88,89,90,244,245,246,247,248,249,
  48,49,50,51,52,53,54,55,56,57,250,251,252,253,254,255
  };
  return e2a[letter];
}
#define GET_ASCII_CODE(byte_char) byte_char
#define CONVERT_TO_ASCII(data, length) \
    for (int i = 0; i < length ; i++) { \
        data[i] = GET_ASCII_CODE(data[i]);\
    }
#define NATIVE_ENCODING(byte_char) byte_char
#else
#define GET_ASCII_CODE(byte_char) byte_char
#define CONVERT_TO_ASCII(data, length)
#define NATIVE_ENCODING(byte_char) byte_char
#endif


#endif   // V8_BASE_MACROS_H_
