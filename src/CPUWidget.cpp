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
    int spacing = width() / cpuValues.size();
    float maxy = height() - 2;

    cpuValuesMax.resize(cpuValues.size());

    for (size_t i = 0; i < cpuValues.size(); ++i)
    {
        // Color scale according to value
        float value = cpuValues[i] / 100.0f;
        setPenScale(value);

        // Compute max
        cpuValuesMax[i]-=0.01;

        if (value > cpuValuesMax[i])
            cpuValuesMax[i] = value;

        int sizeval = std::max(1,(int)(maxy * value));
        int sizevalmax = std::max(1,(int)(maxy * cpuValuesMax[i])) + 1;

        // Draw bar
        for (int s = 0; s < spacing-1; s++)
        {
            int x = spacing + 1 + bl.x + s + i * spacing - spacing/2;
            Point p1(x, br.y - 1);
            Point p2(x, br.y - 1 - sizeval);
            graphics->line(p1, p2);
        }

        // Cursor for max value
        Point pmax1(spacing + 1 + bl.x + i * spacing - spacing/2, br.y - 1 - sizevalmax);
        Point pmax2(spacing + 1 + bl.x + i * spacing + spacing/2, br.y - 1 - sizevalmax);
        setPenScale(cpuValuesMax[i]);
        graphics->line(pmax1, pmax2);
    }
}
