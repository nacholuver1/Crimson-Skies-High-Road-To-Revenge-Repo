#pragma once

namespace launcher {

extern bool g_show;  // the settings file's launcher: show it at startup

bool Wanted();  // g_show, or --launcher / --no-launcher; never on the game's own relaunch
bool Run();     // the launcher window until the player starts (true) or quits (false); settings saved on each change

}  // namespace launcher
