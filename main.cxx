#include "Gui.h"

int main(int ac, char **av)
{
    Gui gui;

    Fl_Double_Window *w = gui.make_window();
    w->end();
    w->show();

    return Fl::run();
}
