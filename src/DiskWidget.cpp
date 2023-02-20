#include "DiskWidget.h"

using namespace pimoroni;

DiskWidget::DiskWidget() : Widget()
{
}

void DiskWidget::init()
{
    if (!graphics) return;

    BAR_G = graphics->create_pen( 32, 180, 96);
    BAR_Y = graphics->create_pen(230, 126, 34);
    BAR_R = graphics->create_pen(255,  20, 15);
    BORDER = graphics->create_pen(0,  50, 100);
}

void DiskWidget::draw()
{
    if (!graphics) return;

    // Draw borders
    graphics->set_pen(BORDER);
    graphics->line(ul,ur);
    graphics->line(ur,br);
    graphics->line(br,bl);
    graphics->line(bl,ul);

    // If no data, display only title "CPU"
    if (values.empty())
    {
        Point textPosition;
        textPosition.x = (ul.x+br.x)/2 - 5*8;
        textPosition.y = (ul.y+br.y)/2;
        graphics->set_font("sans");
        graphics->text("Disks", textPosition , 0, 1.0f);
        return;
    }

    // Display CPU bars, with color change according to level
    int spacing = height() / values.size();
    float maxx = width() - 22;

    for (size_t i = 0; i < values.size(); ++i)
    {
        auto value = values[i];
        float ratio = value.used / value.total;

        graphics->set_pen(BORDER);
        Point p1(bl.x + 22          , br.y - spacing/2 - spacing*i);
        Point p2(bl.x + 22 + width(), br.y - spacing/2 - spacing*i);
        graphics->thick_line(p1, p2, spacing/2);


        if (ratio < 0.5) 
            graphics->set_pen(BAR_G);
        else if (ratio >= 0.5 && ratio < 0.8 ) 
            graphics->set_pen(BAR_Y);
        else 
            graphics->set_pen(BAR_R);


        int sizeval = std::max(1,(int)(maxx * ratio));

        Point p3(bl.x + 22 + sizeval, br.y - spacing/2 - spacing*i);
        graphics->thick_line(p1, p3, spacing/2);

        Point textPosition;
        std::string l = value.label.substr(0, 1);
        textPosition.x = bl.x + 4;
        textPosition.y = br.y - spacing/2 - spacing*i;
        graphics->set_font("sans");
        graphics->text(l, textPosition , 0, 0.4f);
    }
}
