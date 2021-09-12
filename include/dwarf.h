#ifndef INCLUDE_DWARF_H
#define INCLUDE_DWARF_H

#include <stdint.h>

// Attribute form encodings
#define DW_FORM_ADDR                0x01
#define DW_FORM_BLOCK2              0x03
#define DW_FORM_BLOCK4              0x04
#define DW_FORM_DATA2               0x05
#define DW_FORM_DATA4               0x06
#define DW_FORM_DATA8               0x07
#define DW_FORM_STRING              0x08
#define DW_FORM_BLOCK               0x09
#define DW_FORM_BLOCK1              0x0A
#define DW_FORM_DATA1               0x0B
#define DW_FORM_FLAG                0x0C
#define DW_FORM_SDATA               0x0D
#define DW_FORM_STRP                0x0E
#define DW_FORM_UDATA               0x0F
#define DW_FORM_REF_ADDR            0x10
#define DW_FORM_REF1                0x11
#define DW_FORM_REF2                0x12
#define DW_FORM_REF4                0x13
#define DW_FORM_REF8                0x14
#define DW_FORM_REF_UDATA           0x15
#define DW_FORM_INDIRECT            0x16

// Tag encodings
#define DW_TAG_ARRAY_TYPE               0x01
#define DW_TAG_CLASS_TYPE               0x02
#define DW_TAG_ENTRY_POINT              0x03
#define DW_TAG_ENUMERATION_TYPE         0x04
#define DW_TAG_FORMAL_PARAMETER         0x05
#define DW_TAG_IMPORTED_DECLARATION     0x08
#define DW_TAG_LABEL                    0x0A
#define DW_TAG_LEXICAL_BLOCK            0x0B
#define DW_TAG_MEMBER                   0x0D
#define DW_TAG_POINTER_TYPE             0x0F
#define DW_TAG_REFERENCE_TYPE           0x10
#define DW_TAG_COMPILE_UNIT             0x11
#define DW_TAG_STRING_TYPE              0x12
#define DW_TAG_STRUCTURE_TYPE           0x13
#define DW_TAG_SUBROUTINE_TYPE          0x15
#define DW_TAG_TYPEDEF                  0x16
#define DW_TAG_UNION_TYPE               0x17
#define DW_TAG_UNSPECIFIED_PARAMETERS   0x18
#define DW_TAG_VARIANT                  0x19
#define DW_TAG_COMMON_BLOCK             0x1A
#define DW_TAG_COMMON_INCLUSION         0x1B
#define DW_TAG_INHERITANCE              0x1C
#define DW_TAG_INLINED_SUBROUTINE       0x1D
#define DW_TAG_MODULE                   0x1E
#define DW_TAG_PTR_TO_MEMBER_TYPE       0x1F
#define DW_TAG_SET_TYPE                 0x20
#define DW_TAG_SUBRANGE_TYPE            0x21
#define DW_TAG_WITH_STMT                0x22
#define DW_TAG_ACCESS_DECLARATION       0x23
#define DW_TAG_BASE_TYPE                0x24
#define DW_TAG_CATCH_BLOCK              0x25
#define DW_TAG_CONST_TYPE               0x26
#define DW_TAG_CONSTANT                 0x27
#define DW_TAG_ENUMERATOR               0x28
#define DW_TAG_FILE_TYPE                0x29
#define DW_TAG_FRIEND                   0x2A
#define DW_TAG_NAMELIST                 0x2B
#define DW_TAG_NAMELIST_ITEM            0x2C
#define DW_TAG_PACKED_TYPE              0x2D
#define DW_TAG_SUBPROGRAM               0x2E
#define DW_TAG_TEMPLATE_TYPE_PARAM      0x2F
#define DW_TAG_TEMPLATE_VALUE_PARAM     0x30
#define DW_TAG_THROWN_TYPE              0x31
#define DW_TAG_TRY_BLOCK                0x32
#define DW_TAG_VARIANT_PART             0x33
#define DW_TAG_VARIABLE                 0x34
#define DW_TAG_VOLATILE_TYPE            0x35
#define DW_TAG_LO_USER                  0x4080
#define DW_TAG_HI_USER                  0xFFFF

// Child determination encodings
#define DW_CHILDREN_NO              0
#define DW_CHILDREN_YES             1

// Attribute encodings
#define DW_AT_SIBLING               0x01
#define DW_AT_LOCATION              0x02
#define DW_AT_NAME                  0x03
#define DW_AT_ORDERING              0x09
#define DW_AT_BYTE_SIZE             0x0B
#define DW_AT_BIT_OFFSET            0x0C
#define DW_AT_BIT_SIZE              0x0D
#define DW_AT_STMT_LIST             0x10
#define DW_AT_LOW_PC                0x11
#define DW_AT_HIGH_PC               0x12
#define DW_AT_LANGUAGE              0x13
#define DW_AT_DISCR                 0x15
#define DW_AT_DISCR_VALUE           0x16
#define DW_AT_VISIBILITY            0x17
#define DW_AT_IMPORT                0x18
#define DW_AT_STRING_LENGTH         0x19
#define DW_AT_COMMON_REFERENCE      0x1A
#define DW_AT_COMP_DIR              0x1B
#define DW_AT_CONST_VALUE           0x1C
#define DW_AT_CONTAINING_TYPE       0x1D
#define DW_AT_DEFAULT_VALUE         0x1E
#define DW_AT_INLINE                0x20
#define DW_AT_IS_OPTIONAL           0x21
#define DW_AT_LOWER_BOUND           0x22
#define DW_AT_PRODUCER              0x25
#define DW_AT_PROTOTYPED            0x27
#define DW_AT_RETURN_ADDR           0x2A
#define DW_AT_START_SCOPE           0x2C
#define DW_AT_STRIDE_SIZE           0x2E
#define DW_AT_UPPER_BOUND           0x2F

#define DW_LNS_COPY                 0x1
#define DW_LNS_ADVANCE_PC           0x2
#define DW_LNS_ADVANCE_LINE         0x3
#define DW_LNS_SET_FILE             0x4
#define DW_LNS_SET_COLUMN           0x5
#define DW_LNS_NEGATE_STMT          0x6
#define DW_LNS_SET_BASIC_BLOCK      0x7
#define DW_LNS_CONST_ADD_PC         0x8
#define DW_LNS_FIXED_ADVANCE_PC     0x9
#define DW_LNS_SET_PROLOGUE_END     0xA
#define DW_LNS_SET_EPILOGUE_BEGIN   0xB
#define DW_LNS_SET_ISA              0xC

#define DW_LNE_END_SEQUENCE         0x1
#define DW_LNE_SET_ADDRESS          0x2
#define DW_LNE_DEFINE_FILE          0x3

struct DebugArangesHeader {
  uint32_t length;
  uint16_t version;
  uint32_t offset;
  uint8_t  addr_size;
  uint8_t  seg_size;
} __attribute__((packed));

struct DebugArangesEntry {
  uint32_t addr;
  uint32_t length;
} __attribute__((packed));

struct CompileUnitHeader {
  uint32_t length;
  uint16_t version;
  uint32_t abbrev_offset;
  uint8_t  addr_size;
} __attribute__((packed));

struct DebugLinePrologue {
  uint32_t total_length;
  uint16_t version;
  uint32_t prologue_length;
  uint8_t  min_instruction_length;
  uint8_t  default_is_stmt;
  int8_t   line_base;
  uint8_t  line_range;
  uint8_t  opcode_base;
  uint8_t  std_opcode_lengths[];
} __attribute__((packed));

#endif  // !INCLUDE_DWARF_H
