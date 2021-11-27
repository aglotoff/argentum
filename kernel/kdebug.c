#include <dwarf.h>
#include <stddef.h>

#include "console.h"
#include "kdebug.h"

static int scan_aranges(uintptr_t, struct PcDebugInfo *); 
static int parse_cu(uint8_t *, uintptr_t, struct PcDebugInfo *);
static int get_debug_line_info(uint8_t *, uintptr_t, struct PcDebugInfo *);

static uint16_t get_uhalf(uint8_t *, uint8_t **);
static uint32_t get_uword(uint8_t *, uint8_t **);
static uint32_t get_uleb128(uint8_t *, uint8_t **);
static int32_t  get_sleb128(uint8_t *, uint8_t **);

int
debug_info_pc(uintptr_t pc, struct PcDebugInfo *info)
{
  info->file = "<uknown>";
  info->line = 0;
  info->fn_name = "<unknown>";
  info->fn_addr = pc;

  return scan_aranges(pc, info);
}

/*
 * ----------------------------------------------------------------------------
 * Lookup by address
 * ----------------------------------------------------------------------------
 */

extern uint8_t __debug_aranges_begin__[], __debug_aranges_end__[];

/*
 * Scan the address range table for compilation unit that contains the given
 * address.
 */
static int
scan_aranges(uintptr_t addr, struct PcDebugInfo *info)
{
  struct DebugArangesHeader *header;
  struct DebugArangesEntry *e;
  uint8_t *ptr;
  
  ptr = __debug_aranges_begin__;
  while (ptr < __debug_aranges_end__) {
    header = (struct DebugArangesHeader *) ptr;
 
    ptr = (uint8_t *) (header + 1) + sizeof(uint32_t);

    for (e = (struct DebugArangesEntry *) ptr; e->addr || e->length; e++) {
      if (addr >= e->addr && addr < (e->addr + e->length))
        return parse_cu((uint8_t *) header->offset, addr, info);
    }

    ptr = (uint8_t *) (header) + header->length + sizeof header->length;
  }

  return -1;
}

/*
 * ----------------------------------------------------------------------------
 * Compilation units
 * ----------------------------------------------------------------------------
 */

union AttrValue {
  struct {
    void  *data;
    size_t length;
  } buf;
  uint32_t  num;
  char     *str;
};

extern uint8_t __debug_abbrev_begin__[], __debug_abbrev_end__[];

static uint8_t *scan_abbrev_table(uint8_t *, uint32_t);
static int get_attr_value(uint32_t, union AttrValue *, uint8_t *, uint8_t **);
static char *get_debug_str(uint8_t *);

static int
parse_cu(uint8_t *ptr, uintptr_t addr, struct PcDebugInfo *info)
{
  struct CompileUnitHeader* header;
  uint8_t *end, *abbrev;
  uint32_t code, tag;
  uintptr_t fn_hi, fn_lo;
  char *fn_name;
  uint32_t attr_name, attr_form;
  union AttrValue val;

  header = (struct CompileUnitHeader *) ptr;

  ptr = (uint8_t *) (header + 1);
  end = (uint8_t *) header + header->length - sizeof(header->length);

  while (ptr < end) {
    fn_name = NULL;
    fn_hi = fn_lo = 0;

    code = get_uleb128(ptr, &ptr);
    if (code == 0)
      continue;

    abbrev = scan_abbrev_table((uint8_t *) header->abbrev_offset, code);

    tag = get_uleb128(abbrev, &abbrev);
    abbrev++;

    // Parse attributes
    for (;;) {
      attr_name = get_uleb128(abbrev, &abbrev);
      attr_form = get_uleb128(abbrev, &abbrev);

      if (!attr_name && !attr_form)
        break;

      if (get_attr_value(attr_form, &val, ptr, &ptr) < 0)
        return -1;

      if (tag == DW_TAG_COMPILE_UNIT) {
        switch (attr_name) {
        case DW_AT_NAME:
          info->file = val.str;
          break;
        case DW_AT_STMT_LIST:
          get_debug_line_info((uint8_t *) val.num, addr, info);
          break;
        }
      } else if (tag == DW_TAG_SUBPROGRAM) {
        switch (attr_name) {
        case DW_AT_NAME:
          fn_name = val.str;
          break;
        case DW_AT_LOW_PC:
          fn_lo = val.num;
          break;
        case DW_AT_HIGH_PC:
          fn_hi = val.num;
          break;
        }
      }
    }

    if (addr >= fn_lo && addr < fn_hi) {
      info->fn_name = fn_name;
      info->fn_addr = fn_lo;
    }
  }

  return 0;
}

/*
 * Find abbreviation declaration for the given code.
 */
static uint8_t *
scan_abbrev_table(uint8_t *ptr, uint32_t code)
{
  uint32_t decl_code, attr_name, attr_form;

  if (ptr < __debug_abbrev_begin__)
    return NULL;

  while (ptr < __debug_abbrev_end__) {
    decl_code = get_uleb128(ptr, &ptr);

    // End of abbreviations for a given compilation unit
    if (decl_code == 0)
      return NULL;

    if (decl_code == code)
      return ptr;

    // Skip tag, children determination, and attribute encodings
    get_uleb128(ptr, &ptr);
    ptr++;
    do {
      attr_name = get_uleb128(ptr, &ptr);
      attr_form = get_uleb128(ptr, &ptr);
    } while (attr_name || attr_form);
  }

  return NULL;
}

/*
 * Decode attribute value
 */
static int
get_attr_value(uint32_t form, union AttrValue *value,
               uint8_t *ptr, uint8_t **endptr)
{
  switch (form) {
  case DW_FORM_BLOCK1:
    value->buf.length = *ptr++;
    value->buf.data = ptr;
    ptr += value->buf.length;
    break;
  case DW_FORM_BLOCK2:
    value->buf.length = get_uhalf(ptr, &ptr);
    value->buf.data = ptr;
    ptr += value->buf.length;
    break;
  case DW_FORM_BLOCK4:
    value->buf.length = get_uword(ptr, &ptr);
    value->buf.data = ptr;
    ptr += value->buf.length;
    break;
  case DW_FORM_BLOCK:
    value->buf.length = get_uleb128(ptr, &ptr);
    value->buf.data = ptr;
    ptr += value->buf.length;
    break;
  case DW_FORM_REF1:
  case DW_FORM_DATA1:
  case DW_FORM_FLAG:
    value->num = *ptr++;
    break;
  case DW_FORM_REF2:
  case DW_FORM_DATA2:
    value->num = get_uhalf(ptr, &ptr);
    break;
  case DW_FORM_REF4:
  case DW_FORM_REF8:
  case DW_FORM_DATA4:
  case DW_FORM_DATA8:
  case DW_FORM_ADDR:
    value->num = get_uword(ptr, &ptr);
    break;
  case DW_FORM_SDATA:
    value->num = get_sleb128(ptr, &ptr);
    break;
  case DW_FORM_UDATA:
  case DW_FORM_REF_UDATA:
    value->num = get_uleb128(ptr, &ptr);
    break;
  case DW_FORM_STRING:
    value->str = (char *) ptr;
    while (*ptr++)
      ;
    break;
  case DW_FORM_STRP:
    value->str = get_debug_str((uint8_t *) get_uword(ptr, &ptr));
    break;
  default:
    return -1;
  }

  if (endptr)
    *endptr = ptr;

  return 0;
}

/*
 * ----------------------------------------------------------------------------
 * String table
 * ----------------------------------------------------------------------------
 */

extern uint8_t __debug_str_begin__[], __debug_str_end__[];

static char *
get_debug_str(uint8_t *ptr)
{
  if (ptr < __debug_str_begin__ || ptr >= __debug_str_end__)
    return NULL;
  return (char *) ptr;
}

/*
 * ----------------------------------------------------------------------------
 * Line number information
 * ----------------------------------------------------------------------------
 */

extern uint8_t __debug_line_begin__[], __debug_line_end__[];

static int
get_debug_line_info(uint8_t *ptr, uintptr_t pc, struct PcDebugInfo *info)
{
  uint32_t unit_length, header_length;
  uint8_t opcode, min_instruction_length, line_range, opcode_base;
  int8_t line_base;
  uint8_t *program_begin, *program_end;
  uintptr_t address;
  int line;

  if (ptr < __debug_line_begin__ || ptr >= __debug_line_end__)
    return -1;

  // Determine the end of the line number information
  unit_length = get_uword(ptr, &ptr);
  program_end = ptr + unit_length;

  get_uhalf(ptr, &ptr); // skip version

  // Determine the beginning of the line number program
  header_length = get_uword(ptr, &ptr);
  program_begin = ptr + header_length;

  min_instruction_length = *ptr++;
  ptr++;  // skip default_is_stmt
  line_base = *ptr++;
  line_range = *ptr++;
  opcode_base = *ptr++;

  // Execute the line number program
  address = 0;
  line = 1;
  for (ptr = program_begin; ptr < program_end; ) {
    opcode = *ptr++;

    switch(opcode) {
    // Standard Opcodes
    case DW_LNS_COPY:
      // basic_block = prologue_end = epilogue_begin = 0;
      break;
    case DW_LNS_ADVANCE_PC:
      address += get_uleb128(ptr, &ptr) * min_instruction_length;
      break;
    case DW_LNS_ADVANCE_LINE:
      line += get_sleb128(ptr, &ptr);
      break;
    case DW_LNS_SET_FILE:
      // file = get_uleb128(ptr, &ptr);
      get_uleb128(ptr, &ptr);
      break;
    case DW_LNS_SET_COLUMN:
      // column = get_uleb128(ptr, &ptr);
      get_uleb128(ptr, &ptr);
      break;
    case DW_LNS_NEGATE_STMT:
      // is_stmt = !is_stmt;
      break;
    case DW_LNS_SET_BASIC_BLOCK:
      // basic_block = 1;
      break;
    case DW_LNS_CONST_ADD_PC:
      address += ((255 - opcode_base) / line_range) * min_instruction_length;
      break;
    case DW_LNS_FIXED_ADVANCE_PC:
      address += get_uhalf(ptr, &ptr);
      break;
    case DW_LNS_SET_PROLOGUE_END:
      // prologue_end = 1;
      break;
    case DW_LNS_SET_EPILOGUE_BEGIN:
      // epilogue_begin = 1;
      break;
    case DW_LNS_SET_ISA:
      // isa = get_uleb128(ptr, &ptr);
      get_uleb128(ptr, &ptr);
      break;

    // Extended Opcodes
    case 0:
      // ext_length = get_uleb128(ptr, &ptr);
      get_uleb128(ptr, &ptr);

      switch (*ptr++) {
      case DW_LNE_END_SEQUENCE:
        address = 0;
        line = 1;
        break;
      case DW_LNE_SET_ADDRESS:
        address = get_uword(ptr, &ptr);
        break;
      case DW_LNE_DEFINE_FILE:
        while (*ptr++)
          ;
        get_uleb128(ptr, &ptr);
        get_uleb128(ptr, &ptr);
        get_uleb128(ptr, &ptr);
        break;
      }
      break;

    // Special Opcodes
    default:
      address += ((opcode - opcode_base) / line_range) * min_instruction_length;
      line += line_base + ((opcode - opcode_base) % line_range);
    }

    if (address >= pc) {
      info->line = line;
      return 0;
    }
  }

  return -1;
}

/*
 * ----------------------------------------------------------------------------
 * Decode data
 * ----------------------------------------------------------------------------
 */

/*
 * Get a (probably misaligned) unsigned 2-byte value.
 */
static uint16_t
get_uhalf(uint8_t *ptr, uint8_t **endptr)
{
  uint16_t result;
  
  result  = *ptr++;
  result |= *ptr++ << 8;

  if (endptr)
    *endptr = ptr;

  return result;
}

/*
 * Get a (probably misaligned) unsigned 4-byte value.
 */
static uint32_t
get_uword(uint8_t *ptr, uint8_t **endptr)
{
  uint32_t result;

  result  = *ptr++;
  result |= *ptr++ << 8;
  result |= *ptr++ << 16;
  result |= *ptr++ << 24;

  if (endptr)
    *endptr = ptr;

  return result;
}

/*
 * Decode unsigned LEB128 number.
 */
static uint32_t
get_uleb128(uint8_t *ptr, uint8_t **endptr)
{
  uint32_t result = 0;
  uint8_t byte;
  unsigned shift = 0;

  for (;;) {
    byte = *ptr++;
    result |= (byte & 0x7F) << shift;
    if (!(byte & 0x80))
      break;
    shift += 7;
  }

  if (endptr)
    *endptr = ptr;

  return result;
}

/*
 * Decode signed LEB128 number.
 */
static int32_t
get_sleb128(uint8_t *ptr, uint8_t **endptr)
{
  int32_t result = 0;
  int8_t byte;
  unsigned shift = 0;

  for (;;) {
    byte = *ptr++;
    result |= (byte & 0x7F) << shift;
    shift += 7;
    if (!(byte & 0x80))
      break;
  }

  if ((shift < (8 * sizeof(int32_t))) && (byte & 0x40))
    // sign extend
    result |= -(1 << shift);

  if (endptr)
    *endptr = ptr;

  return result;
}
