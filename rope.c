#include "rope.h"

#include "byte_vector.h"
#include "pair.h"
#include "root.h"
#include "string.h"

// TODO: Make memory a global.

void AppendNewRopeString(struct Memory *memory, Object last_pair, s64 string_size, u8 character);
void AppendCharacterToRopeString(struct Memory *memory, Object rope_string, u8 character);
Object MakeNewRopeString(struct Memory *memory, s64 string_size, u8 character);
u8 *StringPointer(struct Memory *memory, Object rope_string);
Object LastCdr(struct Memory *memory, Object list);
s64 RopeStringLength(struct Memory *memory, Object rope_string);
b64 IsStringFull(struct Memory *memory, Object rope_string, s64 string_size);

Object MakeRope(struct Memory *memory, s64 string_size) {
  Object rope = AllocatePair(memory);
  SetCar(memory, UnboxReference(rope), BoxFixnum(string_size));
  SetCdr(memory, UnboxReference(rope), nil);
  return rope;
}

void AppendRope(struct Memory *memory, Object rope, u8 character) {
  s64 string_size = UnboxFixnum(Car(memory, UnboxReference(rope)));
  Object strings = Cdr(memory, UnboxReference(rope));
  if (strings == nil) {
    // this is the first character being added to the rope.
    AppendNewRopeString(memory, rope, string_size, character);
  } else {
    Object end = LastCdr(memory, strings);
    Object last_string = Car(memory, UnboxReference(end));
    if (IsStringFull(memory, last_string, string_size)) {
      // The last rope_string is full, need to allocate a new rope_string, and append it to the end of the rope.
      AppendNewRopeString(memory, end, string_size, character);
    } else {
      AppendCharacterToRopeString(memory, last_string, character);
    }
  }
}

Object FinalizeRope(struct Memory *memory, Object rope) {
}

// TODO: Move this shit.
u8 *StringPointer(struct Memory *memory, Object string) {
  return (u8 *)&memory->the_objects[UnboxReference(string) + 1];
}

// rope_string := (num_bytes . string_reference)
Object MakeNewRopeString(struct Memory *memory, s64 string_size, u8 character) {
  Object string = BoxString(AllocateByteVector(memory, string_size));

  u8 *str = StringPointer(memory, string);
  str[0] = character;
  str[1] = 0;

  return MakePair(memory, BoxFixnum(2), string);
}

void AppendNewRopeString(struct Memory *memory, Object last_pair, s64 string_size, u8 character) {

  SetRegister(memory, REGISTER_ROPE_SAVE, last_pair);
  Object pair = MakePair(memory, MakeNewRopeString(memory, string_size, character), nil);
  last_pair = GetRegister(memory, REGISTER_ROPE_SAVE);

  SetCdr(memory, UnboxReference(last_pair), pair);
}

s64 RopeStringLength(struct Memory *memory, Object rope_string) {
  return UnboxFixnum(Car(memory, UnboxReference(rope_string)));
}

void AppendCharacterToRopeString(struct Memory *memory, Object rope_string, u8 character) {
  s64 length = RopeStringLength(memory, rope_string);
  Object string = Cdr(memory, UnboxReference(rope_string));

  u8 *str = StringPointer(memory, string);
  str[length-1] = character;
  str[length] = 0;
  ++length;
}

Object LastCdr(struct Memory *memory, Object list) {
  for (Object rest = Cdr(memory, UnboxReference(list));
      rest != nil;
      list = rest, rest = Cdr(memory, UnboxReference(rest)))
    ;

  return list;
}

b64 IsStringFull(struct Memory *memory, Object rope_string, s64 string_size) {
  return string_size == RopeStringLength(memory, rope_string);
}

void TestRope() {
}
