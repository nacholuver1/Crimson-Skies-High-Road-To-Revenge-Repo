#pragma once
// AI-upscaled cutscenes, made on the player's PC: CrimsonSkies.exe --ai-movies [--game=<path>] downloads the tools
// (Real-ESRGAN ncnn Vulkan and FFmpeg, both checked by SHA-256) into tools\, then turns each of the game's Bink
// movies into movies\ai\<movie>.mp4 at twice the size, which the game draws in place of the movie (sdk gpu/movies.h).
// No window and no questions: it prints "progress <0-1> <text>" lines (RecompLauncher's prepare step and our own
// launcher read them), skips movies already made and exits 0 when all are there.
#include <string>

namespace ai_movies {

bool Wanted();  // --ai-movies or --prepare (RecompLauncher's setup step) on the command line
int Run();      // the whole step (nothing but a progress line for --prepare alone); the process exit code

// what our launcher shows: how many of the movies are made, of how many (0 of 0 without the game)
struct Status {
    int made = 0, total = 0;
};
Status Check();

// our launcher's Make: the step in a child process of this exe, for the game chosen now; Stop ends it (the
// movies made so far stay)
void Start();
void Stop();
bool Running();
double Progress();      // 0-1 while running
std::string Message();  // the step's last words: what it is doing, or why it stopped
bool Failed();          // it ended without making them all

}  // namespace ai_movies
