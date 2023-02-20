#include "NullWidget.h"

using namespace pimoroni;

int NullWidget::BORDER = -1;
int NullWidget::CROSS  = -1;
int NullWidget::LABEL  = -1;

NullWidget::NullWidget(const std::string label) :
Widget(),
myLabel(label)
{
}

void NullWidget::init()
{
    if (!graphics) return;

    // Initialize static pens (shared accros instances)
    if (BORDER == -1) BORDER = graphics->create_pen(0,  25, 50);
    if (CROSS == -1)  CROSS  = graphics->create_pen(150,  25, 50);
    if (LABEL == -1)  LABEL  = graphics->create_pen(255,  255, 255);
}

void NullWidget::draw()
{
    if (!graphics) return;

    graphics->set_pen(BORDER);
    graphics->rectangle(Rect(ul,br));

    graphics->set_pen(CROSS);
    graphics->line(ul,br);
    graphics->line(bl,ur);

    graphics->set_pen(LABEL);
    graphics->set_font("sans");
    Point textPosition;
    textPosition.x = (ul.x+br.x)/2 - myLabel.size()*8;
    textPosition.y = (ul.y+br.y)/2;
    
    graphics->text(myLabel, textPosition , 0, 1.0f);
}
