#include "graphics/RenderGraph.hpp"

// ----------------------------------------------------------------------------
// RenderGraph Implementation
// ----------------------------------------------------------------------------

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
        res.fbo->Create(desc.width, desc.height, floatingPoint, true);
    }

    m_nameToHandle[name] = handle;
    return handle;
}

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

void RenderGraph::Execute()
{
    ResourceAccessor accessor(this);

    for (auto& pass : m_passes) {
        pass.execute(accessor);
    }
}

void RenderGraph::Reset()
{
    m_passes.clear();
    // Resources persist between frames
}

// ----------------------------------------------------------------------------
// ResourceAccessor Implementation
// ----------------------------------------------------------------------------

Framebuffer* ResourceAccessor::GetFramebuffer(const std::string& name)
{
    return m_graph->GetFramebuffer(name);
}

GLuint ResourceAccessor::GetTexture(const std::string& name)
{
    Framebuffer* fbo = m_graph->GetFramebuffer(name);
    return fbo ? fbo->GetColorTexture(0) : 0;
}