/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qml2cppdefaultpasses.h"
#include "qmltcpropertyutils.h"

#include <QtCore/qfileinfo.h>
#include <QtCore/qqueue.h>
#include <QtCore/qloggingcategory.h>

#include <algorithm>

QT_BEGIN_NAMESPACE

using namespace Qt::StringLiterals;

static QString const cppKeywords[] = {
    u"alignas"_s,
    u"alignof"_s,
    u"and"_s,
    u"and_eq"_s,
    u"asm"_s,
    u"atomic_cancel"_s,
    u"atomic_commit"_s,
    u"atomic_noexcept"_s,
    u"auto"_s,
    u"bitand"_s,
    u"bitor"_s,
    u"bool"_s,
    u"break"_s,
    u"case"_s,
    u"catch"_s,
    u"char"_s,
    u"char8_t"_s,
    u"char16_t"_s,
    u"char32_t"_s,
    u"class"_s,
    u"compl"_s,
    u"concept"_s,
    u"const"_s,
    u"consteval"_s,
    u"constexpr"_s,
    u"const_cast"_s,
    u"continue"_s,
    u"co_await"_s,
    u"co_return"_s,
    u"co_yield"_s,
    u"decltype"_s,
    u"default"_s,
    u"delete"_s,
    u"do"_s,
    u"double"_s,
    u"dynamic_cast"_s,
    u"else"_s,
    u"enum"_s,
    u"explicit"_s,
    u"export"_s,
    u"extern"_s,
    u"false"_s,
    u"float"_s,
    u"for"_s,
    u"friend"_s,
    u"goto"_s,
    u"if"_s,
    u"inline"_s,
    u"int"_s,
    u"long"_s,
    u"mutable"_s,
    u"namespace"_s,
    u"new"_s,
    u"noexcept"_s,
    u"not"_s,
    u"not_eq"_s,
    u"nullptr"_s,
    u"operator"_s,
    u"or"_s,
    u"or_eq"_s,
    u"private"_s,
    u"protected"_s,
    u"public"_s,
    u"reflexpr"_s,
    u"register"_s,
    u"reinterpret_cast"_s,
    u"requires"_s,
    u"return"_s,
    u"short"_s,
    u"signed"_s,
    u"sizeof"_s,
    u"static"_s,
    u"static_assert"_s,
    u"static_cast"_s,
    u"struct"_s,
    u"switch"_s,
    u"synchronized"_s,
    u"template"_s,
    u"this"_s,
    u"thread_local"_s,
    u"throw"_s,
    u"true"_s,
    u"try"_s,
    u"typedef"_s,
    u"typeid"_s,
    u"typename"_s,
    u"union"_s,
    u"unsigned"_s,
    u"using"_s,
    u"virtual"_s,
    u"void"_s,
    u"volatile"_s,
    u"wchar_t"_s,
    u"while"_s,
    u"xor"_s,
    u"xor_eq"_s,
};

static bool isReservedWord(QStringView word)
{
    if (word.startsWith(QChar(u'_')) && word.size() >= 2
        && (word[1].isUpper() || word[1] == QChar(u'_'))) {
        return true; // Indentifiers starting with underscore and uppercase are reserved in C++
    }
    return std::binary_search(std::begin(cppKeywords), std::end(cppKeywords), word);
}

template<typename IRElement>
quint32 irNameIndex(const IRElement &irElement)
{
    return irElement.nameIndex;
}

template<>
quint32 irNameIndex<QmlIR::Alias>(const QmlIR::Alias &alias)
{
    return alias.nameIndex();
}

template<typename InputIterator>
static decltype(auto) findIrElement(const QmlIR::Document *doc, InputIterator first,
                                    InputIterator last, QStringView name)
{
    auto it = std::find_if(first, last, [&](const auto &candidate) {
        return name == doc->stringAt(irNameIndex(candidate));
    });
    Q_ASSERT(it != last); // must be satisfied by the caller
    return *it;
}

// convenience wrapper
template<typename InputIterator>
static decltype(auto) findIrLocation(const QmlIR::Document *doc, InputIterator first,
                                     InputIterator last, QStringView name)
{
    return findIrElement(doc, first, last, name).location;
}

Q_LOGGING_CATEGORY(lcDefaultPasses, "qml.qmltc.compilerpasses", QtWarningMsg);

static bool isComponent(const QQmlJSScope::ConstPtr &type)
{
    auto base = type->baseType();
    return base && base->internalName() == u"QQmlComponent"_s;
}

static bool isComponentBased(const QQmlJSScope::ConstPtr &type)
{
    auto base = QQmlJSScope::nonCompositeBaseType(type);
    return base && base->internalName() == u"QQmlComponent"_s;
}

static QString findPropertyName(const Qml2CppContext &context, const QQmlJSScope::ConstPtr &type,
                                const QmlIR::Binding &binding)
{
    QString name = context.document->stringAt(binding.propertyNameIndex);
    if (name.isEmpty()) {
        Q_ASSERT(type);
        auto base = type->baseType();
        if (!base)
            return name;
        name = base->defaultPropertyName();
    }
    return name;
}

void verifyTypes(const Qml2CppContext &context, QList<Qml2CppObject> &objects)
{
    const auto verifyProperty = [&](const QQmlJSMetaProperty &property,
                                    const QmlIR::Object *irObject) {
        // TODO: this whole verify function is a mess
        Q_ASSERT(!property.isAlias());
        if (property.propertyName().isEmpty())
            context.recordError(irObject->location, u"Property with unknown name found"_s);

        const auto type = property.type();
        if (type)
            return;

        // search irObject's properties and try to find matching one
        const QStringView name = property.propertyName();
        auto loc = findIrLocation(context.document, irObject->propertiesBegin(),
                                  irObject->propertiesEnd(), name);
        context.recordError(loc, u"Property '" + name + u"' of unknown type");
        if (property.isList() && !property.type()->isReferenceType()) {
            context.recordError(loc,
                                u"Property '" + name
                                        + u"' is a list type that contains non-pointer types");
        }
    };

    // Note: this is an intentionally incomplete check
    const auto verifyAlias = [&](const QQmlJSMetaProperty &alias, const QmlIR::Object *irObject,
                                 const QQmlJSScope::ConstPtr &objectType) {
        Q_ASSERT(alias.isAlias());
        if (alias.propertyName().isEmpty()) {
            context.recordError(irObject->location, u"Property alias with unknown name found"_s);
            return;
        }

        auto loc = findIrLocation(context.document, irObject->aliasesBegin(),
                                  irObject->aliasesEnd(), alias.propertyName());
        QStringList aliasExprBits = alias.aliasExpression().split(u'.');
        if (aliasExprBits.isEmpty()) {
            context.recordError(loc, u"Alias expression is invalid"_s);
            return;
        }

        QQmlJSScope::ConstPtr type = context.typeResolver->scopeForId(aliasExprBits[0], objectType);
        if (!type) {
            context.recordError(loc, u"Alias references an invalid id '" + aliasExprBits[0] + u"'");
            return;
        }
        aliasExprBits.removeFirst();
        for (const QString &bit : qAsConst(aliasExprBits)) {
            if (bit.isEmpty()) {
                context.recordError(loc, u"Alias expression contains empty piece"_s);
                break;
            }
            // TODO: we might need some better location, but this is not
            // absolutely essential
            QQmlJSMetaProperty p = type->property(bit);
            if (!p.isValid()) {
                Q_ASSERT(!type->hasProperty(bit)); // we're in deep trouble otherwise
                context.recordError(loc,
                                    u"Property '" + bit + u"' in '" + alias.aliasExpression()
                                            + u"' does not exist");
                break;
            }
            if (!p.type()) {
                context.recordError(loc,
                                    u"Property '" + bit + u"' in '" + alias.aliasExpression()
                                            + u"' of unknown type");
                break;
            }
            type = p.type();
            // NB: the rest is checked at a later point when we set up
            // to-be-compiled types properly
        }
    };

    const auto verifyBinding = [&](const QmlIR::Binding &binding,
                                   const QQmlJSScope::ConstPtr &type) {
        // QQmlComponent-wrapped types are special. consider:
        // `Component { QtObject {} }`
        // Component doesn't have a default property so this is an error in
        // normal code
        if (isComponent(type))
            return;

        const QString propName = findPropertyName(context, type, binding);
        // it's an error here
        if (propName.isEmpty()) {
            context.recordError(binding.location,
                                u"Cannot assign to non-existent default property"_s);
            return;
        }

        // ignore signal properties
        if (QmlIR::IRBuilder::isSignalPropertyName(propName))
            return;

        // attached property is special
        if (binding.type() == QmlIR::Binding::Type_AttachedProperty) {
            const auto [attachedObject, attachedType] = objects.at(binding.value.objectIndex);
            if (!attachedObject || !attachedType) {
                context.recordError(binding.location,
                                    u"Binding on attached object '" + propName
                                            + u"' of unknown type");
            }
            // Note: since attached object is part of objects, it will be
            // verified at a later point anyway, so nothing has to be done here
            return;
        }

        QQmlJSMetaProperty p = type->property(propName);
        if (!p.isValid()) {
            context.recordError(binding.location,
                                u"Binding on unknown property '" + propName + u"'");
            return; // nothing to do here anyway
        }
        if (!p.type()) { // Note: aliases also have valid type
            context.recordError(binding.location,
                                u"Binding on property '" + propName + u"' of unknown type");
        }

        // TODO: why isList() needed here?
        if (!p.isWritable() && !p.isList()
            && !binding.hasFlag(QmlIR::Binding::InitializerForReadOnlyDeclaration)
            && binding.type() != QmlIR::Binding::Type_GroupProperty) {
            context.recordError(binding.location,
                                u"Binding on read-only property '" + propName + u"'");
        }
    };

    for (const auto &object : objects) {
        const auto [irObject, type] = object;
        Q_ASSERT(irObject && type); // assume verified
        if (!type->baseType()) { // base class exists
            context.recordError(type->sourceLocation(), u"QML type has undefined base type"_s);
            // not critical for type verification, so do not break the iteration
        }

        if (type->isInCustomParserParent()) { // has QML_CUSTOMPARSER
            // TODO: we might end up supporting it later, but this needs certain
            // modifications to QML_CUSTOMPARSER macro at the very least to
            // support parser flags - see e.g.
            // QQmlCustomParser::AcceptsAttachedProperties

#if 0
            context.recordError(type->sourceLocation(),
                                u"QML type of this kind is not supported (QML_CUSTOMPARSER)"_s);
            continue;
#else
            // do silent error only because QQmlListModel is affected as well
            qCDebug(lcDefaultPasses) << "QML type" << type->internalName()
                                     << "of this kind is not supported (QML_CUSTOMPARSER)";
#endif
        }

        // verify own properties
        const auto properties = object.type->ownProperties();
        for (auto it = properties.cbegin(); it != properties.cend(); ++it) {
            const auto &prop = *it;
            if (prop.isAlias())
                verifyAlias(prop, irObject, object.type);
            else
                verifyProperty(prop, irObject);
        }

        // verify bindings
        for (auto it = irObject->bindingsBegin(); it != irObject->bindingsEnd(); ++it)
            verifyBinding(*it, type);
    }
}

// verify that object's strings are not reserved C++ words:
// * enumeration name (because enum name stays "as is") and keys
// * own property names
// * own method names and method parameter names
// * TODO: bindings are ignored
//
// additionally, verify that no redefinition happens (e.g. no two properties
// named the same way, etc.)
static void checkObjectStringsForCollisions(const Qml2CppContext &context,
                                            const Qml2CppObject &object)
{
    const auto isValidName = [&](QStringView name, auto getLocation, const QString &errorPrefix,
                                 QSet<QStringView> &seenSymbols) {
        // check name (e.g. property name) for correctness
        const bool isReserved = isReservedWord(name);
        const bool isSeenBefore = seenSymbols.contains(name);
        // getLocation() might be slow, so only use it once if there is an error
        decltype(getLocation()) location {}; // stub value
        if (isReserved || isSeenBefore)
            location = getLocation();

        if (isReserved) {
            context.recordError(location,
                                errorPrefix + u" '" + name
                                        + u"' is a reserved C++ word, consider renaming");
        }
        if (isSeenBefore) {
            context.recordError(location, errorPrefix + u" with this name already exists");
        } else {
            seenSymbols.insert(name);
        }
    };

    const auto irObject = object.irObject;
    const auto &type = object.type;

    QSet<QStringView> uniqueSymbols;

    const auto enums = type->ownEnumerations();
    for (auto it = enums.cbegin(); it != enums.cend(); ++it) {
        const QQmlJSMetaEnum e = it.value();
        QStringView name = e.name();
        QmlIR::Enum irEnum {}; // reuse for enumeration keys
        const auto getEnumLocation = [&]() {
            irEnum = findIrElement(context.document, irObject->enumsBegin(), irObject->enumsEnd(),
                                   name);
            return irEnum.location;
        };
        isValidName(name, getEnumLocation, u"Enumeration"_s, uniqueSymbols);

        const auto enumKeys = e.keys();
        for (const auto &key : enumKeys) {
            const auto getEnumKeyLocation = [&]() {
                return findIrLocation(context.document, irEnum.enumValuesBegin(),
                                      irEnum.enumValuesEnd(), key);
            };
            // no support for enum classes: each key is visible outside of enum
            isValidName(key, getEnumKeyLocation, u"Enumeration key"_s, uniqueSymbols);
        }
    }

    const auto properties = type->ownProperties();
    for (auto it = properties.cbegin(); it != properties.cend(); ++it) {
        const QQmlJSMetaProperty &p = it.value();
        QStringView name = p.propertyName();
        const auto getPropertyLocation = [&]() { return p.type()->sourceLocation(); };
        isValidName(name, getPropertyLocation, u"Property"_s, uniqueSymbols);
    }

    const auto methods = type->ownMethods();
    for (auto it = methods.cbegin(); it != methods.cend(); ++it) {
        const QQmlJSMetaMethod &m = it.value();
        QStringView name = m.methodName();
        const auto getMethodLocation = [&]() { return m.returnType()->sourceLocation(); };
        isValidName(name, getMethodLocation, u"Method"_s, uniqueSymbols);

        const auto parameterNames = m.parameterNames();
        QSet<QStringView> uniqueParameters; // parameters can shadow
        for (qsizetype i = 0; i < parameterNames.size(); ++i) {
            QStringView paramName = parameterNames.at(i);
            const auto getParamLocation = [&]() {
                static auto paramTypes = m.parameterTypes();
                return paramTypes.at(i)->sourceLocation();
            };
            isValidName(paramName, getParamLocation, u"Parameter"_s, uniqueParameters);
        }
    }
}

void checkForNamingCollisionsWithCpp(const Qml2CppContext &context, QList<Qml2CppObject> &objects)
{
    for (const auto &object : objects)
        checkObjectStringsForCollisions(context, object);
}

static void updateInternalName(QQmlJSScope::Ptr root, QString prefix,
                               QHash<QString, qsizetype> &typeCounts)
{
    if (!root)
        return;

    // gives unique C++ class name for a basic name based on type occurrence,
    // additionally updating the type occurrence
    const auto uniqueCppClassName = [&](QString basicName) -> QString {
        if (!typeCounts.contains(basicName)) {
            typeCounts.insert(basicName, 1);
            return basicName;
        }
        return basicName + u"_" + QString::number(typeCounts[basicName]++);
    };

    switch (root->scopeType()) {
    case QQmlJSScope::AttachedPropertyScope: {
        // special case
        Q_ASSERT(!root->baseTypeName().isEmpty());
        root->setInternalName(root->baseTypeName());
        break;
    }
    // case QQmlJSScope::JSFunctionScope:
    // case QQmlJSScope::JSLexicalScope:
    // case QQmlJSScope::GroupedPropertyScope:
    // case QQmlJSScope::QMLScope:
    // case QQmlJSScope::EnumScope:
    default: {
        root->setInternalName(prefix);
        break;
    }
    }

    const QList<QQmlJSScope::Ptr> children = root->childScopes();
    for (QQmlJSScope::Ptr child : children) {
        updateInternalName(child,
                           uniqueCppClassName(root->internalName() + u"_" + child->baseTypeName()),
                           typeCounts);
    }
}

void makeUniqueCppNames(const Qml2CppContext &context, QList<Qml2CppObject> &objects)
{
    Q_UNUSED(objects);

    QHash<QString, qsizetype> typeCounts;
    for (const QString &str : cppKeywords)
        typeCounts.insert(str, 1);

    // root is special:
    QQmlJSScope::Ptr root = context.typeResolver->root();
    QFileInfo fi(context.documentUrl);
    auto cppName = fi.baseName();
    if (typeCounts.contains(cppName)) {
        context.recordError(root->sourceLocation(),
                            u"Root object name '" + cppName + u"' is reserved");
        return;
    }
    if (cppName.isEmpty()) {
        context.recordError(root->sourceLocation(), u"Root object's name is empty"_s);
        return;
    }
    typeCounts.insert(cppName, 1);

    updateInternalName(root, cppName, typeCounts);
}

static void setupQmlCppType(const Qml2CppContext &context, const QQmlJSScope::Ptr &type,
                            const QString &filePath)
{
    Q_ASSERT(type);
    if (filePath.isEmpty()) {
        context.recordError(type->sourceLocation(), u"QML type has unknown file path"_s);
        return;
    }
    if (type->filePath().endsWith(u".h")) // consider this one to be already set up
        return;
    if (!filePath.endsWith(u".qml"_s)) {
        context.recordError(type->sourceLocation(),
                            u"QML type has non-QML origin (internal error)"_s);
        return;
    }

    // TODO: this does not cover QT_QMLTC_FILE_BASENAME renaming
    if (filePath != context.documentUrl) {
        // this file name will be discovered during findCppIncludes
        type->setFilePath(QFileInfo(filePath).baseName().toLower() + u".h"_s);
    }

    const auto properties = type->ownProperties();
    for (auto it = properties.cbegin(); it != properties.cend(); ++it) {
        QQmlJSMetaProperty p = it.value();
        Q_ASSERT(it.key() == p.propertyName());

        if (p.isAlias()) // we'll process aliases separately
            continue;

        QmltcPropertyData compiledData(p);
        if (p.read().isEmpty())
            p.setRead(compiledData.read);
        if (p.write().isEmpty() && p.isWritable() && !p.isList())
            p.setWrite(compiledData.write);
        if (p.bindable().isEmpty() && !p.isList())
            p.setBindable(compiledData.bindable);
        // TODO: p.setNotify(compiledData.notify); - ?
        type->addOwnProperty(p);
    }
}

QSet<QString> setupQmlCppTypes(const Qml2CppContext &context, QList<Qml2CppObject> &objects)
{
    // TODO: in general, the whole logic here is incomplete. it will suffice as
    // long as we only import QML types from our own module and C++ types from
    // any module. importing QML types (that are presumably compiled) from
    // external modules is not supported, so we can get away with it

    QSet<QString> qmlBaseTypes;
    for (const auto &object : objects) {
        // 1. set up object itself
        setupQmlCppType(context, object.type, context.documentUrl);

        // 2. set up the base type if it is also QML originated
        if (auto base = object.type->baseType(); base->isComposite()) {
            auto pair = context.typeResolver->importedType(base);
            if (pair.first.isEmpty()) {
                context.recordError(object.type->sourceLocation(),
                                    u"QML base type has unknown origin. Do you miss an import?"_s);
                continue;
            }

            setupQmlCppType(context, pair.second, pair.first);
            qmlBaseTypes.insert(object.type->baseTypeName());
        }
    }
    return qmlBaseTypes;
}

#if 0
// TODO: somewhat of a necessary crutch (or can we just not ignore it - and move
// to qmllint)
template<typename It, typename UnaryPredicate>
static bool traverseAlias(It first, It last, QQmlJSMetaProperty p, UnaryPredicate pred)
{
    QQmlJSScope::ConstPtr type = p.type();
    for (; first != last; ++first) {
        p = type->property(*first);
        if (pred(p))
            return true;
        type = p.type();
    }
    return false;
}
#endif

// TODO: this should really become a part of the visitor. otherwise, the
// to-be-compiled types from different documents do not get resolved aliases
// a.k.a. we have to use QObject::setProperty() instead.
static void resolveValidateOrSkipAlias(const Qml2CppContext &context,
                                       const Qml2CppObject &aliasOwner, QQmlJSMetaProperty alias,
                                       QSet<QQmlJSMetaProperty> &unresolved)
{
    Q_ASSERT(alias.isAlias());
    Q_ASSERT(!alias.propertyName().isEmpty());
    // TODO: we might need some better location, but this is not
    // absolutely essential
    auto loc = findIrLocation(context.document, aliasOwner.irObject->aliasesBegin(),
                              aliasOwner.irObject->aliasesEnd(), alias.propertyName());
    QStringList aliasExprBits = alias.aliasExpression().split(u'.');
    Q_ASSERT(aliasExprBits.size() > 1);

    bool canHaveWrite = true;

    QQmlJSMetaProperty p;
    QQmlJSScope::ConstPtr type =
            context.typeResolver->scopeForId(aliasExprBits[0], aliasOwner.type);
    Q_ASSERT(type);
    aliasExprBits.removeFirst();
    for (qsizetype i = 0; i < aliasExprBits.size(); ++i) {
        const QString &bit = qAsConst(aliasExprBits)[i];
        p = type->property(bit);
        Q_ASSERT(p.isValid() && p.type());

        const bool isYetToBeResolvedAlias = unresolved.contains(p);
        if (isYetToBeResolvedAlias) {
            // we can do nothing with `alias` at the moment, so just skip it for
            // now. the caller must then re-call this function, after `p` is
            // processed. we assume no cycles exist at this stage, so the
            // caller's code must terminate at some point.
            return;
        }

        struct Remover
        {
            QSet<QQmlJSMetaProperty> &unresolved;
            const QQmlJSMetaProperty &alias;
            bool needUpdate = true;
            ~Remover()
            {
                if (needUpdate)
                    unresolved.remove(alias);
            }
        } scopedRemover { unresolved, alias };

        // validate and resolve the alias:
        if (p.read().isEmpty()) { // we won't be able to read this property -- always an error
            context.recordError(loc,
                                u"Property '" + bit + u"' of '" + type->internalName()
                                        + u"' does not have READ method");
            return;
        }
        // NB: last iteration is special, don't check for WRITE as it might
        // not exist even for value property (in this case alias would only
        // have a READ method, which is fine)
        if (i == aliasExprBits.size() - 1)
            continue; // NB: this will trigger scopedRemover, which is good

        if (!p.isWritable()) {
            canHaveWrite = false;
            // check whether value type property has no WRITE method while it's
            // subproperties have WRITE - if so, this is likely at least a
            // warning/info

#if 0 // TODO: something here is broken in release build
      // NB: expensive in general case, pretty cheap otherwise, but still
      // only run this in debug
            const auto hasWrite = [](const QQmlJSMetaProperty &p) { return p.isWritable(); };
            if (p.type()->accessSemantics() == QQmlJSScope::AccessSemantics::Value
                && traverseAlias(aliasExprBits.cbegin() + i, aliasExprBits.cend(), p, hasWrite)) {
                qCDebug(lcDefaultPasses).noquote().nospace()
                        << context.documentUrl << ":" << loc.line << ":" << loc.column << ":"
                        << "Value type property '" << bit << u"' of '" << type->internalName()
                        << u"' does not have WRITE method while it probably should";
            }
#endif
        }
        type = p.type();
        scopedRemover.needUpdate = false;
    }

    Q_ASSERT(!unresolved.contains(alias));

    // here, `p` is the actually aliased property, `type` is the owner of this
    // property

    QmltcPropertyData compiledData(alias);
    if (alias.read().isEmpty())
        alias.setRead(compiledData.read);
    // TODO: how to handle redefinition/overload?
    if (QString setName = p.write(); !setName.isEmpty()) {
        QList<QQmlJSMetaMethod> methods = type->methods(setName);
        if (methods.size() > 1) {
            context.recordError(loc,
                                u"WRITE method of aliased property '" + p.propertyName() + u"' of '"
                                        + type->internalName() + u"' is ambiguous");
        }
        if (canHaveWrite && alias.write().isEmpty())
            alias.setWrite(compiledData.write);
    }
    if (!p.bindable().isEmpty() && alias.bindable().isEmpty())
        alias.setBindable(compiledData.bindable);
    if (QString notifyName = p.notify(); !notifyName.isEmpty()) {
        QList<QQmlJSMetaMethod> methods = type->methods(notifyName);
        if (methods.size() > 1) {
            context.recordError(loc,
                                u"NOTIFY method of aliased property '" + p.propertyName()
                                        + u"' of '" + type->internalName() + u"' is ambiguous");
        }
        if (alias.notify().isEmpty())
            alias.setNotify(compiledData.notify);
    }
    aliasOwner.type->addOwnProperty(alias);
}

void deferredResolveValidateAliases(const Qml2CppContext &context, QList<Qml2CppObject> &objects)
{
    QSet<QQmlJSMetaProperty> unresolved;
    for (const auto &object : objects) {
        const auto [irObject, type] = object;
        Q_ASSERT(irObject && type); // assume verified

        const auto properties = object.type->ownProperties();
        for (QQmlJSMetaProperty p : properties) {
            if (!p.isAlias())
                continue;
            QStringList aliasExprBits = p.aliasExpression().split(u'.');
            Q_ASSERT(!aliasExprBits.isEmpty());
            if (aliasExprBits.size() == 1) { // special case
                Q_ASSERT(context.typeResolver->scopeForId(aliasExprBits.at(0), object.type));
                // since it points to an object, set only the essential stuff
                // and continue - it is already resolved after this
                QmltcPropertyData compiledData(p);
                if (p.read().isEmpty())
                    p.setRead(compiledData.read);
                // NB: id-pointing aliases are read-only
                type->addOwnProperty(p);
                continue;
            }
            unresolved.insert(p);
        }
    }

    // NB: assume no cycles at this stage - see
    // QQmlJSImportVisitor::resolveAliases()
    while (!unresolved.isEmpty()) {
        for (const auto &object : objects) {
            const auto properties = object.type->ownProperties();
            for (QQmlJSMetaProperty p : properties) {
                if (!unresolved.contains(p)) // only contains aliases, so p.isAlias() is redundant
                    continue;
                resolveValidateOrSkipAlias(context, object, p, unresolved);
            }
        }
    }
}

static void addFirstCppIncludeFromType(QSet<QString> &cppIncludes,
                                       const QQmlJSScope::ConstPtr &type)
{
    auto t = QQmlJSScope::nonCompositeBaseType(type);
    if (!t)
        return;
    if (QString includeFile = t->filePath(); includeFile.endsWith(u".h"))
        cppIncludes.insert(includeFile);
}

static void populateCppIncludes(QSet<QString> &cppIncludes, const QQmlJSScope::ConstPtr &type)
{
    const auto constructPrivateInclude = [](QStringView publicInclude) -> QString {
        if (publicInclude.isEmpty())
            return QString();
        Q_ASSERT(publicInclude.endsWith(u".h"_s) || publicInclude.endsWith(u".hpp"_s));
        const qsizetype dotLocation = publicInclude.endsWith(u".h"_s) ? publicInclude.size() - 2
                                                                       : publicInclude.size() - 4;
        QStringView extension = publicInclude.sliced(dotLocation);
        QStringView includeWithoutExtension = publicInclude.first(dotLocation);
        // check if the "public" include is actually private
        if (includeWithoutExtension.startsWith(u"private"))
            return includeWithoutExtension.toString() + u"_p" + extension.toString();
        return u"private/" + includeWithoutExtension.toString() + u"_p" + extension.toString();
    };

    // TODO: this pass is VERY slow - we have to do exhaustive search, however,
    // because some classes could do forward declarations

    // look in type itself
    // addFirstCppIncludeFromType(cppIncludes, type);

    // look in type hierarchy
    for (auto t = type; t; t = t->baseType()) {
        // NB: Composite types might have include files - this is custom qmltc
        // logic for local imports
        if (QString includeFile = t->filePath(); includeFile.endsWith(u".h"))
            cppIncludes.insert(includeFile);

        // look in property types
        const auto properties = t->ownProperties();
        for (const QQmlJSMetaProperty &p : properties) {
            addFirstCppIncludeFromType(cppIncludes, p.type());

            const auto baseType = QQmlJSScope::nonCompositeBaseType(t);

            if (p.isPrivate() && baseType->filePath().endsWith(u".h")) {
                const QString ownersInclude = baseType->filePath();
                QString privateInclude = constructPrivateInclude(ownersInclude);
                if (!privateInclude.isEmpty())
                    cppIncludes.insert(std::move(privateInclude));
            }
        }

        // look in method types
        const auto methods = t->ownMethods();
        for (const QQmlJSMetaMethod &m : methods) {
            addFirstCppIncludeFromType(cppIncludes, m.returnType());

            const auto parameters = m.parameterTypes();
            for (const auto &p : parameters)
                addFirstCppIncludeFromType(cppIncludes, p);
        }
    }
}

QSet<QString> findCppIncludes(const Qml2CppContext &context, QList<Qml2CppObject> &objects)
{
    Q_UNUSED(objects);
    QSet<QString> cppIncludes;

    QQueue<QQmlJSScope::ConstPtr> objectsQueue;
    objectsQueue.enqueue(context.typeResolver->root());

    while (!objectsQueue.isEmpty()) {
        QQmlJSScope::ConstPtr current = objectsQueue.dequeue();
        Q_ASSERT(current); // assume verified

        populateCppIncludes(cppIncludes, current);

        const auto children = current->childScopes();
        for (auto child : children)
            objectsQueue.enqueue(child);
    }

    return cppIncludes;
}

QHash<int, int> findAndResolveExplicitComponents(const Qml2CppContext &context,
                                                 QList<Qml2CppObject> &objects)
{
    QHash<int, int> identity;
    // NB: unlike in the case of implicit components, we only need to look at
    // the objects array and ignore the bindings
    for (Qml2CppObject &o : objects) {
        if (isComponent(o.type)) {
            o.irObject->flags |= QV4::CompiledData::Object::IsComponent;
            Q_ASSERT(context.typeIndices->contains(o.type));
            const int index = int(context.typeIndices->value(o.type, -1));
            identity[index] = index;
        }
    }
    return identity;
}

template<typename Update>
static void updateImplicitComponents(const Qml2CppContext &context, Qml2CppObject &object,
                                     QList<Qml2CppObject> &objects, Update update)
{
    const auto checkAndUpdate = [&](const QmlIR::Binding &binding) {
        if (binding.type() != QmlIR::Binding::Type_Object)
            return;
        if (object.irObject->flags & QV4::CompiledData::Object::IsComponent) // already set
            return;

        const QString propName = findPropertyName(context, object.type, binding);
        Q_ASSERT(!propName.isEmpty()); // assume verified
        QQmlJSMetaProperty p = object.type->property(propName);
        Q_ASSERT(p.isValid()); // assume verified
        Q_ASSERT(p.type()); // assume verified

        // NB: in case property is a QQmlComponent, we need to handle it
        // specially
        if (p.type()->internalName() == u"QQmlComponent"_s) {
            // if it's an implicit component, call update function
            Q_ASSERT(binding.value.objectIndex < objects.size());
            update(objects[binding.value.objectIndex], binding.value.objectIndex);
        }
    };

    std::for_each(object.irObject->bindingsBegin(), object.irObject->bindingsEnd(), checkAndUpdate);
}

QHash<int, int> findAndResolveImplicitComponents(const Qml2CppContext &context,
                                                 QList<Qml2CppObject> &objects)
{
    int syntheticComponentCount = 0;
    QHash<int, int> indexMapping;
    const auto setQQmlComponentFlag = [&](Qml2CppObject &object, int objectIndex) {
        // QQmlComponentAndAliasResolver uses QMetaObject of the type and
        // compares it against QQmlComponent::staticMetaObject. we don't have it
        // here, so instead we should check whether the type is derived from
        // QQmlComponent and if so, it doesn't need a QQmlComponent wrapping
        if (isComponentBased(object.type)) {
            Q_ASSERT(!isComponent(object.type)
                     || (object.irObject->flags & QV4::CompiledData::Object::IsComponent));
            // this ir object is *already* marked as Component. which means it
            // is the case of explicit component bound to Component property:
            // property Component p: Component { ... }
            // property Component p: ComponentDerived { ... }
            return;
        }
        object.irObject->flags |= QV4::CompiledData::Object::IsComponent;
        Q_ASSERT(!indexMapping.contains(objectIndex));
        // TODO: the mapping construction is very ad-hoc, it could be that the
        // logic is more complicated. This is to compensate the lack of
        // QQmlComponentAndAliasResolver::findAndRegisterImplicitComponents()
        // that the QQmlTypeCompiler does
        indexMapping[objectIndex] = int(objects.size()) + syntheticComponentCount++;
    };

    for (Qml2CppObject &o : objects)
        updateImplicitComponents(context, o, objects, setQQmlComponentFlag);

    return indexMapping;
}

static void setObjectId(const Qml2CppContext &context, const QList<Qml2CppObject> &objects,
                        int objectIndex, QHash<int, int> &idToObjectIndex)
{
    // TODO: this method is basically a (modified) version of
    // QQmlComponentAndAliasResolver::collectIdsAndAliases()

    const auto isImplicitComponent = [](const Qml2CppObject &object) {
        // special (to this code) way to detect implicit components after
        // findAndResolveImplicitComponents() is run: unlike
        // QQmlComponentAndAliasResolver we do *not* create synthetic
        // components, but instead mark existing objects with IsComponent flag.
        // this gives a bad side effect (for the logic here) that we cannot
        // really distinguish between implicit and explicit components anymore
        return object.irObject->flags & QV4::CompiledData::Object::IsComponent
                && !isComponentBased(object.type);
    };

    const Qml2CppObject &object = objects.at(objectIndex);
    Q_ASSERT(object.irObject == context.document->objectAt(objectIndex));
    QmlIR::Object *irObject = object.irObject;
    Q_ASSERT(object.type); // assume verified
    Q_ASSERT(irObject); // assume verified

    if (isImplicitComponent(object)) {
        // Note: somehow QQmlTypeCompiler passes ensure that implicit components
        // have no idNameIndex set when setting ids for the document root. this
        // logic can't do it, so reject implicit components straight away
        // instead. the way QQmlTypeCompiler might make it work is through
        // synthetic components (which are created for every implicit
        // component): those do not have idNameIndex set
        return;
    }

    if (irObject->idNameIndex != 0) {
        if (idToObjectIndex.contains(irObject->idNameIndex)) {
            context.recordError(irObject->location, u"Object id is not unique"_s);
            return;
        }
        irObject->id = int(idToObjectIndex.size());
        idToObjectIndex.insert(irObject->idNameIndex, objectIndex);
    }

    // NB: rejecting IsComponent only *after* the id is potentially set. this is
    // aligned with QQmlTypeCompiler logic
    if (irObject->flags & QV4::CompiledData::Object::IsComponent && objectIndex != 0)
        return;

    std::for_each(irObject->bindingsBegin(), irObject->bindingsEnd(),
                  [&](const QmlIR::Binding &binding) {
                      switch (binding.type()) {
                      case QV4::CompiledData::Binding::Type_Object:
                      case QV4::CompiledData::Binding::Type_AttachedProperty:
                      case QV4::CompiledData::Binding::Type_GroupProperty:
                          setObjectId(context, objects, binding.value.objectIndex, idToObjectIndex);
                          break;
                      default:
                          break;
                      }
                  });
}

void setObjectIds(const Qml2CppContext &context, QList<Qml2CppObject> &objects)
{
    Q_UNUSED(objects);

    QHash<int, int> idToObjectIndex;
    const auto set = [&](int index) {
        idToObjectIndex.clear();
        Q_ASSERT(objects.at(index).irObject == context.document->objectAt(index));
        setObjectId(context, objects, index, idToObjectIndex);
    };

    // NB: in reality, we need to do the same also for implicit components, but
    // for now this is good enough
    for (qsizetype i = 1; i < objects.size(); ++i) {
        if (isComponent(objects[i].type))
            set(i);
    }
    set(0);
}

QSet<QQmlJSScope::ConstPtr> collectIgnoredTypes(const Qml2CppContext &context,
                                                QList<Qml2CppObject> &objects)
{
    Q_UNUSED(context);
    QSet<QQmlJSScope::ConstPtr> ignored;

    for (const Qml2CppObject &object : objects) {
        if (!(object.irObject->flags & QV4::CompiledData::Object::IsComponent))
            continue;
        if (ignored.contains(object.type))
            continue;

        // component root elements (and all their children) are ignored by the
        // code generator as they are going to be created by QQmlComponent
        QQueue<QQmlJSScope::ConstPtr> objectsQueue;
        objectsQueue.enqueue(object.type);
        while (!objectsQueue.isEmpty()) {
            auto current = objectsQueue.dequeue();
            if (current && current->scopeType() == QQmlJSScope::QMLScope)
                ignored.insert(current);

            const auto children = current->childScopes();
            for (auto child : children)
                objectsQueue.enqueue(child);
        }
    }

    return ignored;
}

QT_END_NAMESPACE
