#include "Chip8.hpp"
#include <stdio.h>
#include <filesystem>
#include <vector>
#include <chrono>
#include <string>
#if __has_include(<SDL.h>)
#include <SDL.h>
#elif __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#else
#error "SDL header not found"
#endif
#include <iostream>

const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 320;

static std::string findDefaultRomPath(const char* argv0) {
    namespace fs = std::filesystem;

    std::vector<fs::path> searchRoots;
    searchRoots.push_back(fs::current_path());

    if (argv0 != nullptr && std::string(argv0).size() > 0) {
        fs::path exePath(argv0);
        if (exePath.has_parent_path()) {
            searchRoots.push_back(exePath.parent_path());
        }
    }

    const std::vector<std::string> candidateNames = {
        "Pong (1 player).ch8",
        "Pong__1_player_.ch8",
        "Pong.ch8"
    };

    for (const auto& root : searchRoots) {
        for (const auto& name : candidateNames) {
            fs::path fullPath = root / name;
            if (fs::exists(fullPath) && fs::is_regular_file(fullPath)) {
                return fullPath.string();
            }
        }
    }

    return "";
}

int main(int argc, char* argv[]) {
    Chip8 myChip8;

    std::string romPath;
    int cpuHz = 90; // Slower default for better playability.
    if (argc > 1) {
        romPath = argv[1];
        if (argc > 2) {
            cpuHz = std::stoi(argv[2]);
            if (cpuHz <= 0) {
                std::cerr << "Invalid CPU Hz: " << argv[2] << std::endl;
                return 1;
            }
        }
    } else {
        romPath = findDefaultRomPath(argv[0]);
        if (romPath.empty()) {
            std::cerr << "No ROM specified and no default ROM found in current directory." << std::endl;
            std::cerr << "Try: ./chip8 \"Pong (1 player).ch8\" 90" << std::endl;
            return 1;
        }
        std::cout << "Using default ROM: " << romPath << std::endl;
    }
    const bool pongFriendlyControls =
        (romPath.find("Pong") != std::string::npos) ||
        (romPath.find("pong") != std::string::npos);

    std::cout << "CPU speed: " << cpuHz << " Hz" << std::endl;
    std::cout << "Controls: '-' slower, '=' faster" << std::endl;
    if (pongFriendlyControls) {
        std::cout << "Pong controls: Up/W/1 = up, Down/S/Q/4 = down" << std::endl;
    }

    if (!myChip8.loadROM(romPath)) {
        std::cerr << "ROM load failed, exiting." << std::endl;
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Chip-8 Emulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 64, 32);
    if (!texture) {
        std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool quit = false;
    SDL_Event e;
    auto lastTick = std::chrono::high_resolution_clock::now();
    double cpuAccumulator = 0.0;
    double timerAccumulator = 0.0;
    const double timerHz = 60.0;
    int titleUpdateCounter = 0;

    while (!quit) {
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = now - lastTick;
        lastTick = now;

        cpuAccumulator += elapsed.count() * static_cast<double>(cpuHz);
        timerAccumulator += elapsed.count() * timerHz;

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) quit = true;

            if (e.type == SDL_KEYDOWN) {
                const auto sym = e.key.keysym.sym;
                if (sym == SDLK_MINUS) {
                    cpuHz -= 10;
                    if (cpuHz < 30) cpuHz = 30;
                    std::cout << "CPU speed: " << cpuHz << " Hz" << std::endl;
                } else if (sym == SDLK_EQUALS) {
                    cpuHz += 10;
                    if (cpuHz > 1000) cpuHz = 1000;
                    std::cout << "CPU speed: " << cpuHz << " Hz" << std::endl;
                }
            }
        }

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        auto isDown = [&](SDL_Scancode sc) -> uint8_t { return keys[sc] ? 1 : 0; };

        myChip8.keypad[0x1] = isDown(SDL_SCANCODE_1) || isDown(SDL_SCANCODE_KP_1);
        myChip8.keypad[0x2] = isDown(SDL_SCANCODE_2) || isDown(SDL_SCANCODE_KP_2);
        myChip8.keypad[0x3] = isDown(SDL_SCANCODE_3) || isDown(SDL_SCANCODE_KP_3);
        myChip8.keypad[0xC] = isDown(SDL_SCANCODE_4) || isDown(SDL_SCANCODE_KP_4);
        myChip8.keypad[0x4] = isDown(SDL_SCANCODE_Q);
        myChip8.keypad[0x5] = isDown(SDL_SCANCODE_W);
        myChip8.keypad[0x6] = isDown(SDL_SCANCODE_E);
        myChip8.keypad[0xD] = isDown(SDL_SCANCODE_R);
        myChip8.keypad[0x7] = isDown(SDL_SCANCODE_A);
        myChip8.keypad[0x8] = isDown(SDL_SCANCODE_S);
        myChip8.keypad[0x9] = isDown(SDL_SCANCODE_D);
        myChip8.keypad[0xE] = isDown(SDL_SCANCODE_F);
        myChip8.keypad[0xA] = isDown(SDL_SCANCODE_Z);
        myChip8.keypad[0x0] = isDown(SDL_SCANCODE_X);
        myChip8.keypad[0xB] = isDown(SDL_SCANCODE_C);
        myChip8.keypad[0xF] = isDown(SDL_SCANCODE_V);

        if (pongFriendlyControls) {
            if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_1]) {
                myChip8.keypad[0x1] = 1;
            }
            if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S] ||
                keys[SDL_SCANCODE_Q] || keys[SDL_SCANCODE_4]) {
                myChip8.keypad[0x4] = 1;
            }

            titleUpdateCounter++;
            if (titleUpdateCounter >= 6) {
                titleUpdateCounter = 0;
                std::string title = "Chip-8 Emulator  K1=" +
                    std::to_string(static_cast<int>(myChip8.keypad[0x1])) +
                    " K4=" + std::to_string(static_cast<int>(myChip8.keypad[0x4]));
                SDL_SetWindowTitle(window, title.c_str());
            }
        }

        while (cpuAccumulator >= 1.0) {
            myChip8.cycle();
            cpuAccumulator -= 1.0;
        }

        while (timerAccumulator >= 1.0) {
            if (myChip8.delayTimer > 0) myChip8.delayTimer--;
            if (myChip8.soundTimer > 0) myChip8.soundTimer--;
            timerAccumulator -= 1.0;
        }

        if (myChip8.drawFlag) {
            uint32_t pixels[2048];
            for (int i = 0; i < 2048; ++i) {
                pixels[i] = (myChip8.display[i] == 1) ? 0xFFFFFFFF : 0xFF000000;
            }
            SDL_UpdateTexture(texture, NULL, pixels, 64 * sizeof(uint32_t));
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
            myChip8.drawFlag = false;
        }

        SDL_Delay(1);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
