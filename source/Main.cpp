#include <cstdio>

#include <SDL2/SDL.h>

#include "Emulation/Controller.hpp"
#include "SMB/SMBEngine.hpp"
#include "Util/Video.hpp"

#include "Constants.hpp"

uint8_t* romImage;
static SDL_Window* window;
static SDL_Renderer* renderer;
static SDL_Texture* texture;
static SDL_Texture* scanlineTexture;
static SMBEngine* smbEngine = nullptr;
static uint32_t renderBuffer[RENDER_WIDTH * RENDER_HEIGHT];

/**
 * Load the Super Mario Bros. ROM image.
 */
static bool loadRomImage()
{
    // TODO: this should be configurable
    FILE* file = fopen(ROM_FILENAME, "r");
    if (file == NULL)
    {
        return false;
    }

    // Find the size of the file
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    fseek(file, 0L, SEEK_SET);

    // Read the entire file into a buffer
    romImage = new uint8_t[fileSize];
    fread(romImage, sizeof(uint8_t), fileSize, file);
    fclose(file);

    return true;
}

/**
 * SDL Audio callback function.
 */
static void audioCallback(void* userdata, uint8_t* buffer, int len)
{
    if (smbEngine != nullptr)
    {
        smbEngine->audioCallback(buffer, len);
    }
}

/**
 * Initialize libraries for use.
 */
static bool initialize()
{
    // Load the SMB ROM image
    if (!loadRomImage())
    {
        return false;
    }

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
    {
        return false;
    }

    // Create the window
    window = SDL_CreateWindow(APP_TITLE,
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              RENDER_WIDTH * RENDER_SCALE,
                              RENDER_HEIGHT * RENDER_SCALE,
                              0
    );

    // Setup the renderer and texture buffer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    SDL_RenderSetLogicalSize(renderer, RENDER_WIDTH, RENDER_HEIGHT);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, RENDER_WIDTH, RENDER_HEIGHT);

    scanlineTexture = generateScanlineTexture(renderer);

    // Initialize audio
    SDL_AudioSpec desiredSpec;
    desiredSpec.freq = AUDIO_FREQUENCY_HZ;
    desiredSpec.format = AUDIO_S8;
    desiredSpec.channels = 1;
    desiredSpec.samples = 2048;
    desiredSpec.callback = audioCallback;
    desiredSpec.userdata = NULL;

    SDL_AudioSpec obtainedSpec;
    SDL_OpenAudio(&desiredSpec, &obtainedSpec);

    // Start playing audio
    SDL_PauseAudio(0);

    return true;
}

/**
 * Shutdown libraries for exit.
 */
static void shutdown()
{
    SDL_CloseAudio();

    SDL_DestroyTexture(scanlineTexture);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
}

static void mainLoop()
{
    SMBEngine engine(romImage);
    smbEngine = &engine;
    engine.reset();

    bool running = true;
    int progStartTime = SDL_GetTicks();
    int frame = 0;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_WINDOWEVENT:
                switch (event.window.event)
                {
                case SDL_WINDOWEVENT_CLOSE:
                    running = false;
                    break;
                }
                break;

            default:
                break;
            }
        }

        const Uint8* keys = SDL_GetKeyboardState(NULL);
        Controller& controller1 = engine.getController1();
        controller1.setButtonState(BUTTON_A, keys[SDL_SCANCODE_X]);
        controller1.setButtonState(BUTTON_B, keys[SDL_SCANCODE_Z]);
        controller1.setButtonState(BUTTON_SELECT, keys[SDL_SCANCODE_BACKSPACE]);
        controller1.setButtonState(BUTTON_START, keys[SDL_SCANCODE_RETURN]);
        controller1.setButtonState(BUTTON_UP, keys[SDL_SCANCODE_UP]);
        controller1.setButtonState(BUTTON_DOWN, keys[SDL_SCANCODE_DOWN]);
        controller1.setButtonState(BUTTON_LEFT, keys[SDL_SCANCODE_LEFT]);
        controller1.setButtonState(BUTTON_RIGHT, keys[SDL_SCANCODE_RIGHT]);

        if (keys[SDL_SCANCODE_R])
        {
            // Reset
            engine.reset();
        }
        if (keys[SDL_SCANCODE_ESCAPE])
        {
            // quit
            running = false;
            break;
        }
        if (keys[SDL_SCANCODE_F])
        {
            SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        }

        engine.update();
        engine.render(renderBuffer);

        SDL_UpdateTexture(texture, NULL, renderBuffer, sizeof(uint32_t) * RENDER_WIDTH);

        SDL_RenderClear(renderer);

        // Render the screen
        SDL_RenderSetLogicalSize(renderer, RENDER_WIDTH, RENDER_HEIGHT);
        SDL_RenderCopy(renderer, texture, NULL, NULL);

        // Render scanlines
        SDL_RenderSetLogicalSize(renderer, RENDER_WIDTH * 3, RENDER_HEIGHT * 3);
        SDL_RenderCopy(renderer, scanlineTexture, NULL, NULL);

        SDL_RenderPresent(renderer);

        /**
         * Ensure that the framerate stays as close to 60FPS as possible. If the frame was rendered faster, then delay. 
         * If the frame was slower, reset time so that the game doesn't try to "catch up", going super-speed.
         */
        int now = SDL_GetTicks();
        int delay = progStartTime + int(double(frame) * double(MS_PER_SEC) / double(FPS)) - now;
        if(delay > 0) 
        {
            SDL_Delay(delay);
        }
        else 
        {
            frame = 0;
            progStartTime = now;
        }
        frame++;
    }
}

int main(int argc, char** argv)
{
    if (!initialize())
    {
        return -1;
    }

    mainLoop();

    shutdown();

    return 0;
}
