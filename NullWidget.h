#pragma once

#include "Widget.h"

class NullWidget : public Widget 
{
public:
    NullWidget(const std::string label);

    void init();
    void draw();

protected:
    static pimoroni::Pen BORDER, LABEL, CROSS;
    std::string myLabel;
};
