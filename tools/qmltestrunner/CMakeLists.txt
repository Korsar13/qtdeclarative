# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

# Generated from qmltestrunner.pro.

#####################################################################
## qmltestrunner Tool:
#####################################################################

qt_get_tool_target_name(target_name qmltestrunner)
qt_internal_add_tool(${target_name}
    TARGET_DESCRIPTION "QML Test Runner"
    TOOLS_TARGET Qml # special case
    SOURCES
        main.cpp
    LIBRARIES
        Qt::Gui
        Qt::Qml
        Qt::QuickTest
)
qt_internal_return_unless_building_tools()

#### Keys ignored in scope 1:.:.:qmltestrunner.pro:<TRUE>:
# QMAKE_TARGET_DESCRIPTION = "QML" "Test" "Runner"
