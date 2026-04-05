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
    /**
     * @brief Construct a resource accessor bound to one render graph.
     * @param graph Owning render graph used for resource lookup.
     */
    ResourceAccessor(RenderGraph* graph) : m_graph(graph) {}

    /**
     * @brief Get a framebuffer resource by logical name.
     * @param name Logical resource name registered in the graph.
     * @return Pointer to the framebuffer, or nullptr when missing.
     */
    Framebuffer* GetFramebuffer(const std::string& name);

    /**
     * @brief Get the first color attachment texture id for a named framebuffer.
     * @param name Logical resource name registered in the graph.
     * @return OpenGL texture id, or 0 when the resource is unavailable.
     */
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
    /**
     * @brief Descriptor for one graph texture resource.
     */
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

    /**
     * @brief Create and register a texture resource in the graph.
     * @param name Unique logical resource name.
     * @param desc Resource descriptor including dimensions, format, and target type.
     * @return Stable handle for the created resource.
     */
    RGHandle CreateTexture(const std::string& name, const TextureDesc& desc);

    /**
     * @brief Find a framebuffer by logical resource name.
     * @param name Resource name registered via CreateTexture.
     * @return Pointer to framebuffer, or nullptr if not found.
     */
    Framebuffer* GetFramebuffer(const std::string& name);

    /**
     * @brief Add one render pass and its declared resource dependencies.
     * @param name Human-readable pass name.
     * @param readNames Resource names sampled by the pass.
     * @param writeNames Resource names written by the pass.
     * @param execute Callback invoked during Execute for this pass.
     * @return void
     */
    void AddPass(const std::string& name,
        const std::vector<std::string>& readNames,
        const std::vector<std::string>& writeNames,
        std::function<void(ResourceAccessor&)> execute);

    /**
     * @brief Execute all registered passes in submission order.
     */
    void Execute();

    /**
     * @brief Clear the pass list while keeping allocated resources alive.
     */
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
