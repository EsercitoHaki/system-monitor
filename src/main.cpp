#include "system_data.h"
#include "gui_manager.h"

int main(int argc, char* argv[]) {
    SystemData sys_data;
    GUIManager gui_manager(sys_data);
    gui_manager.run();

    return 0;
}