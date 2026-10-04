// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/DisplayList.h>

#include <QColor>
#include <QPainter>
#include <QPalette>

namespace timeline_qt
{

/// Resolves semantic roles against Qt theme colors and focus state.
QColor style_color(timeline::StyleRole role, const QPalette &palette, bool focused);

/// Delegates display-list primitives to QPainter in logical widget coordinates.
void draw_display_list(
    QPainter &painter, const timeline::DisplayList &list, const QPalette &palette, bool focused, int label_width);

} // namespace timeline_qt
