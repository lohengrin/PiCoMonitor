#pragma once

#include "Widget.h"

#include <vector>

class CPUWidget : public Widget {
public:
    CPUWidget();
 
    void init();
    void draw();

    //! Values as % per code
    void setValues(std::vector<double>& cpus) {cpuValues = cpus;}
protected:
    void setPenScale(double value);

    pimoroni::Pen BAR_R, BAR_Y, BAR_G, BORDER;
    std::vector<double> cpuValues;
    std::vector<double> cpuValuesMax;
};
