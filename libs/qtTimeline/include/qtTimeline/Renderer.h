// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/DisplayList.h>

#include <QColor>
#include <QPainter>
#include <QPalette>

#include <map>

namespace timeline_qt
{

/// Application-selected native colors for semantic style roles.
using StyleColors = std::map<timeline::StyleRole, QColor>;

/// Maps a semantic role to native QPalette colors and focus state.
QColor style_color(timeline::StyleRole role, const QPalette &palette, bool focused);

/// Returns an application override or the native default for a style role.
QColor style_color(timeline::StyleRole role, const QPalette &palette, const StyleColors &style_colors, bool focused);

/// Delegates display-list primitives to QPainter in logical widget coordinates.
void draw_display_list(
    QPainter &painter, const timeline::DisplayList &list, const QPalette &palette, bool focused, int label_width);

/// Delegates display-list primitives to QPainter in logical widget coordinates.
void draw_display_list(QPainter &painter, const timeline::DisplayList &list, const QPalette &palette,
    const StyleColors &style_colors, bool focused, int label_width);

} // namespace timeline_qt
