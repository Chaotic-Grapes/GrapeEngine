/* Start Header *****************************************************************/
/*!
\file   RenderGraph.hpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\date   31st October 2025
\brief
Declares the RenderGraph and ResourceAccessor classes, which together provide
a high-level interface for constructing and executing multi-pass rendering
pipelines.

The RenderGraph organizes framebuffer-based resources (textures, render targets)
into named nodes and manages the order of execution for render passes. Each
pass defines explicit read and write dependencies to ensure correct data flow
between passes.

Although passes currently execute in submission order, this abstraction allows
future dependency-based scheduling, automatic resource reuse, and flexible
pipeline configuration without modifying the RendererSystem directly.

The ResourceAccessor class provides a convenient API for retrieving framebuffers
and textures from within a render pass.
*/
/* End Header *******************************************************************/

#pragma once
#include "Export.h"
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <memory>
#include "graphics/FrameBuffer.hpp"

using RGHandle = uint32_t;
constexpr RGHandle INVALID_RG_HANDLE = 0;

class RenderGraph; // forward declaration

// Provides access to framebuffer resources managed by the RenderGraph.
// Used inside render pass callbacks to fetch textures or FBOs.
class GRAPEENGINE_API ResourceAccessor {
public:
    // ============================================================================
    // Constructs the accessor and binds it to a RenderGraph instance.
    // ============================================================================
    ResourceAccessor(RenderGraph* graph) : m_graph(graph) {}

    // ============================================================================
    // Returns a pointer to the framebuffer associated with the given name.
    // ============================================================================
    Framebuffer* GetFramebuffer(const std::string& name);

    // ============================================================================
    // Returns the OpenGL texture ID of the framebuffer�s first color attachment.
    // ============================================================================
    GLuint GetTexture(const std::string& name);

private:
    RenderGraph* m_graph;
    friend class RenderGraph;
};

// Manages framebuffer resources and orchestrates ordered rendering passes.
// Each pass defines which textures it reads from and writes to, allowing
// clear, data-driven control over the render pipeline.
class GRAPEENGINE_API RenderGraph {
public:
    // ----------------------------------------------------------------------------
    // Describes the properties of a texture or render target.
    // ----------------------------------------------------------------------------
    struct TextureDesc {
        int width = 0;
        int height = 0;
        GLenum format = GL_RGBA8;
        bool isBackbuffer = false;
        int colorAttachmentCount = 1;
    };

    RenderGraph() = default;
    ~RenderGraph() = default;

    // Delete copy operations (contains unique_ptr)
    RenderGraph(const RenderGraph&) = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;

    // Allow move operations
    RenderGraph(RenderGraph&&) noexcept = default;
    RenderGraph& operator=(RenderGraph&&) noexcept = default;

    // ============================================================================
    // Creates a framebuffer resource and registers it with the RenderGraph.
    // Returns a unique handle used for dependency tracking.
    // ============================================================================
    RGHandle CreateTexture(const std::string& name, const TextureDesc& desc);

    // ============================================================================
    // Retrieves a framebuffer by its logical name.
    // Used internally by ResourceAccessor for resource lookups.
    // ============================================================================
    Framebuffer* GetFramebuffer(const std::string& name);

    // ============================================================================
    // Adds a render pass that specifies which resources it reads and writes.
    // The execute callback defines the rendering logic for that pass.
    // ============================================================================
    void AddPass(const std::string& name,
        const std::vector<std::string>& readNames,
        const std::vector<std::string>& writeNames,
        std::function<void(ResourceAccessor&)> execute);

    // ============================================================================
    // Executes all registered passes sequentially in submission order.
    // Each pass receives a ResourceAccessor for resource access.
    // ============================================================================
    void Execute();

    // ============================================================================
    // Clears the list of passes while preserving framebuffer resources.
    // This prevents unnecessary FBO recreation and reduces CPU overhead
    // by reusing GPU memory across frames.
    // ============================================================================
    void Reset();

private:
    // ----------------------------------------------------------------------------
    // Internal structure representing a managed framebuffer resource.
    // ----------------------------------------------------------------------------
    struct Resource {
        std::string name;
        TextureDesc desc;
        std::unique_ptr<Framebuffer> fbo;

        // Default constructor
        Resource() = default;

        // Move constructor and assignment (required for unique_ptr)
        Resource(Resource&&) noexcept = default;
        Resource& operator=(Resource&&) noexcept = default;

        // Delete copy operations
        Resource(const Resource&) = delete;
        Resource& operator=(const Resource&) = delete;
    };

    // ----------------------------------------------------------------------------
    // Represents a single render pass within the graph.
    // Each pass contains its dependency handles and execution callback.
    // ----------------------------------------------------------------------------
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
