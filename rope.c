#include "rope.h"

#include "byte_vector.h"
#include "memory.h"
#include "pair.h"
#include "root.h"
#include "string.h"

// TODO: Make memory a global.

void AppendNewRopeString(Object last_pair, s64 string_size, u8 character);
void AppendCharacterToRopeString(Object rope_string, u8 character);
Object MakeNewRopeString(s64 string_size, u8 character);
u8 *StringPointer(Object rope_string);
Object LastCdr(Object list);
s64 RopeStringLength(Object rope_string);
b64 IsStringFull(Object rope_string, s64 string_size);

Object MakeRope(s64 string_size) {
  Object rope = AllocatePair();
  SetCar(UnboxReference(rope), BoxFixnum(string_size));
  SetCdr(UnboxReference(rope), nil);
  return rope;
}

void AppendRope(Object rope, u8 character) {
  s64 string_size = UnboxFixnum(Car(UnboxReference(rope)));
  Object strings = Cdr(UnboxReference(rope));
  if (strings == nil) {
    // this is the first character being added to the rope.
    AppendNewRopeString(rope, string_size, character);
  } else {
    Object end = LastCdr(strings);
    Object last_string = Car(UnboxReference(end));
    if (IsStringFull(last_string, string_size)) {
      // The last rope_string is full, need to allocate a new rope_string, and append it to the end of the rope.
      AppendNewRopeString(end, string_size, character);
    } else {
      AppendCharacterToRopeString(last_string, character);
    }
  }
}

Object FinalizeRope(Object rope) {
}

// TODO: Move this shit.
u8 *StringPointer(Object string) {
  return (u8 *)&memory.the_objects[UnboxReference(string) + 1];
}

// rope_string := (num_bytes . string_reference)
Object MakeNewRopeString(s64 string_size, u8 character) {
  Object string = BoxString(AllocateByteVector(string_size));

  u8 *str = StringPointer(string);
  str[0] = character;
  str[1] = 0;

  return MakePair(BoxFixnum(2), string);
}

void AppendNewRopeString(Object last_pair, s64 string_size, u8 character) {

  SetRegister(REGISTER_ROPE_SAVE, last_pair);
  Object pair = MakePair(MakeNewRopeString(string_size, character), nil);
  last_pair = GetRegister(REGISTER_ROPE_SAVE);

  SetCdr(UnboxReference(last_pair), pair);
}

s64 RopeStringLength(Object rope_string) {
  return UnboxFixnum(Car(UnboxReference(rope_string)));
}

void AppendCharacterToRopeString(Object rope_string, u8 character) {
  s64 length = RopeStringLength(rope_string);
  Object string = Cdr(UnboxReference(rope_string));

  u8 *str = StringPointer(string);
  str[length-1] = character;
  str[length] = 0;
  ++length;
}

Object LastCdr(Object list) {
  for (Object rest = Cdr(UnboxReference(list));
      rest != nil;
      list = rest, rest = Cdr(UnboxReference(rest)))
    ;

  return list;
}

b64 IsStringFull(Object rope_string, s64 string_size) {
  return string_size == RopeStringLength(rope_string);
}

void TestRope() {
}
