/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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

#ifndef FINDUNQUALIFIED_H
#define FINDUNQUALIFIED_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

#include "typedescriptionreader.h"
#include "scopetree.h"
#include "qcoloroutput.h"

#include <QtQml/private/qqmljsastvisitor_p.h>
#include <QtQml/private/qqmljsast_p.h>

#include <QtCore/qscopedpointer.h>

class FindUnqualifiedIDVisitor : public QQmlJS::AST::Visitor
{
    Q_DISABLE_COPY_MOVE(FindUnqualifiedIDVisitor)
public:
    explicit FindUnqualifiedIDVisitor(QStringList qmltypeDirs, QString code,
                                      QString fileName, bool silent);
    ~FindUnqualifiedIDVisitor() override = default;
    bool check();

private:
    struct Import {
        QHash<QString, ScopeTree::ConstPtr> objects;
        QList<ModuleApiInfo> moduleApis;
        QStringList dependencies;
    };

    QScopedPointer<ScopeTree> m_rootScope;
    ScopeTree *m_currentScope = nullptr;
    QHash<QString, ScopeTree::ConstPtr> m_exportedName2Scope;
    QStringList m_qmltypeDirs;
    QString m_code;
    QHash<QString, ScopeTree::ConstPtr> m_qmlid2scope;
    QString m_rootId;
    QString m_filePath;
    QSet<QPair<QString, QString>> m_alreadySeenImports;
    QSet<QString> m_unknownImports;
    ColorOutput m_colorOut;
    bool m_visitFailed = false;

    struct OutstandingConnection
    {
        QString targetName;
        ScopeTree *scope;
        QQmlJS::AST::UiObjectDefinition *uiod;
    };

    QVarLengthArray<OutstandingConnection, 3> m_outstandingConnections; // Connections whose target we have not encountered

    void enterEnvironment(ScopeType type, QString name);
    void leaveEnvironment();
    void importHelper(const QString &module, const QString &prefix = QString(),
                      int major = -1, int minor = -1);

    void readQmltypes(const QString &filename, Import &result);
    Import readQmldir(const QString &dirname);
    void processImport(const QString &prefix, const Import &import);

    ScopeTree *localQmlFile2ScopeTree(const QString &filePath);

    void importDirectory(const QString &directory, const QString &prefix);
    void importExportedNames(const QStringRef &prefix, QString name);

    void throwRecursionDepthError() override;

    // start block/scope handling
    bool visit(QQmlJS::AST::UiProgram *ast) override;
    void endVisit(QQmlJS::AST::UiProgram *ast) override;

    bool visit(QQmlJS::AST::ClassExpression *ast) override;
    void endVisit(QQmlJS::AST::ClassExpression *ast) override;

    bool visit(QQmlJS::AST::ClassDeclaration *ast) override;
    void endVisit(QQmlJS::AST::ClassDeclaration *ast) override;

    bool visit(QQmlJS::AST::ForStatement *ast) override;
    void endVisit(QQmlJS::AST::ForStatement *ast) override;

    bool visit(QQmlJS::AST::ForEachStatement *ast) override;
    void endVisit(QQmlJS::AST::ForEachStatement *ast) override;

    bool visit(QQmlJS::AST::Block *ast) override;
    void endVisit(QQmlJS::AST::Block *ast) override;

    bool visit(QQmlJS::AST::CaseBlock *ast) override;
    void endVisit(QQmlJS::AST::CaseBlock *ast) override;

    bool visit(QQmlJS::AST::Catch *ast) override;
    void endVisit(QQmlJS::AST::Catch *ast) override;

    bool visit(QQmlJS::AST::WithStatement *withStatement) override;
    void endVisit(QQmlJS::AST::WithStatement *ast) override;

    void visitFunctionExpressionHelper(QQmlJS::AST::FunctionExpression *fexpr);
    bool visit(QQmlJS::AST::FunctionExpression *fexpr) override;
    void endVisit(QQmlJS::AST::FunctionExpression *fexpr) override;

    bool visit(QQmlJS::AST::FunctionDeclaration *fdecl) override;
    void endVisit(QQmlJS::AST::FunctionDeclaration *fdecl) override;
    /* --- end block handling --- */

    bool visit(QQmlJS::AST::VariableDeclarationList *vdl) override;
    bool visit(QQmlJS::AST::FormalParameterList *fpl) override;

    bool visit(QQmlJS::AST::UiImport *import) override;
    bool visit(QQmlJS::AST::UiEnumDeclaration *uied) override;
    bool visit(QQmlJS::AST::UiObjectBinding *uiob) override;
    void endVisit(QQmlJS::AST::UiObjectBinding *uiob) override;
    bool visit(QQmlJS::AST::UiObjectDefinition *uiod) override;
    void endVisit(QQmlJS::AST::UiObjectDefinition *) override;
    bool visit(QQmlJS::AST::UiScriptBinding *uisb) override;
    bool visit(QQmlJS::AST::UiPublicMember *uipm) override;

    // expression handling
    bool visit(QQmlJS::AST::IdentifierExpression *idexp) override;

    bool visit(QQmlJS::AST::PatternElement *) override;
};

#endif // FINDUNQUALIFIED_H
