/* Start Header *****************************************************************/
/*!
\file   RenderGraph.cpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\date   31st October 2025
\brief
Implements the RenderGraph system, which manages the creation, scheduling,
and execution of framebuffer-based rendering passes in the engine’s graphics
pipeline.

Although passes currently execute in submission order, the RenderGraph
abstracts this process to model dependencies between passes and their
framebuffer resources. This makes the pipeline more flexible, allowing new
effects or pass order changes without modifying core renderer code.

This file provides:
- Creation and tracking of named framebuffer resources
- Registration of passes with explicit read/write dependencies
- Execution of all passes in the configured order
- Resource access via the ResourceAccessor helper

By decoupling pass logic from execution, the RenderGraph keeps the
RendererSystem focused on scene rendering while the graph manages pipeline flow.
*/
/* End Header *******************************************************************/

#include "graphics/RenderGraph.hpp"

// ============================================================================
// Creates a new texture resource and framebuffer (if not a backbuffer).
// Used to allocate off-screen targets like HDR or bloom buffers.
// ============================================================================
RGHandle RenderGraph::CreateTexture(const std::string& name, const TextureDesc& desc)
{
    RGHandle handle = m_nextHandle++;

    auto& res = m_resources[handle];
    res.name = name;
    res.desc = desc;

    // Create the actual framebuffer if not a backbuffer
    if (!desc.isBackbuffer) {
        res.fbo = std::make_unique<Framebuffer>();
        bool floatingPoint = (desc.format == GL_RGBA16F || desc.format == GL_RGB16F);
        res.fbo->Create(desc.width, desc.height, floatingPoint, true, desc.colorAttachmentCount);
    }

    m_nameToHandle[name] = handle;
    return handle;
}

// ============================================================================
// Retrieves a framebuffer pointer by its registered resource name.
// Returns nullptr if the resource does not exist.
// ============================================================================
Framebuffer* RenderGraph::GetFramebuffer(const std::string& name)
{
    auto it = m_nameToHandle.find(name);
    if (it == m_nameToHandle.end()) {
        std::cerr << "RenderGraph: Resource '" << name << "' not found!\n";
        return nullptr;
    }

    auto& res = m_resources[it->second];
    return res.fbo.get();
}

// ============================================================================
// Adds a render pass to the graph, specifying its read and write dependencies.
// Each pass defines a callback that performs its rendering logic.
// ============================================================================
void RenderGraph::AddPass(const std::string& name,
    const std::vector<std::string>& readNames,
    const std::vector<std::string>& writeNames,
    std::function<void(ResourceAccessor&)> execute)
{
    Pass pass;
    pass.name = name;
    pass.execute = execute;

    // Convert names to handles and validate
    for (const auto& rname : readNames) {
        auto it = m_nameToHandle.find(rname);
        if (it == m_nameToHandle.end()) {
            std::cerr << "ERROR: Pass '" << name
                << "' reads unknown resource '" << rname << "'\n";
            continue;
        }
        pass.reads.push_back(it->second);
    }

    for (const auto& wname : writeNames) {
        auto it = m_nameToHandle.find(wname);
        if (it == m_nameToHandle.end()) {
            std::cerr << "ERROR: Pass '" << name
                << "' writes unknown resource '" << wname << "'\n";
            continue;
        }
        pass.writes.push_back(it->second);
    }

    m_passes.push_back(pass);
}

// ============================================================================
// Executes all registered passes in submission order.
// Each pass is invoked with a ResourceAccessor for resource access.
// ============================================================================
void RenderGraph::Execute()
{
    ResourceAccessor accessor(this);

    for (auto& pass : m_passes) {
        pass.execute(accessor);
    }
}

// ============================================================================
// Clears all registered render passes but keeps allocated framebuffer resources.
// Called at the start of each frame to rebuild the pass list without re-creating
// GPU objects.
//
// By preserving existing FBOs and textures, the RenderGraph avoids expensive
// driver calls (glGenFramebuffer, glTexImage2D, etc.) that would otherwise stall
// the CPU–GPU pipeline. Framebuffers are reused across frames and only destroyed
// when the RenderGraph itself is destroyed or rebuilt (e.g., on window resize).
// This design drastically reduces per-frame CPU overhead and improves frame
// stability by eliminating unnecessary GPU memory reallocations.
// ============================================================================
void RenderGraph::Reset()
{
    m_passes.clear();
    // Resources persist between frames
}

// ============================================================================
// Returns a framebuffer by name for use within a render pass.
// ============================================================================
Framebuffer* ResourceAccessor::GetFramebuffer(const std::string& name)
{
    return m_graph->GetFramebuffer(name);
}

// ============================================================================
// Returns the texture ID of the framebuffer’s first color attachment.
// Used by shaders for sampling rendered results.
// ============================================================================
GLuint ResourceAccessor::GetTexture(const std::string& name)
{
    Framebuffer* fbo = m_graph->GetFramebuffer(name);
    return fbo ? fbo->GetColorTexture(0) : 0;
}
