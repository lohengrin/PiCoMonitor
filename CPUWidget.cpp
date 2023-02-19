#include "CPUWidget.h"

using namespace pimoroni;

CPUWidget::CPUWidget() : Widget()
{
}

void CPUWidget::init()
{
    if (!graphics) return;

    BAR_G = graphics->create_pen( 32, 180, 96);
    BAR_Y = graphics->create_pen(230, 126, 34);
    BAR_R = graphics->create_pen(255,  20, 15);
    BORDER = graphics->create_pen(0,  50, 100);
}

void CPUWidget::draw()
{
    if (!graphics) return;

    // Draw borders
    graphics->set_pen(BORDER);
    graphics->line(ul,ur);
    graphics->line(ur,br);
    graphics->line(br,bl);
    graphics->line(bl,ul);

    // If no data, display only title "CPU"
    if (cpuValues.empty())
    {
        Point textPosition;
        textPosition.x = (ul.x+br.x)/2 - 3*8;
        textPosition.y = (ul.y+br.y)/2;
        graphics->set_font("sans");
        graphics->text("CPU", textPosition , 0, 1.0f);
        return;
    }

    // Display CPU bars, with color change according to level
    int spacing = width() / cpuValues.size();
    float maxy = height() - 2;

    for (size_t i = 0; i < cpuValues.size(); ++i)
    {
        float value = cpuValues[i] / 100.0f;
        if (value < 0.5) 
            graphics->set_pen(BAR_G);
        else if (value >= 0.5 && value < 0.8 ) 
            graphics->set_pen(BAR_Y);
        else 
            graphics->set_pen(BAR_R);

        int sizeval = (int)(maxy * value);

        Point p1(spacing + 1 + bl.x + i * spacing, br.y - 1);
        Point p2(spacing + 1 + bl.x + i * spacing, br.y - 1 - sizeval);
        graphics->thick_line(p1, p2, spacing-1);
    }
}
