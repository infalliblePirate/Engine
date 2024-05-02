#include "dmpch.h"
#include "VertexArray.h"

#include "Renderer.h"
#include "Platform/OpenGL/OpenGLVertexArray.h"

namespace Deimos {
    VertexArray *VertexArray::create() {
        switch (Renderer::getAPI()) {
            case RendererAPI::API::None: DM_ASSERT(false, "Deimos currently does not support RendererAPI::None!");
            case RendererAPI::API::OpenGL: return new OpenGLVertexArray();
        }
        DM_ASSERT(false, "Unknown RendererAPI!");
        return nullptr;
    }
}



