//===-- ubsan_handlers.cc -------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Error logging entry points for the UBSan runtime.
//
//===----------------------------------------------------------------------===//

#include "ubsan_handlers.h"
#include "ubsan_diag.h"

#include "sanitizer_common/sanitizer_common.h"

using namespace __sanitizer;
using namespace __ubsan;

namespace __ubsan {
  const char *TypeCheckKinds[] = {
    "load of", "store to", "reference binding to", "member access within",
    "member call on", "constructor call on", "downcast of", "downcast of"
  };
}

static void handleTypeMismatchImpl(TypeMismatchData *Data, ValueHandle Pointer,
                                   Location FallbackLoc, bool Abort) {
  Location Loc = Data->Loc.acquire();
  // Use the SourceLocation from Data to track deduplication, even if 'invalid'
  if (Loc.getSourceLocation().isDisabled())
    return;

  ScopedReport R(Abort);

  if (Data->Loc.isInvalid())
    Loc = FallbackLoc;

  if (!Pointer)
    Diag(Loc, DL_Error, "%0 null pointer of type %1")
      << TypeCheckKinds[Data->TypeCheckKind] << Data->Type;
  else if (Data->Alignment && (Pointer & (Data->Alignment - 1)))
    Diag(Loc, DL_Error, "%0 misaligned address %1 for type %3, "
                        "which requires %2 byte alignment")
      << TypeCheckKinds[Data->TypeCheckKind] << (void*)Pointer
      << Data->Alignment << Data->Type;
  else
    Diag(Loc, DL_Error, "%0 address %1 with insufficient space "
                        "for an object of type %2")
      << TypeCheckKinds[Data->TypeCheckKind] << (void*)Pointer << Data->Type;
  if (Pointer)
    Diag(Pointer, DL_Note, "pointer points here");
}

void __ubsan::__ubsan_handle_type_mismatch(TypeMismatchData *Data,
                                           ValueHandle Pointer) {
  handleTypeMismatchImpl(Data, Pointer, getCallerLocation(), false);
}
void __ubsan::__ubsan_handle_type_mismatch_abort(TypeMismatchData *Data,
                                                 ValueHandle Pointer) {
  handleTypeMismatchImpl(Data, Pointer, getCallerLocation(), true);
}

/// \brief Common diagnostic emission for various forms of integer overflow.
template <typename T>
static void handleIntegerOverflowImpl(OverflowData *Data, ValueHandle LHS,
                                      const char *Operator, T RHS, bool Abort) {
  SourceLocation Loc = Data->Loc.acquire();
  if (Loc.isDisabled())
    return;

  ScopedReport R(Abort);

  Diag(Loc, DL_Error, "%0 integer overflow: "
                      "%1 %2 %3 cannot be represented in type %4")
    << (Data->Type.isSignedIntegerTy() ? "signed" : "unsigned")
    << Value(Data->Type, LHS) << Operator << RHS << Data->Type;
}

#define UBSAN_OVERFLOW_HANDLER(handler_name, op, abort)                        \
  void __ubsan::handler_name(OverflowData *Data, ValueHandle LHS,              \
                             ValueHandle RHS) {                                \
    handleIntegerOverflowImpl(Data, LHS, op, Value(Data->Type, RHS), abort);   \
  }

UBSAN_OVERFLOW_HANDLER(__ubsan_handle_add_overflow, "+", false)
UBSAN_OVERFLOW_HANDLER(__ubsan_handle_add_overflow_abort, "+", true)
UBSAN_OVERFLOW_HANDLER(__ubsan_handle_sub_overflow, "-", false)
UBSAN_OVERFLOW_HANDLER(__ubsan_handle_sub_overflow_abort, "-", true)
UBSAN_OVERFLOW_HANDLER(__ubsan_handle_mul_overflow, "*", false)
UBSAN_OVERFLOW_HANDLER(__ubsan_handle_mul_overflow_abort, "*", true)

static void handleNegateOverflowImpl(OverflowData *Data, ValueHandle OldVal,
                                     bool Abort) {
  SourceLocation Loc = Data->Loc.acquire();
  if (Loc.isDisabled())
    return;

  ScopedReport R(Abort);

  if (Data->Type.isSignedIntegerTy())
    Diag(Loc, DL_Error,
         "negation of %0 cannot be represented in type %1; "
         "cast to an unsigned type to negate this value to itself")
      << Value(Data->Type, OldVal) << Data->Type;
  else
    Diag(Loc, DL_Error,
         "negation of %0 cannot be represented in type %1")
      << Value(Data->Type, OldVal) << Data->Type;
}

void __ubsan::__ubsan_handle_negate_overflow(OverflowData *Data,
                                             ValueHandle OldVal) {
  handleNegateOverflowImpl(Data, OldVal, false);
}
void __ubsan::__ubsan_handle_negate_overflow_abort(OverflowData *Data,
                                                    ValueHandle OldVal) {
  handleNegateOverflowImpl(Data, OldVal, true);
}

static void handleDivremOverflowImpl(OverflowData *Data, ValueHandle LHS,
                                     ValueHandle RHS, bool Abort) {
  SourceLocation Loc = Data->Loc.acquire();
  if (Loc.isDisabled())
    return;

  ScopedReport R(Abort);

  Value LHSVal(Data->Type, LHS);
  Value RHSVal(Data->Type, RHS);
  if (RHSVal.isMinusOne())
    Diag(Loc, DL_Error,
         "division of %0 by -1 cannot be represented in type %1")
      << LHSVal << Data->Type;
  else
    Diag(Loc, DL_Error, "division by zero");
}

void __ubsan::__ubsan_handle_divrem_overflow(OverflowData *Data,
                                             ValueHandle LHS, ValueHandle RHS) {
  handleDivremOverflowImpl(Data, LHS, RHS, false);
}
void __ubsan::__ubsan_handle_divrem_overflow_abort(OverflowData *Data,
                                                    ValueHandle LHS,
                                                    ValueHandle RHS) {
  handleDivremOverflowImpl(Data, LHS, RHS, true);
}

static void handleShiftOutOfBoundsImpl(ShiftOutOfBoundsData *Data,
                                       ValueHandle LHS, ValueHandle RHS,
                                       bool Abort) {
  SourceLocation Loc = Data->Loc.acquire();
  if (Loc.isDisabled())
    return;

  ScopedReport R(Abort);

  Value LHSVal(Data->LHSType, LHS);
  Value RHSVal(Data->RHSType, RHS);
  if (RHSVal.isNegative())
    Diag(Loc, DL_Error, "shift exponent %0 is negative") << RHSVal;
  else if (RHSVal.getPositiveIntValue() >= Data->LHSType.getIntegerBitWidth())
    Diag(Loc, DL_Error,
         "shift exponent %0 is too large for %1-bit type %2")
      << RHSVal << Data->LHSType.getIntegerBitWidth() << Data->LHSType;
  else if (LHSVal.isNegative())
    Diag(Loc, DL_Error, "left shift of negative value %0") << LHSVal;
  else
    Diag(Loc, DL_Error,
         "left shift of %0 by %1 places cannot be represented in type %2")
      << LHSVal << RHSVal << Data->LHSType;
}

void __ubsan::__ubsan_handle_shift_out_of_bounds(ShiftOutOfBoundsData *Data,
                                                 ValueHandle LHS,
                                                 ValueHandle RHS) {
  handleShiftOutOfBoundsImpl(Data, LHS, RHS, false);
}
void __ubsan::__ubsan_handle_shift_out_of_bounds_abort(
                                                     ShiftOutOfBoundsData *Data,
                                                     ValueHandle LHS,
                                                     ValueHandle RHS) {
  handleShiftOutOfBoundsImpl(Data, LHS, RHS, true);
}

static void handleOutOfBoundsImpl(OutOfBoundsData *Data, ValueHandle Index,
                                  bool Abort) {
  SourceLocation Loc = Data->Loc.acquire();
  if (Loc.isDisabled())
    return;

  ScopedReport R(Abort);

  Value IndexVal(Data->IndexType, Index);
  Diag(Loc, DL_Error, "index %0 out of bounds for type %1")
    << IndexVal << Data->ArrayType;
}

void __ubsan::__ubsan_handle_out_of_bounds(OutOfBoundsData *Data,
                                           ValueHandle Index) {
  handleOutOfBoundsImpl(Data, Index, false);
}
void __ubsan::__ubsan_handle_out_of_bounds_abort(OutOfBoundsData *Data,
                                                 ValueHandle Index) {
  handleOutOfBoundsImpl(Data, Index, true);
}

void __ubsan::__ubsan_handle_builtin_unreachable(UnreachableData *Data) {
  ScopedReport R(true);
  Diag(Data->Loc, DL_Error, "execution reached a __builtin_unreachable() call");
}

void __ubsan::__ubsan_handle_missing_return(UnreachableData *Data) {
  ScopedReport R(true);
  Diag(Data->Loc, DL_Error,
       "execution reached the end of a value-returning function "
       "without returning a value");
}

static void handleVLABoundNotPositive(VLABoundData *Data, ValueHandle Bound,
                                      bool Abort) {
  SourceLocation Loc = Data->Loc.acquire();
  if (Loc.isDisabled())
    return;

  ScopedReport R(Abort);

  Diag(Loc, DL_Error, "variable length array bound evaluates to "
                      "non-positive value %0")
    << Value(Data->Type, Bound);
}

void __ubsan::__ubsan_handle_vla_bound_not_positive(VLABoundData *Data,
                                                    ValueHandle Bound) {
  handleVLABoundNotPositive(Data, Bound, false);
}
void __ubsan::__ubsan_handle_vla_bound_not_positive_abort(VLABoundData *Data,
                                                          ValueHandle Bound) {
  handleVLABoundNotPositive(Data, Bound, true);
}


static void handleFloatCastOverflow(FloatCastOverflowData *Data, ValueHandle From,
                                    bool Abort) {
  // TODO: Add deduplication once a SourceLocation is generated for this check.
  ScopedReport R(Abort);

  Diag(getCallerLocation(), DL_Error,
       "value %0 is outside the range of representable values of type %2")
    << Value(Data->FromType, From) << Data->FromType << Data->ToType;
}

void __ubsan::__ubsan_handle_float_cast_overflow(FloatCastOverflowData *Data,
                                                 ValueHandle From) {
  handleFloatCastOverflow(Data, From, false);
}
void
__ubsan::__ubsan_handle_float_cast_overflow_abort(FloatCastOverflowData *Data,
                                                  ValueHandle From) {
  handleFloatCastOverflow(Data, From, true);
}

static void handleLoadInvalidValue(InvalidValueData *Data, ValueHandle Val,
                                   bool Abort) {
  SourceLocation Loc = Data->Loc.acquire();
  if (Loc.isDisabled())
    return;

  ScopedReport R(Abort);

  Diag(Loc, DL_Error,
       "load of value %0, which is not a valid value for type %1")
    << Value(Data->Type, Val) << Data->Type;
}

void __ubsan::__ubsan_handle_load_invalid_value(InvalidValueData *Data,
                                                ValueHandle Val) {
  handleLoadInvalidValue(Data, Val, false);
}
void __ubsan::__ubsan_handle_load_invalid_value_abort(InvalidValueData *Data,
                                                      ValueHandle Val) {
  handleLoadInvalidValue(Data, Val, true);
}

static void handleFunctionTypeMismatch(FunctionTypeMismatchData *Data,
    ValueHandle Function, bool Abort) {
  const char *FName = "(unknown)";

  Location Loc = getFunctionLocation(Function, &FName);

  ScopedReport R(Abort);

  Diag(Data->Loc, DL_Error,
       "call to function %0 through pointer to incorrect function type %1")
    << FName << Data->Type;
  Diag(Loc, DL_Note, "%0 defined here") << FName;
}

void
__ubsan::__ubsan_handle_function_type_mismatch(FunctionTypeMismatchData *Data,
                                               ValueHandle Function) {
  handleFunctionTypeMismatch(Data, Function, false);
}

void __ubsan::__ubsan_handle_function_type_mismatch_abort(
    FunctionTypeMismatchData *Data, ValueHandle Function) {
  handleFunctionTypeMismatch(Data, Function, true);
}
