# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

qt_get_tool_target_name(target_name qmljsrootgen)
qt_internal_add_tool(${target_name}
    TARGET_DESCRIPTION "QML Global Object Metatypes Generator"
    TOOLS_TARGET Qml # special case
    INSTALL_DIR "${INSTALL_LIBEXECDIR}"
    SOURCES
        main.cpp
    LIBRARIES
        Qt::CorePrivate
        Qt::QmlPrivate
)
qt_internal_return_unless_building_tools()
