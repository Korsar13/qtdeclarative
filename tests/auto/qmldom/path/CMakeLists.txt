# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

# Generated from path.pro.

#####################################################################
## tst_qmldompath Test:
#####################################################################

qt_internal_add_test(tst_qmldompath
    SOURCES
        tst_qmldompath.cpp tst_qmldompath.h
    DEFINES
        QT_DEPRECATED_WARNINGS
    PUBLIC_LIBRARIES
        Qt::QmlDomPrivate
)
