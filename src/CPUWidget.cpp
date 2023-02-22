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

void CPUWidget::setPenScale(double value)
{
    if (value < 0.5) 
        graphics->set_pen(BAR_G);
    else if (value >= 0.5 && value < 0.8 ) 
        graphics->set_pen(BAR_Y);
    else 
        graphics->set_pen(BAR_R);
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
    int spacing = (width() - 2)  / cpuValues.size(); // Pixel per cpu
    float maxy = height() - 2;

    cpuValuesMax.resize(cpuValues.size());

    for (size_t i = 0; i < cpuValues.size(); ++i)
    {
        // Color scale according to value
        float value = cpuValues[i] / 100.0f;
        setPenScale(value);

        // Compute max
        cpuValuesMax[i]-=0.01; // Slow move down
        if (value > cpuValuesMax[i])
            cpuValuesMax[i] = value;

        // Size in pixel of value
        int sizeval = std::max(1,(int)(maxy * value));
        int sizevalmax = std::max(1,(int)(maxy * cpuValuesMax[i])) + 1;

        // Draw bar
        int x1 = bl.x + 1 + i * spacing;            // left of each bar
        int x2 = bl.x + 1 + i * spacing + spacing-1; // right of each bar
        Point p1(x1, br.y - 1 - sizeval);
        Point p2(x2, br.y - 1);
        graphics->rectangle(Rect(p1, p2));

        // Cursor for max value
        Point pmax1(bl.x + 1 + i * spacing, br.y - 1 - sizevalmax);
        Point pmax2(bl.x + 1 + i * spacing + spacing-1, br.y - 1 - sizevalmax);
        setPenScale(cpuValuesMax[i]);
        graphics->line(pmax1, pmax2);
    }
}
