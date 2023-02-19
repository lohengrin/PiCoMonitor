#include "GraphWidget.h"

#include <sstream>

using namespace pimoroni;

GraphWidget::GraphWidget(double s, Color c, const std::string& label) : 
Widget(),
color(c),
scale(s),
myLabel(label)
{
}

void GraphWidget::init()
{
    if (!graphics) return;

    AXIS = graphics->create_pen( 255, 255, 255);
    VAL = graphics->create_pen(color.r, color.g, color.b);
    VAL_INV = graphics->create_pen(abs(255-color.r), abs(255-color.g), abs(255-color.b));
    BORDER = graphics->create_pen(0,  50, 100);
}

void GraphWidget::pushValue(double val)
{
    values.push_back(val);
    while (values.size() > width()-2)
        values.pop_front();
}

std::string to_string_with_precision(const double a_value, const int n = 6)
{
    std::ostringstream out;
    out.precision(n);
    out << std::fixed << a_value;
    return out.str();
}

void GraphWidget::draw()
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
        textPosition.x = (ul.x+br.x)/2 - myLabel.size()*8;
        textPosition.y = (ul.y+br.y)/2;
        graphics->set_font("sans");
        graphics->text(myLabel, textPosition , 0, 1.0f);
        return;
    }

    // Compute axis
    Point ax, ay, origin;
    origin.x = bl.x + 2;
    origin.y = bl.y - 2;
    ax.x = br.x - 2;
    ax.y = origin.y;
    ay.x = origin.x;
    ay.y = ul.y - 2;

    float maxy = height() - 2;

    graphics->set_pen(VAL);
    for (int i = 0; i < values.size() ; ++i)
    {
        Point p1(origin.x+i+1,origin.y - 1);
        Point p2(origin.x+i+1,origin.y - 1 - (values[i]/scale)*maxy);
        graphics->line(p1,p2);
    }

    graphics->set_pen(AXIS);
    graphics->line(origin,ax);
    graphics->line(origin,ay);

    // Display value
    graphics->set_pen(VAL_INV);
    Point textPosition;
    textPosition.x = (ul.x+br.x)/2 - myLabel.size()*5;
    textPosition.y = 3*(ul.y+br.y)/4;
    graphics->set_font("sans");
    graphics->text(to_string_with_precision(values.back(), 1), textPosition , 0, 0.5f);
}
