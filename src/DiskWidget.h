#pragma once

#include "Widget.h"
#include "Com.h"

#include <vector>

class DiskWidget : public Widget {
public:
    DiskWidget();
 
    void init();
    void draw();

    //! Values as % per code
    void setValues(std::vector<MonitorData::DiskData>& disks) {values = disks;}
protected:
    pimoroni::Pen BAR_R, BAR_Y, BAR_G, BORDER;
    std::vector<MonitorData::DiskData> values;
};
