/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the LICENSE
 * file in the root directory of this source tree.
 */
#ifndef HERMES_BCGEN_HBC_BYTECODEFILEFORMAT_H
#define HERMES_BCGEN_HBC_BYTECODEFILEFORMAT_H

#include "hermes/Support/Compiler.h"
#include "hermes/Support/RegExpSerialization.h"
#include "hermes/Support/SHA1.h"
#include "hermes/Support/StringTableEntry.h"

#include <cassert>
#include <cstdint>
#include <cstring>

namespace hermes {
namespace hbc {

// "Hermes" in ancient Greek encoded in UTF-16BE and truncated to 8 bytes.
const static uint64_t MAGIC = 0x1F1903C103BC1FC6;

// The "delta prepped" form: a different magic number indicating that the
// bytecode file is in a form suitable for delta diffing, not execution.
const static uint64_t DELTA_MAGIC = ~MAGIC;

// Bytecode version generated by this version of the compiler.
// Updated: Jan 14, 2019
const static uint32_t BYTECODE_VERSION = 41;

/// Property cache index which indicates no caching.
static constexpr uint8_t PROPERTY_CACHING_DISABLED = 0;

/// Bytecode forms
enum class BytecodeForm {
  /// Execution form (the default) is the bytecode prepared for execution.
  Execution,

  /// Delta form is the bytecode prepared to minimize binary diff size.
  Delta,
};

/// Storing information about the bytecode, needed when it is loaded by the
/// runtime.
union BytecodeOptions {
  struct {
    bool staticBuiltins : 1;
  };
  uint8_t _flags;

  BytecodeOptions() : _flags(0) {}
};

/**
 * Header of binary file.
 */
LLVM_PACKED_START
struct BytecodeFileHeader {
  uint64_t magic;
  uint32_t version;
  uint8_t sourceHash[SHA1_NUM_BYTES];
  uint32_t fileLength;
  uint32_t globalCodeIndex;
  uint32_t functionCount;
  uint32_t stringCount; // Number of strings in the string table.
  uint32_t identifierCount; // Number of strings which are identifiers.
  uint32_t stringTableBytes; // Bytes of table entries, including overflow.
  uint32_t stringStorageSize; // Bytes in the blob of string contents.
  uint32_t regExpCount;
  uint32_t regExpStorageSize;
  uint32_t arrayBufferSize;
  uint32_t objKeyBufferSize;
  uint32_t objValueBufferSize;
  int32_t cjsModuleCount; // Number of modules, negative if already resolved.
  uint32_t debugInfoOffset;
  BytecodeOptions options;

  // Insert any padding to make function headers that follow this file header
  // less likely to cross cache lines.
  uint8_t padding[7];

  BytecodeFileHeader(
      uint64_t magic,
      uint32_t version,
      const SHA1 &sourceHash,
      uint32_t fileLength,
      uint32_t globalCodeIndex,
      uint32_t functionCount,
      uint32_t stringCount,
      uint32_t identifierCount,
      uint32_t stringTableBytes,
      uint32_t stringStorageSize,
      uint32_t regExpCount,
      uint32_t regExpStorageSize,
      uint32_t arrayBufferSize,
      uint32_t objKeyBufferSize,
      uint32_t objValueBufferSize,
      int32_t cjsModuleCount,
      uint32_t debugInfoOffset,
      BytecodeOptions options)
      : magic(magic),
        version(version),
        sourceHash(),
        fileLength(fileLength),
        globalCodeIndex(globalCodeIndex),
        functionCount(functionCount),
        stringCount(stringCount),
        identifierCount(identifierCount),
        stringTableBytes(stringTableBytes),
        stringStorageSize(stringStorageSize),
        regExpCount(regExpCount),
        regExpStorageSize(regExpStorageSize),
        arrayBufferSize(arrayBufferSize),
        objKeyBufferSize(objKeyBufferSize),
        objValueBufferSize(objValueBufferSize),
        cjsModuleCount(cjsModuleCount),
        debugInfoOffset(debugInfoOffset),
        options(options) {
    std::copy(sourceHash.begin(), sourceHash.end(), this->sourceHash);
    std::fill(padding, padding + sizeof(padding), 0);
  }
};

/// The string table is an array of these entries, followed by an array of
/// OverflowStringTableEntry for the entries whose length or offset doesn't fit
/// into the bitfields.
struct SmallStringTableEntry {
  bool isUTF16 : 1;
  bool isIdentifier : 1;
  uint32_t offset : 22;
  uint32_t length : 8;

  static constexpr uint32_t INVALID_OFFSET = (1 << 22);
  static constexpr uint32_t INVALID_LENGTH = (1 << 8) - 1;

  bool isOverflowed() const {
    return length == INVALID_LENGTH;
  }

  /// Construct a small entry from 'entry'. If any fields overflow, then set
  /// 'overflowOffset' as the offset instead.
  SmallStringTableEntry(
      const StringTableEntry &entry,
      uint32_t overflowOffset) {
    isUTF16 = entry.isUTF16();
    isIdentifier = entry.isIdentifier();
    if (entry.getOffset() < INVALID_OFFSET &&
        entry.getLength() < INVALID_LENGTH) {
      offset = entry.getOffset();
      length = entry.getLength();
    } else {
      assert(overflowOffset < INVALID_OFFSET);
      offset = overflowOffset;
      length = INVALID_LENGTH;
    }
  }
};

/// These are indexed by the 'offset' field of overflowed SmallStringTableEntry.
struct OverflowStringTableEntry {
  uint32_t offset;
  uint32_t length;

  OverflowStringTableEntry(uint32_t offset, uint32_t length)
      : offset(offset), length(length) {}
};

union FunctionHeaderFlag {
  struct {
    bool strictMode : 1;
    bool hasExceptionHandler : 1;
    bool hasDebugInfo : 1;
    bool overflowed : 1;
  };
  uint8_t flags;

  FunctionHeaderFlag() {
    flags = 0;
  }
};

/// FUNC_HEADER_FIELDS is a macro for defining function header fields.
/// The args are API type, small storage type, name, and bit length.
/// The types can be different if the overflow supports a longer value than the
/// small storage type does.
#define FUNC_HEADER_FIELDS(V)                    \
  /* first word */                               \
  V(uint32_t, uint32_t, offset, 25)              \
  V(uint32_t, uint32_t, paramCount, 7)           \
  /* second word */                              \
  V(uint32_t, uint32_t, bytecodeSizeInBytes, 15) \
  V(uint32_t, uint32_t, functionName, 17)        \
  /* third word */                               \
  V(uint32_t, uint32_t, infoOffset, 25)          \
  V(uint32_t, uint32_t, frameSize, 7)            \
  /* fourth word, with flags below */            \
  V(uint32_t, uint8_t, environmentSize, 8)       \
  V(uint8_t, uint8_t, highestReadCacheIndex, 8)  \
  V(uint8_t, uint8_t, highestWriteCacheIndex, 8)

/**
 * Metadata of a function.
 */
struct FunctionHeader {
// Use api type here since FunctionHeader stores the full type.
#define DECLARE_FIELD(api_type, store_type, name, bits) api_type name;
  FUNC_HEADER_FIELDS(DECLARE_FIELD)
#undef DECLARE_FIELD

  FunctionHeaderFlag flags{};

 public:
  FunctionHeader(
      uint32_t size,
      uint32_t paramCount,
      uint32_t frameSize,
      uint32_t envSize,
      uint32_t functionNameID,
      uint8_t hiRCacheIndex,
      uint8_t hiWCacheIndex)
      : offset(0),
        paramCount(paramCount),
        bytecodeSizeInBytes(size),
        functionName(functionNameID),
        infoOffset(0),
        frameSize(frameSize),
        environmentSize(envSize),
        highestReadCacheIndex(hiRCacheIndex),
        highestWriteCacheIndex(hiWCacheIndex) {}
};

/// Compact version of FunctionHeader. Fits most functions.
/// Has two possible states, indicated by 'overflowed' flag:
/// !overflowed: all fields are valid.
/// overflowed: only flags and getLargeHeaderOffset() are valid,
///             and at the latter is a FunctionHeader.
/// Note that msvc and compatible compilers will not put bitfields
/// of the same type in the same memory, so don't mix uint8_t and
/// uint32_t if you want them packed next to each other.

struct SmallFuncHeader {
// Use the store_type since SmallFuncHeader attempts to minimize storage.
#define DECLARE_BITFIELD(api_type, store_type, name, bits) \
  store_type name : bits;
  FUNC_HEADER_FIELDS(DECLARE_BITFIELD)
#undef DECLARE_BITFIELD

  FunctionHeaderFlag flags{};

  /// Make a small header equivalent to 'large' if all values fit,
  /// else set overflowed with large.infoOffset as large's offset.
  SmallFuncHeader(const FunctionHeader &large) {
    std::memset(this, 0, sizeof(SmallFuncHeader)); // Avoid leaking junk.
    flags = large.flags;
#define CHECK_COPY_FIELD(api_type, store_type, name, bits) \
  if (large.name > (1U << bits) - 1) {                     \
    setLargeHeaderOffset(large.infoOffset);                \
    return;                                                \
  }                                                        \
  name = large.name;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
    FUNC_HEADER_FIELDS(CHECK_COPY_FIELD)
#pragma GCC diagnostic pop
#undef CHECK_COPY_FIELD
    assert(!flags.overflowed);
  }

  void setLargeHeaderOffset(uint32_t largeHeaderOffset) {
    flags.overflowed = true;
    // Can use any fields to store the large offset; pick two big ones.
    offset = largeHeaderOffset & 0xffff;
    infoOffset = largeHeaderOffset >> 16;
  }

  uint32_t getLargeHeaderOffset() const {
    assert(flags.overflowed);
    return (infoOffset << 16) | offset;
  }
};

// Sizes of file and function headers are tuned for good cache line packing.
// If you change their size, try to avoid headers crossing cache lines.
static_assert(
    sizeof(BytecodeFileHeader) % 32 == 0,
    "BytecodeFileHeader size should be cache friendly");

static_assert(
    32 % sizeof(SmallFuncHeader) == 0,
    "SmallFuncHeader size should be cache friendly");

struct ExceptionHandlerTableHeader {
  uint32_t count;
};

/// We need HBCExceptionHandlerInfo other than using ExceptionHandlerInfo
/// directly because we don't need depth in HBC.
struct HBCExceptionHandlerInfo {
  uint32_t start;
  uint32_t end;
  uint32_t target;
};

// The size of the file table and debug data.
struct DebugInfoHeader {
  // Number of filenames stored in the table.
  uint32_t filenameCount;
  // Bytes in the filename storage contents.
  uint32_t filenameStorageSize;

  // Count of the file table.
  uint32_t fileRegionCount;
  // Byte offset in the debug data for the lexical data.
  uint32_t lexicalDataOffset;
  // Size in bytes of the debug data.
  uint32_t debugDataSize;
};

// The string id of files for given offsets in debug info.
struct DebugFileRegion {
  uint32_t fromAddress;
  uint32_t filenameId;
  uint32_t sourceMappingUrlId;
};

LLVM_PACKED_END

/// BytecodeFileFields represents direct byte-level access to the structured
/// fields of a bytecode file, providing pointers and ArrayRefs referencing
/// directly into the buffer. Note some portions of the bytecode file are less
/// structured, such as the function info section; these are not exposed here.
/// Most clients will want to use an immutable BytecodeFileFields, which may be
/// initialized from a read-only buffer. Tools that want to modify the fields
/// in-place may initialize with Mutable=true.
template <bool Mutable>
struct BytecodeFileFields {
  template <typename T>
  using Pointer = typename std::conditional<Mutable, T *, const T *>::type;

  template <typename T>
  using Array = typename std::
      conditional<Mutable, llvm::MutableArrayRef<T>, llvm::ArrayRef<T>>::type;

  /// The file header.
  Pointer<BytecodeFileHeader> header{nullptr};

  /// List of function headers. Some of these may be overflow headers.
  Array<hbc::SmallFuncHeader> functionHeaders;

  /// The list of short string table entries.
  Array<hbc::SmallStringTableEntry> stringTableEntries{};

  /// The list of identifier hashes.
  Array<uint32_t> identifierHashes{};

  /// The list of overflowed string table entries.
  Array<hbc::OverflowStringTableEntry> stringTableOverflowEntries{};

  /// The character buffer used for string storage.
  Array<char> stringStorage;

  /// Buffer for array literals.
  Array<uint8_t> arrayBuffer;

  /// Buffer for object keys.
  Array<uint8_t> objKeyBuffer;

  /// Buffer for object values.
  Array<uint8_t> objValueBuffer;

  /// List of regexp literals.
  Array<RegExpTableEntry> regExpTable;

  /// Storage for regexp bytecode.
  Array<uint8_t> regExpStorage;

  /// List of CJS modules.
  Array<std::pair<uint32_t, uint32_t>> cjsModuleTable;

  /// List of resolved CJS modules.
  Array<uint32_t> cjsModuleTableStatic;

  /// Populate bytecode file fields from a buffer. The fields will point
  /// directly into the buffer and it is the caller's responsibility to ensure
  /// the result does not outlive the buffer.
  /// \p form contains the expected bytecode form (Execution or Delta).
  /// \return true on success, false on
  /// failure, in which case an error is returned by reference.
  bool populateFromBuffer(
      Array<uint8_t> bytes,
      std::string *outError,
      BytecodeForm form = BytecodeForm::Execution);
};

using ConstBytecodeFileFields = BytecodeFileFields<false>;
using MutableBytecodeFileFields = BytecodeFileFields<true>;

} // namespace hbc
} // namespace hermes

#endif // HERMES_BCGEN_HBC_BYTECODEFILEFORMAT_H
