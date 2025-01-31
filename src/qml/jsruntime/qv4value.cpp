// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <qv4runtime_p.h>
#include <qv4propertykey_p.h>
#include <qv4string_p.h>
#include <qv4symbol_p.h>
#include <qv4object_p.h>
#include <qv4objectproto_p.h>
#include <private/qv4mm_p.h>

#include <wtf/MathExtras.h>

using namespace QV4;

int Value::toUInt16() const
{
    if (integerCompatible())
        return (ushort)(uint)integerValue();

    double number = toNumber();

    double D16 = 65536.0;
    if ((number >= 0 && number < D16))
        return static_cast<ushort>(number);

    if (!std::isfinite(number))
        return +0;

    double d = ::floor(::fabs(number));
    if (std::signbit(number))
        d = -d;

    number = ::fmod(d , D16);

    if (number < 0)
        number += D16;

    return (unsigned short)number;
}

bool Value::toBooleanImpl(Value val)
{
    if (val.isManagedOrUndefined()) {
        Heap::Base *b = val.m();
        if (!b)
            return false;
        if (b->internalClass->vtable->isString)
            return static_cast<Heap::String *>(b)->length() > 0;
        return true;
    }

    // double
    double d = val.doubleValue();
    return d && !std::isnan(d);
}

double Value::toNumberImpl(Value val)
{
    switch (val.type()) {
    case QV4::Value::Undefined_Type:
        return std::numeric_limits<double>::quiet_NaN();
    case QV4::Value::Managed_Type:
        if (String *s = val.stringValue())
            return RuntimeHelpers::stringToNumber(s->toQString());
        if (val.isSymbol()) {
            Managed &m = static_cast<Managed &>(val);
            m.engine()->throwTypeError();
            return 0;
        }
        {
            Q_ASSERT(val.isObject());
            Scope scope(val.objectValue()->engine());
            ScopedValue protectThis(scope, val);
            ScopedValue prim(scope, RuntimeHelpers::toPrimitive(val, NUMBER_HINT));
            if (scope.hasException())
                return 0;
            return prim->toNumber();
        }
    case QV4::Value::Null_Type:
    case QV4::Value::Boolean_Type:
    case QV4::Value::Integer_Type:
        return val.int_32();
    default: // double
        Q_UNREACHABLE();
    }
}

static QString primitiveToQString(const Value *value)
{
    switch (value->type()) {
    case Value::Empty_Type:
        Q_ASSERT(!"empty Value encountered");
        Q_UNREACHABLE();
        return QString();
    case Value::Undefined_Type:
        return QStringLiteral("undefined");
    case Value::Null_Type:
        return QStringLiteral("null");
    case Value::Boolean_Type:
        if (value->booleanValue())
            return QStringLiteral("true");
        else
            return QStringLiteral("false");
    case Value::Managed_Type:
        Q_UNREACHABLE();
        return QString();
    case Value::Integer_Type: {
        QString str;
        RuntimeHelpers::numberToString(&str, (double)value->int_32(), 10);
        return str;
    }
    case Value::Double_Type: {
        QString str;
        RuntimeHelpers::numberToString(&str, value->doubleValue(), 10);
        return str;
    }
    } // switch

    Q_UNREACHABLE();
    return QString();
}


QString Value::toQStringNoThrow() const
{
    if (isManaged()) {
        if (String *s = stringValue())
            return s->toQString();
        if (Symbol *s = symbolValue())
            return s->descriptiveString();

        Q_ASSERT(isObject());
        Scope scope(objectValue()->engine());
        ScopedValue ex(scope);
        bool caughtException = false;
        ScopedValue prim(scope, RuntimeHelpers::toPrimitive(*this, STRING_HINT));
        if (scope.hasException()) {
            ex = scope.engine->catchException();
            caughtException = true;
        } else if (prim->isPrimitive()) {
            return prim->toQStringNoThrow();
        }

        // Can't nest try/catch due to CXX ABI limitations for foreign exception nesting.
        if (caughtException) {
            ScopedValue prim(scope, RuntimeHelpers::toPrimitive(ex, STRING_HINT));
            if (scope.hasException()) {
                ex = scope.engine->catchException();
            } else if (prim->isPrimitive()) {
                return prim->toQStringNoThrow();
            }
        }

        return QString();
    }

    return primitiveToQString(this);
}

QString Value::toQString() const
{
    if (isManaged()) {
        if (String *s = stringValue())
            return s->toQString();

        if (isSymbol()) {
            static_cast<const Managed *>(this)->engine()->throwTypeError();
            return QString();
        }

        Q_ASSERT(isObject());
        Scope scope(objectValue()->engine());
        ScopedValue prim(scope, RuntimeHelpers::toPrimitive(*this, STRING_HINT));
        return prim->toQString();
    }

    return primitiveToQString(this);
}

QString Value::toQString(bool *ok) const
{
    if (isManaged()) {
        if (String *s = stringValue()) {
            *ok = true;
            return s->toQString();
        }

        if (isSymbol()) {
            static_cast<const Managed *>(this)->engine()->throwTypeError();
            *ok = false;
            return QString();
        }

        Q_ASSERT(isObject());
        Scope scope(objectValue()->engine());
        ScopedValue prim(scope, RuntimeHelpers::toPrimitive(*this, STRING_HINT));

        if (scope.hasException()) {
            *ok = false;
            return QString();
        }

        return prim->toQString(ok);
    }

    return primitiveToQString(this);
}

QV4::PropertyKey Value::toPropertyKey(ExecutionEngine *e) const
{
    if (isInteger() && int_32() >= 0)
        return PropertyKey::fromArrayIndex(static_cast<uint>(int_32()));
    if (isStringOrSymbol()) {
        Scope scope(e);
        ScopedStringOrSymbol s(scope, this);
        return s->toPropertyKey();
    }
    Scope scope(e);
    ScopedValue v(scope, RuntimeHelpers::toPrimitive(*this, STRING_HINT));
    if (!v->isStringOrSymbol())
        v = v->toString(e);
    if (e->hasException)
        return PropertyKey::invalid();
    ScopedStringOrSymbol s(scope, v);
    return s->toPropertyKey();
}

bool Value::sameValue(Value other) const {
    if (_val == other._val)
        return true;
    String *s = stringValue();
    String *os = other.stringValue();
    if (s && os)
        return s->isEqualTo(os);
    if (isInteger() && other.isDouble())
        return int_32() ? (double(int_32()) == other.doubleValue())
                        : (other.doubleValue() == 0 && !std::signbit(other.doubleValue()));
    if (isDouble() && other.isInteger())
        return other.int_32() ? (doubleValue() == double(other.int_32()))
                              : (doubleValue() == 0 && !std::signbit(doubleValue()));
    if (isManaged())
        return other.isManaged() && cast<Managed>()->isEqualTo(other.cast<Managed>());
    return false;
}

bool Value::sameValueZero(Value other) const {
    if (_val == other._val)
        return true;
    String *s = stringValue();
    String *os = other.stringValue();
    if (s && os)
        return s->isEqualTo(os);
    if (isInteger() && other.isDouble())
        return double(int_32()) == other.doubleValue();
    if (isDouble() && other.isInteger())
        return other.int_32() == doubleValue();
    if (isDouble() && other.isDouble()) {
        if (doubleValue() == 0 && other.doubleValue() == 0) {
            return true;
        }
    }
    if (isManaged())
        return other.isManaged() && cast<Managed>()->isEqualTo(other.cast<Managed>());
    return false;
}

Heap::String *Value::toString(ExecutionEngine *e, Value val)
{
    return RuntimeHelpers::convertToString(e, val);
}

Heap::Object *Value::toObject(ExecutionEngine *e, Value val)
{
    return RuntimeHelpers::convertToObject(e, val);
}

uint Value::asArrayLength(bool *ok) const
{
    *ok = true;
    if (isInteger()) {
        if (int_32() >= 0) {
            return (uint)int_32();
        } else {
            *ok = false;
            return UINT_MAX;
        }
    }
    if (isNumber()) {
        double d = doubleValue();
        uint idx = (uint)d;
        if (idx != d) {
            *ok = false;
            return UINT_MAX;
        }
        return idx;
    }
    if (String *s = stringValue())
        return s->toUInt(ok);

    uint idx = toUInt32();
    double d = toNumber();
    if (d != idx) {
        *ok = false;
        return UINT_MAX;
    }
    return idx;
}
