#ifndef ENGINE_RENDERERAPI_H
#define ENGINE_RENDERERAPI_H

#include <glm/glm/glm.hpp>
#include "VertexArray.h"

namespace Deimos {
    class RendererAPI {
    public:
        enum class API {
            None = 0, OpenGL = 1
        };
    public:
        virtual void setClearColor(const glm::vec4& color) = 0;
        virtual void clear() = 0;

        virtual void drawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount) = 0;
        virtual void drawLine(const Ref<VertexArray>& vertexArray, float thickness) = 0;

        virtual void init() = 0;
        virtual void setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;

        inline static API getAPI() { return s_API; }
    private:
        static API s_API;
    };

}


#endif //ENGINE_RENDERERAPI_H
