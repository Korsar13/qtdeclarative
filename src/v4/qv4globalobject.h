/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the V4VM module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef QV4GLOBALOBJECT_H
#define QV4GLOBALOBJECT_H

#include "qv4global.h"
#include "qv4functionobject.h"

QT_END_NAMESPACE

namespace QQmlJS {
namespace VM {

struct Q_V4_EXPORT EvalFunction : FunctionObject
{
    EvalFunction(ExecutionContext *scope);
    EvalFunction(ExecutionContext *scope, Object *qmlActivation);

    static QQmlJS::VM::Function *parseSource(QQmlJS::VM::ExecutionContext *ctx,
                                             const QString &fileName,
                                             const QString &source,
                                             QQmlJS::Codegen::Mode mode, bool strictMode,
                                             bool inheritContext);

    Value evalCall(ExecutionContext *context, Value thisObject, Value *args, int argc, bool directCall);

    using Managed::construct;
    static Value call(Managed *that, ExecutionContext *, const Value &, Value *, int);

    Object *qmlActivation;

protected:
    static const ManagedVTable static_vtbl;
};

struct GlobalFunctions
{
    static Value method_parseInt(CallContext *context);
    static Value method_parseFloat(CallContext *context);
    static Value method_isNaN(CallContext *context);
    static Value method_isFinite(CallContext *context);
    static Value method_decodeURI(ExecutionContext *parentCtx, Value thisObject, Value *argv, int argc);
    static Value method_decodeURIComponent(ExecutionContext *parentCtx, Value thisObject, Value *argv, int argc);
    static Value method_encodeURI(ExecutionContext *parentCtx, Value thisObject, Value *argv, int argc);
    static Value method_encodeURIComponent(ExecutionContext *parentCtx, Value thisObject, Value *argv, int argc);
    static Value method_escape(CallContext *context);
    static Value method_unescape(CallContext *context);
};

} // namespace VM
} // namespace QQmlJS

QT_END_NAMESPACE

#endif // QMLJS_OBJECTS_H
