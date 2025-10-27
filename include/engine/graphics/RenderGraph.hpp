/* RenderGraph.hpp */
#pragma once
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <memory>
#include "graphics/FrameBuffer.hpp"

using RGHandle = uint32_t;
constexpr RGHandle INVALID_RG_HANDLE = 0;

// ----------------------------------------------------------------------------
// ResourceAccessor - needs to be declared before RenderGraph
// ----------------------------------------------------------------------------
class RenderGraph; // forward declare

class ResourceAccessor {
public:
    ResourceAccessor(RenderGraph* graph) : m_graph(graph) {}

    Framebuffer* GetFramebuffer(const std::string& name);
    GLuint GetTexture(const std::string& name);

private:
    RenderGraph* m_graph;
    friend class RenderGraph;
};

// ----------------------------------------------------------------------------
// RenderGraph
// ----------------------------------------------------------------------------
class RenderGraph {
public:
    struct TextureDesc {
        int width = 0;
        int height = 0;
        GLenum format = GL_RGBA8;
        bool isBackbuffer = false;
    };

    RenderGraph() = default;
    ~RenderGraph() = default;

    // Creates and owns the framebuffer
    RGHandle CreateTexture(const std::string& name, const TextureDesc& desc);

    // Get resource by name (for accessor)
    Framebuffer* GetFramebuffer(const std::string& name);

    // Add a pass with string-based dependencies
    void AddPass(const std::string& name,
        const std::vector<std::string>& readNames,
        const std::vector<std::string>& writeNames,
        std::function<void(ResourceAccessor&)> execute);

    // Execute all passes in order
    void Execute();

    // Clear passes (resources persist)
    void Reset();

private:
    struct Resource {
        std::string name;
        TextureDesc desc;
        std::unique_ptr<Framebuffer> fbo;
    };

    struct Pass {
        std::string name;
        std::vector<RGHandle> reads;
        std::vector<RGHandle> writes;
        std::function<void(ResourceAccessor&)> execute;
    };

    RGHandle m_nextHandle = 1;
    std::unordered_map<RGHandle, Resource> m_resources;
    std::unordered_map<std::string, RGHandle> m_nameToHandle;
    std::vector<Pass> m_passes;

    friend class ResourceAccessor;
};