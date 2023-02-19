#pragma once

#include "Widget.h"

#include <deque>

class GraphWidget : public Widget {
public:
    GraphWidget(double scale, Color color, const std::string& label);
 
    void init();
    void draw();

    //! Values as % per code
    void pushValue(double val);
protected:
    pimoroni::Pen AXIS, VAL, VAL_FILL, BORDER, VAL_INV;
    std::deque<double> values;
    double scale;
    Color color;
    std::string myLabel;
};
