#include "generic/stage2/json_iterator.h"
#include "generic/stage2/tape_writer.h"
#include "generic/stage2/atomparsing.h"

namespace {
namespace SIMDJSON_IMPLEMENTATION {
namespace stage2 {

struct tape_builder {
  template<bool STREAMING>
  WARN_UNUSED static really_inline error_code parse_document(
    dom_parser_implementation &dom_parser,
    dom::document &doc) noexcept;

  /** Called when a non-empty document starts. */
  WARN_UNUSED really_inline error_code visit_document_start(json_iterator &iter) noexcept;
  /** Called when a non-empty document ends without error. */
  WARN_UNUSED really_inline error_code visit_document_end(json_iterator &iter) noexcept;

  /** Called when a non-empty array starts. */
  WARN_UNUSED really_inline error_code visit_array_start(json_iterator &iter) noexcept;
  /** Called when a non-empty array ends. */
  WARN_UNUSED really_inline error_code visit_array_end(json_iterator &iter) noexcept;
  /** Called when an empty array is found. */
  WARN_UNUSED really_inline error_code visit_empty_array(json_iterator &iter) noexcept;

  /** Called when a non-empty object starts. */
  WARN_UNUSED really_inline error_code visit_object_start(json_iterator &iter) noexcept;
  /**
   * Called when a key in a field is encountered.
   *
   * primitive, visit_object_start, visit_empty_object, visit_array_start, or visit_empty_array
   * will be called after this with the field value.
   */
  WARN_UNUSED really_inline error_code visit_key(json_iterator &iter, const uint8_t *key) noexcept;
  /** Called when a non-empty object ends. */
  WARN_UNUSED really_inline error_code visit_object_end(json_iterator &iter) noexcept;
  /** Called when an empty object is found. */
  WARN_UNUSED really_inline error_code visit_empty_object(json_iterator &iter) noexcept;

  /**
   * Called when a string, number, boolean or null is found.
   */
  WARN_UNUSED really_inline error_code visit_primitive(json_iterator &iter, const uint8_t *value) noexcept;
  /**
   * Called when a string, number, boolean or null is found at the top level of a document (i.e.
   * when there is no array or object and the entire document is a single string, number, boolean or
   * null.
   *
   * This is separate from primitive() because simdjson's normal primitive parsing routines assume
   * there is at least one more token after the value, which is only true in an array or object.
   */
  WARN_UNUSED really_inline error_code visit_root_primitive(json_iterator &iter, const uint8_t *value) noexcept;

  /** Called each time a new field or element in an array or object is found. */
  WARN_UNUSED really_inline error_code increment_count(json_iterator &iter) noexcept;

private:
  /** Next location to write to tape */
  tape_writer tape;
  /** Next write location in the string buf for stage 2 parsing */
  uint8_t *current_string_buf_loc;

  really_inline tape_builder(dom::document &doc) noexcept;

  WARN_UNUSED really_inline error_code visit_string(json_iterator &iter, const uint8_t *value, bool key = false) noexcept;
  WARN_UNUSED really_inline error_code visit_number(json_iterator &iter, const uint8_t *value) noexcept;
  WARN_UNUSED really_inline error_code visit_root_number(json_iterator &iter, const uint8_t *value) noexcept;
  WARN_UNUSED really_inline error_code visit_true_atom(json_iterator &iter, const uint8_t *value) noexcept;
  WARN_UNUSED really_inline error_code visit_root_true_atom(json_iterator &iter, const uint8_t *value) noexcept;
  WARN_UNUSED really_inline error_code visit_false_atom(json_iterator &iter, const uint8_t *value) noexcept;
  WARN_UNUSED really_inline error_code visit_root_false_atom(json_iterator &iter, const uint8_t *value) noexcept;
  WARN_UNUSED really_inline error_code visit_null_atom(json_iterator &iter, const uint8_t *value) noexcept;
  WARN_UNUSED really_inline error_code visit_root_null_atom(json_iterator &iter, const uint8_t *value) noexcept;

  really_inline uint32_t next_tape_index(json_iterator &iter) const noexcept;
  really_inline void start_container(json_iterator &iter) noexcept;
  WARN_UNUSED really_inline error_code end_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept;
  WARN_UNUSED really_inline error_code empty_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept;
  really_inline uint8_t *on_start_string(json_iterator &iter) noexcept;
  really_inline void on_end_string(uint8_t *dst) noexcept;
}; // class tape_builder

template<bool STREAMING>
WARN_UNUSED really_inline error_code tape_builder::parse_document(
    dom_parser_implementation &dom_parser,
    dom::document &doc) noexcept {
  dom_parser.doc = &doc;
  json_iterator iter(dom_parser, STREAMING ? dom_parser.next_structural_index : 0);
  tape_builder builder(doc);
  return iter.walk_document<STREAMING>(builder);
}

WARN_UNUSED really_inline error_code tape_builder::visit_root_primitive(json_iterator &iter, const uint8_t *value) noexcept {
  switch (*value) {
    case '"': return visit_string(iter, value);
    case 't': return visit_root_true_atom(iter, value);
    case 'f': return visit_root_false_atom(iter, value);
    case 'n': return visit_root_null_atom(iter, value);
    case '-':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      return visit_root_number(iter, value);
    default:
      iter.log_error("Document starts with a non-value character");
      return TAPE_ERROR;
  }
}
WARN_UNUSED really_inline error_code tape_builder::visit_primitive(json_iterator &iter, const uint8_t *value) noexcept {
  switch (*value) {
    case '"': return visit_string(iter, value);
    case 't': return visit_true_atom(iter, value);
    case 'f': return visit_false_atom(iter, value);
    case 'n': return visit_null_atom(iter, value);
    case '-':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      return visit_number(iter, value);
    default:
      iter.log_error("Non-value found when value was expected!");
      return TAPE_ERROR;
  }
}
WARN_UNUSED really_inline error_code tape_builder::visit_empty_object(json_iterator &iter) noexcept {
  return empty_container(iter, internal::tape_type::START_OBJECT, internal::tape_type::END_OBJECT);
}
WARN_UNUSED really_inline error_code tape_builder::visit_empty_array(json_iterator &iter) noexcept {
  return empty_container(iter, internal::tape_type::START_ARRAY, internal::tape_type::END_ARRAY);
}

WARN_UNUSED really_inline error_code tape_builder::visit_document_start(json_iterator &iter) noexcept {
  start_container(iter);
  return SUCCESS;
}
WARN_UNUSED really_inline error_code tape_builder::visit_object_start(json_iterator &iter) noexcept {
  start_container(iter);
  return SUCCESS;
}
WARN_UNUSED really_inline error_code tape_builder::visit_array_start(json_iterator &iter) noexcept {
  start_container(iter);
  return SUCCESS;
}

WARN_UNUSED really_inline error_code tape_builder::visit_object_end(json_iterator &iter) noexcept {
  return end_container(iter, internal::tape_type::START_OBJECT, internal::tape_type::END_OBJECT);
}
WARN_UNUSED really_inline error_code tape_builder::visit_array_end(json_iterator &iter) noexcept {
  return end_container(iter, internal::tape_type::START_ARRAY, internal::tape_type::END_ARRAY);
}
WARN_UNUSED really_inline error_code tape_builder::visit_document_end(json_iterator &iter) noexcept {
  constexpr uint32_t start_tape_index = 0;
  tape.append(start_tape_index, internal::tape_type::ROOT);
  tape_writer::write(iter.dom_parser.doc->tape[start_tape_index], next_tape_index(iter), internal::tape_type::ROOT);
  return SUCCESS;
}
WARN_UNUSED really_inline error_code tape_builder::visit_key(json_iterator &iter, const uint8_t *key) noexcept {
  return visit_string(iter, key, true);
}

WARN_UNUSED really_inline error_code tape_builder::increment_count(json_iterator &iter) noexcept {
  iter.dom_parser.open_containers[iter.depth].count++; // we have a key value pair in the object at parser.dom_parser.depth - 1
  return SUCCESS;
}

really_inline tape_builder::tape_builder(dom::document &doc) noexcept : tape{doc.tape.get()}, current_string_buf_loc{doc.string_buf.get()} {}

WARN_UNUSED really_inline error_code tape_builder::visit_string(json_iterator &iter, const uint8_t *value, bool key) noexcept {
  iter.log_value(key ? "key" : "string");
  uint8_t *dst = on_start_string(iter);
  dst = stringparsing::parse_string(value, dst);
  if (dst == nullptr) {
    iter.log_error("Invalid escape in string");
    return STRING_ERROR;
  }
  on_end_string(dst);
  return SUCCESS;
}

WARN_UNUSED really_inline error_code tape_builder::visit_number(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("number");
  if (!numberparsing::parse_number(value, tape)) { iter.log_error("Invalid number"); return NUMBER_ERROR; }
  return SUCCESS;
}

WARN_UNUSED really_inline error_code tape_builder::visit_root_number(json_iterator &iter, const uint8_t *value) noexcept {
  //
  // We need to make a copy to make sure that the string is space terminated.
  // This is not about padding the input, which should already padded up
  // to len + SIMDJSON_PADDING. However, we have no control at this stage
  // on how the padding was done. What if the input string was padded with nulls?
  // It is quite common for an input string to have an extra null character (C string).
  // We do not want to allow 9\0 (where \0 is the null character) inside a JSON
  // document, but the string "9\0" by itself is fine. So we make a copy and
  // pad the input with spaces when we know that there is just one input element.
  // This copy is relatively expensive, but it will almost never be called in
  // practice unless you are in the strange scenario where you have many JSON
  // documents made of single atoms.
  //
  uint8_t *copy = static_cast<uint8_t *>(malloc(iter.remaining_len() + SIMDJSON_PADDING));
  if (copy == nullptr) { return MEMALLOC; }
  memcpy(copy, value, iter.remaining_len());
  memset(copy + iter.remaining_len(), ' ', SIMDJSON_PADDING);
  error_code error = visit_number(iter, copy);
  free(copy);
  return error;
}

WARN_UNUSED really_inline error_code tape_builder::visit_true_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("true");
  if (!atomparsing::is_valid_true_atom(value)) { return T_ATOM_ERROR; }
  tape.append(0, internal::tape_type::TRUE_VALUE);
  return SUCCESS;
}

WARN_UNUSED really_inline error_code tape_builder::visit_root_true_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("true");
  if (!atomparsing::is_valid_true_atom(value, iter.remaining_len())) { return T_ATOM_ERROR; }
  tape.append(0, internal::tape_type::TRUE_VALUE);
  return SUCCESS;
}

WARN_UNUSED really_inline error_code tape_builder::visit_false_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("false");
  if (!atomparsing::is_valid_false_atom(value)) { return F_ATOM_ERROR; }
  tape.append(0, internal::tape_type::FALSE_VALUE);
  return SUCCESS;
}

WARN_UNUSED really_inline error_code tape_builder::visit_root_false_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("false");
  if (!atomparsing::is_valid_false_atom(value, iter.remaining_len())) { return F_ATOM_ERROR; }
  tape.append(0, internal::tape_type::FALSE_VALUE);
  return SUCCESS;
}

WARN_UNUSED really_inline error_code tape_builder::visit_null_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("null");
  if (!atomparsing::is_valid_null_atom(value)) { return N_ATOM_ERROR; }
  tape.append(0, internal::tape_type::NULL_VALUE);
  return SUCCESS;
}

WARN_UNUSED really_inline error_code tape_builder::visit_root_null_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("null");
  if (!atomparsing::is_valid_null_atom(value, iter.remaining_len())) { return N_ATOM_ERROR; }
  tape.append(0, internal::tape_type::NULL_VALUE);
  return SUCCESS;
}

// private:

really_inline uint32_t tape_builder::next_tape_index(json_iterator &iter) const noexcept {
  return uint32_t(tape.next_tape_loc - iter.dom_parser.doc->tape.get());
}

WARN_UNUSED really_inline error_code tape_builder::empty_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept {
  auto start_index = next_tape_index(iter);
  tape.append(start_index+2, start);
  tape.append(start_index, end);
  return SUCCESS;
}

really_inline void tape_builder::start_container(json_iterator &iter) noexcept {
  iter.dom_parser.open_containers[iter.depth].tape_index = next_tape_index(iter);
  iter.dom_parser.open_containers[iter.depth].count = 0;
  tape.skip(); // We don't actually *write* the start element until the end.
}

WARN_UNUSED really_inline error_code tape_builder::end_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept {
  // Write the ending tape element, pointing at the start location
  const uint32_t start_tape_index = iter.dom_parser.open_containers[iter.depth].tape_index;
  tape.append(start_tape_index, end);
  // Write the start tape element, pointing at the end location (and including count)
  // count can overflow if it exceeds 24 bits... so we saturate
  // the convention being that a cnt of 0xffffff or more is undetermined in value (>=  0xffffff).
  const uint32_t count = iter.dom_parser.open_containers[iter.depth].count;
  const uint32_t cntsat = count > 0xFFFFFF ? 0xFFFFFF : count;
  tape_writer::write(iter.dom_parser.doc->tape[start_tape_index], next_tape_index(iter) | (uint64_t(cntsat) << 32), start);
  return SUCCESS;
}

really_inline uint8_t *tape_builder::on_start_string(json_iterator &iter) noexcept {
  // we advance the point, accounting for the fact that we have a NULL termination
  tape.append(current_string_buf_loc - iter.dom_parser.doc->string_buf.get(), internal::tape_type::STRING);
  return current_string_buf_loc + sizeof(uint32_t);
}

really_inline void tape_builder::on_end_string(uint8_t *dst) noexcept {
  uint32_t str_length = uint32_t(dst - (current_string_buf_loc + sizeof(uint32_t)));
  // TODO check for overflow in case someone has a crazy string (>=4GB?)
  // But only add the overflow check when the document itself exceeds 4GB
  // Currently unneeded because we refuse to parse docs larger or equal to 4GB.
  memcpy(current_string_buf_loc, &str_length, sizeof(uint32_t));
  // NULL termination is still handy if you expect all your strings to
  // be NULL terminated? It comes at a small cost
  *dst = 0;
  current_string_buf_loc = dst + 1;
}

} // namespace stage2
} // namespace SIMDJSON_IMPLEMENTATION
} // unnamed namespace
