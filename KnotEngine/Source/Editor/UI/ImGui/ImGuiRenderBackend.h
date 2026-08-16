#pragma once

struct ImDrawData;

class IImGuiRenderBackend
{
public:
    IImGuiRenderBackend() = default;
    virtual ~IImGuiRenderBackend() = default;

    IImGuiRenderBackend(const IImGuiRenderBackend&) = delete;
    IImGuiRenderBackend& operator=(const IImGuiRenderBackend&) = delete;
    IImGuiRenderBackend(IImGuiRenderBackend&&) = delete;
    IImGuiRenderBackend& operator=(IImGuiRenderBackend&&) = delete;

    virtual void Startup() = 0;
    virtual void BeginFrame() = 0;
    virtual void Render(ImDrawData* DrawData) = 0;
    virtual void Shutdown() = 0;
};
