#pragma once

#include "pico_toolset/widget.h"

#include <string>
#include <utility>
#include <vector>

/// @brief Bordered list of "label ........ value" rows (label left, value right),
/// for text-only information such as the System and GPU pages. The text size
/// (1x or 2x) is chosen so every row fits.
class InfoListWidget : public pico_toolset::Widget {
public:
    using Row = std::pair<std::string, std::string>;

    //! @param title shown centered, in the border color, while there are no rows
    InfoListWidget(int x, int y, int w, int h, const char* title)
        : m_x(x), m_y(y), m_w(w), m_h(h), m_title(title) {}

    void setRows(std::vector<Row> rows) { m_rows = std::move(rows); }

    void draw(pico_toolset::DisplayDriver& display) const override;

private:
    int m_x, m_y, m_w, m_h;
    std::string m_title;
    std::vector<Row> m_rows;
};
