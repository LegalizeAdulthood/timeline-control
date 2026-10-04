// Copyright (c) 2026 Richard Thomson

#include <Viewer.h>

#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <memory>
#include <mutex>
#include <stdexcept>
#include <string_view>

namespace
{

using timeline_imgui_viewer::Command;
using timeline_imgui_viewer::Viewer;

/// Initializes SDL and keeps it alive until all host resources are destroyed.
///
class SdlRuntime
{
public:
    SdlRuntime();
    ~SdlRuntime()
    {
        SDL_Quit();
    }
};

SdlRuntime::SdlRuntime()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        throw std::runtime_error(SDL_GetError());
    }
}

/// Thread-safe handoff of an asynchronous native dialog's owned result.
///
struct DialogResult
{
    std::mutex mutex;
    bool ready{false};
    Command command{Command::NONE};
    std::filesystem::path path;
    std::string error;
};

void SDLCALL dialog_finished(void *userdata, const char *const *files, int)
{
    const std::unique_ptr<std::shared_ptr<DialogResult>> owner(static_cast<std::shared_ptr<DialogResult> *>(userdata));
    const std::shared_ptr<DialogResult> &result = *owner;
    const std::lock_guard<std::mutex> lock(result->mutex);
    if (!files)
    {
        result->error = SDL_GetError();
    }
    else if (*files)
    {
        result->path = std::filesystem::u8path(*files);
    }
    result->ready = true;
}

/// Owns native rendering resources and shuts down backends before their context.
///
class Host
{
public:
    Host();
    ~Host();
    void run(Viewer &viewer, bool smoke);

private:
    void request_dialog(Command command);
    void receive_dialog(Viewer &viewer);
    void show_diagnostics(const Viewer &viewer);
    bool rendered_pixels() const;

    SdlRuntime m_runtime;
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> m_window{nullptr, SDL_DestroyWindow};
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> m_renderer{nullptr, SDL_DestroyRenderer};
    std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> m_context{nullptr, ImGui::DestroyContext};
    bool m_platform_ready{false};
    bool m_renderer_ready{false};
    std::shared_ptr<DialogResult> m_dialog;
};

Host::Host()
{
    m_window.reset(
        SDL_CreateWindow("ImGui Timeline Viewer", 1100, 700, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY));
    if (!m_window)
    {
        throw std::runtime_error(SDL_GetError());
    }
    m_renderer.reset(SDL_CreateRenderer(m_window.get(), nullptr));
    if (!m_renderer)
    {
        throw std::runtime_error(SDL_GetError());
    }
    SDL_SetRenderVSync(m_renderer.get(), 1);
    IMGUI_CHECKVERSION();
    m_context.reset(ImGui::CreateContext());
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsLight();
    ImFontConfig font_config;
    font_config.SizePixels = 16.0F;
    io.Fonts->AddFontDefaultVector(&font_config);
    const float scale = SDL_GetWindowDisplayScale(m_window.get());
    ImGui::GetStyle().ScaleAllSizes(scale);
    ImGui::GetStyle().FontScaleDpi = scale;
    m_platform_ready = ImGui_ImplSDL3_InitForSDLRenderer(m_window.get(), m_renderer.get());
    if (!m_platform_ready)
    {
        throw std::runtime_error("Unable to initialize the SDL3 ImGui backend.");
    }
    m_renderer_ready = ImGui_ImplSDLRenderer3_Init(m_renderer.get());
    if (!m_renderer_ready)
    {
        ImGui_ImplSDL3_Shutdown();
        m_platform_ready = false;
        throw std::runtime_error("Unable to initialize the SDL3 ImGui renderer.");
    }
}

Host::~Host()
{
    if (m_renderer_ready)
    {
        ImGui_ImplSDLRenderer3_Shutdown();
    }
    if (m_platform_ready)
    {
        ImGui_ImplSDL3_Shutdown();
    }
}

void Host::request_dialog(Command command)
{
    m_dialog = std::make_shared<DialogResult>();
    m_dialog->command = command;
    // The callback owns a reference even if the host exits before it arrives.
    std::unique_ptr<std::shared_ptr<DialogResult>> owner = std::make_unique<std::shared_ptr<DialogResult>>(m_dialog);
    static constexpr SDL_DialogFileFilter json_filter[]{{"JSON files", "json"}};
    static constexpr SDL_DialogFileFilter snapshot_filter[]{{"Text snapshots", "txt"}};
    if (command == Command::EXPORT)
    {
        SDL_ShowSaveFileDialog(dialog_finished, owner.release(), m_window.get(), snapshot_filter, 1, "timeline.txt");
    }
    else
    {
        SDL_ShowOpenFileDialog(dialog_finished, owner.release(), m_window.get(), json_filter, 1, nullptr, false);
    }
}

void Host::show_diagnostics(const Viewer &viewer)
{
    if (viewer.diagnostics().empty())
    {
        return;
    }
    std::string message;
    for (const std::string &diagnostic : viewer.diagnostics())
    {
        message += diagnostic + '\n';
    }
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Timeline diagnostics", message.c_str(), m_window.get());
}

void Host::receive_dialog(Viewer &viewer)
{
    if (!m_dialog)
    {
        return;
    }
    const std::shared_ptr<DialogResult> result = m_dialog;
    const std::lock_guard<std::mutex> lock(result->mutex);
    if (!result->ready)
    {
        return;
    }
    m_dialog.reset();
    if (!result->error.empty())
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "File dialog failed", result->error.c_str(), m_window.get());
    }
    else if (!result->path.empty())
    {
        if (result->command == Command::EXPORT)
        {
            viewer.export_snapshot(result->path);
        }
        else
        {
            viewer.load_file(result->path, result->command == Command::ADD);
            if (viewer.control().document())
            {
                const std::string title = "ImGui Timeline Viewer - " + viewer.control().document()->metadata().title();
                SDL_SetWindowTitle(m_window.get(), title.c_str());
            }
        }
        show_diagnostics(viewer);
    }
}

bool Host::rendered_pixels() const
{
    const std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(
        SDL_RenderReadPixels(m_renderer.get(), nullptr), SDL_DestroySurface);
    if (!surface)
    {
        return false;
    }
    for (int y = 0; y < surface->h; y += 11)
    {
        for (int x = 0; x < surface->w; x += 13)
        {
            Uint8 r = 0, g = 0, b = 0;
            if (SDL_ReadSurfacePixel(surface.get(), x, y, &r, &g, &b, nullptr) && r < 100 && g < 100 && b < 100)
            {
                return true;
            }
        }
    }
    return false;
}

void Host::run(Viewer &viewer, bool smoke)
{
    bool quit = false;
    int frames = 0;
    bool right_arrow_received = false;
    if (smoke)
    {
        SDL_Event arrow{};
        arrow.type = SDL_EVENT_KEY_DOWN;
        arrow.key.windowID = SDL_GetWindowID(m_window.get());
        arrow.key.scancode = SDL_SCANCODE_RIGHT;
        arrow.key.key = SDLK_RIGHT;
        arrow.key.down = true;
        if (!SDL_PushEvent(&arrow))
        {
            throw std::runtime_error(SDL_GetError());
        }
    }
    while (!quit || m_dialog)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                    event.window.windowID == SDL_GetWindowID(m_window.get())))
            {
                quit = true;
            }
        }
        receive_dialog(viewer);
        if (SDL_GetWindowFlags(m_window.get()) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        if (smoke)
        {
            right_arrow_received = right_arrow_received || ImGui::IsKeyPressed(ImGuiKey_RightArrow);
        }
        const Command command = timeline_imgui_viewer::draw_viewer(viewer, m_dialog != nullptr);
        if (command == Command::EXIT)
        {
            quit = true;
        }
        else if (command != Command::NONE)
        {
            request_dialog(command);
        }
        ImGui::Render();
        const ImGuiIO &io = ImGui::GetIO();
        SDL_SetRenderScale(m_renderer.get(), io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(m_renderer.get(), 240, 240, 240, 255);
        SDL_RenderClear(m_renderer.get());
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_renderer.get());
        if (smoke && ++frames == 4)
        {
            if (!right_arrow_received || !viewer.control().layout() || ImGui::GetDrawData()->TotalVtxCount == 0 ||
                !rendered_pixels())
            {
                throw std::runtime_error("The viewer did not render timeline content.");
            }
            quit = true;
        }
        SDL_RenderPresent(m_renderer.get());
    }
}

} // namespace

int main(int argc, char **argv)
{
    try
    {
        const bool smoke = argc == 3 && std::string_view(argv[1]) == "--smoke-test";
        if (argc > 1 && !smoke && argc != 2)
        {
            throw std::runtime_error("Expected an optional JSON path.");
        }
        Viewer viewer;
        if (argc > 1 && !viewer.load_file(std::filesystem::u8path(argv[smoke ? 2 : 1]), false))
        {
            throw std::runtime_error(
                viewer.diagnostics().empty() ? "Timeline import failed" : viewer.diagnostics().front());
        }
        Host host;
        host.run(viewer, smoke);
        return 0;
    }
    catch (const std::exception &error)
    {
        SDL_Log("%s", error.what());
        return 1;
    }
}
