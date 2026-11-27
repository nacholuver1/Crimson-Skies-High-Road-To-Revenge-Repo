#pragma once
// AI upscaling made on the player's PC, never shipped. No window and no questions: the steps print
// "progress <0-1> <text>" lines (RecompLauncher's prepare step and our own launcher read them), keep what an earlier
// run made and exit 0 when all is there.
// - CrimsonSkies.exe --ai-movies [--game=<path>]: downloads Real-ESRGAN ncnn Vulkan and FFmpeg (SHA-256 checked)
//   into tools\, then turns each Bink movie into movies\ai\<movie>.mp4 at twice the size, drawn in its place
//   (sdk gpu/movies.h).
// - CrimsonSkies.exe --ai-textures [--game=<path>]: every texture in the game's level bundles, upscaled 2x into
//   textures\ai\ under the key the game gives it (sdk gpu/texturedumper.h); drawn when ai_textures is on. Menus,
//   the HUD and data textures are left as they are.
#include <string>

namespace ai_movies {

bool Wanted();  // --ai-movies, --ai-textures or --prepare (RecompLauncher's setup step) on the command line
int Run();      // the steps asked for (nothing but a progress line for --prepare alone); the process exit code

// what our launcher shows: how many of the movies are made, of how many (0 of 0 without the game)
struct Status {
    int made = 0, total = 0;
};
Status Check();
bool TexturesReady();  // textures\ai\ is complete for the current recipe

// our launcher's Make: one step at a time in a child process of this exe, for the game chosen now; Stop ends it
// (what is made so far stays)
enum class Job { Movies, Textures };
void Start(Job job);
void Stop();
bool Running();
Job Current();          // the step started last
double Progress();      // 0-1 while running
std::string Message();  // the step's last words: what it is doing, or why it stopped
bool Failed();          // it ended without making all

}  // namespace ai_movies
