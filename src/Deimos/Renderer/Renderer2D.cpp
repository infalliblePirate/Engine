#include "dmpch.h"
#include "Deimos/Core/Core.h"

#include "Renderer2D.h"
#include "RenderCommand.h"

#include "Shader.h"
#include "VertexArray.h"
#include "Platform/OpenGL/OpenGLShader.h"

#include <glm/glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm/gtx/compatibility.hpp>

namespace Deimos {

    struct QuadVertex {
        glm::vec3 position;
        glm::vec4 color;
        glm::vec2 texCoord;
        float texID;
    };

    struct Renderer2DData {
        const uint32_t maxQuads = 10'000;
        const uint32_t maxVertices = maxQuads * 4;
        const uint32_t maxIndices = maxQuads * 6;
        static const int maxSlots = 16; // TODO: derive from specs

        std::array<Ref<Texture>, maxSlots> textures;
        uint32_t index = 1; // 0 is reserved for white texture

        QuadVertex* quadVertexBufferBase = nullptr;
        QuadVertex* quadVertexBufferPtr = nullptr;

        uint32_t quadIndexCount = 0;

        Ref<VertexBuffer> quadVB;
        Ref<VertexArray> quadVertexArray;

        Ref<VertexArray> lineVertexArray;
        Ref<VertexArray> triangleVertexArray;
        Ref<VertexArray> circleVertexArray;
        Ref<VertexArray> ovalVertexArray;
        Ref<VertexArray> polygonVertexArray;
        Ref<VertexArray> bezierVertexArray;
        
        Ref<Shader> textureShader;
        Ref<Shader> plainColorShader;

        Ref<Texture2D> whiteTexture;

        glm::vec4 QuadVertexPositions[4];
    };

    static Renderer2DData s_data;

    void Renderer2D::init() {
        DM_PROFILE_FUNCTION();

        s_data.whiteTexture = Texture2D::create(1, 1);
        uint32_t whiteTextureData = 0xffffffff;
        s_data.whiteTexture->setData(&whiteTextureData, sizeof(uint32_t));
        s_data.textures[0] = s_data.whiteTexture;

        s_data.textureShader = Shader::create(std::string(ASSETS_DIR) + "/shaders/Texture.glsl");
        s_data.plainColorShader = Shader::create(std::string(ASSETS_DIR) + "/shaders/PlainColor.glsl");

        s_data.textureShader->bind();
        
        // Array of sample slots
        int samplers[s_data.maxSlots];
        for (uint32_t i = 0; i < s_data.maxSlots; ++i)
            samplers[i] = i;

        s_data.textureShader->setIntVec("u_textures", samplers, s_data.maxSlots);

        s_data.QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
        s_data.QuadVertexPositions[1] = { 0.5f, -0.5f, 0.0f, 1.0f };
        s_data.QuadVertexPositions[2] = {  0.5f,  0.5f, 0.0f, 1.0f };
		s_data.QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };


        // LINE
        {
            s_data.lineVertexArray = VertexArray::create();
            float lineVertices[2 * 3]{
                0.0f, 0.0f, 0.0f, 
                1.f, 0.0f, 0.0f,
            };

            Ref<VertexBuffer> lineVB;
            lineVB = VertexBuffer::create(lineVertices, sizeof(lineVertices));
            lineVB->setLayout(
                {
                    { ShaderDataType::Float3, "a_position" }
                }
            );

            s_data.lineVertexArray->addVertexBuffer(lineVB);

            unsigned int lineindices[2] = { 0, 1 };

            Ref<IndexBuffer> lineIB;
            lineIB = IndexBuffer::create(lineindices, sizeof(lineindices) / sizeof(unsigned int));

            s_data.lineVertexArray->setIndexBuffer(lineIB);
        }

        // QUAD
        {
            s_data.quadVertexBufferBase = new QuadVertex[s_data.maxVertices];
            s_data.quadVertexBufferPtr = s_data.quadVertexBufferBase;

            s_data.quadVertexArray = Deimos::VertexArray::create();

            s_data.quadVB = VertexBuffer::create(s_data.maxVertices * sizeof(QuadVertex));
            s_data.quadVB->setLayout(
                    {
                            { ShaderDataType::Float3, "a_position" },
                            { ShaderDataType::Float4, "a_color" },
                            { ShaderDataType::Float2, "a_texCoord" },
                            { ShaderDataType::Float,  "a_texID"}
                    });
            s_data.quadVertexArray->addVertexBuffer(s_data.quadVB);

            unsigned int* quadIndices = new unsigned int[s_data.maxIndices];

            uint32_t offset = 0;
            for (size_t i = 0; i < s_data.maxIndices; i += 6) {
                quadIndices[i + 0] = offset + 0;
                quadIndices[i + 1] = offset + 1;
                quadIndices[i + 2] = offset + 2;
                
                quadIndices[i + 3] = offset + 2;
                quadIndices[i + 4] = offset + 3;
                quadIndices[i + 5] = offset + 0;

                offset += 4;
            }

            Ref<IndexBuffer> quadIB = IndexBuffer::create(quadIndices, s_data.maxIndices);
            s_data.quadVertexArray->setIndexBuffer(quadIB);
            delete[] quadIndices;
        }

        // TRIANGLE:
        {
                s_data.triangleVertexArray = VertexArray::create();
            float triangleVertices[3 * 3]{
                -0.5f, -0.5f, 0.0f, 
                0.5f, -0.5f, 0.0f,
                0.0f, 0.5f, 0.0f
            };

            Ref<VertexBuffer> triangleVB;
            triangleVB = VertexBuffer::create(triangleVertices, sizeof(triangleVertices));
            triangleVB->setLayout(
                {
                    { ShaderDataType::Float3, "a_position" }
                }
            );

            s_data.triangleVertexArray->addVertexBuffer(triangleVB);

            unsigned int triangleindices[3] = { 0, 1, 2 };

            Ref<IndexBuffer> triangleIB;
            triangleIB = IndexBuffer::create(triangleindices, sizeof(triangleindices) / sizeof(unsigned int));

            s_data.triangleVertexArray->setIndexBuffer(triangleIB);
        }

        // CIRCLE
        // TODO change from predefined behaviour
        {
            int vCount = 32; // count of "angles" in a circle
            float angle = 360.f / vCount;
            int triangleCount = vCount - 2;

            std::vector<glm::vec3> temp;

            // positions
            for (size_t i = 0; i < vCount; ++i) {
                float x = cos(glm::radians(angle * i));
                float y = sin(glm::radians(angle * i));
                float z = 0.f;

                temp.push_back(glm::vec3(x, y, z));
            }

            s_data.circleVertexArray = VertexArray::create();

            std::vector<float> circleVertices;
            circleVertices.reserve(3 * vCount); // create circle vertices array
            for (size_t i = 0; i < vCount; ++i) {
                circleVertices.push_back(temp[i].x);
                circleVertices.push_back(temp[i].y);
                circleVertices.push_back(temp[i].z);
            }

            Ref<VertexBuffer> circleVB;
            circleVB = VertexBuffer::create(circleVertices.data(), sizeof(float) * circleVertices.size());
            circleVB->setLayout(
                {
                    { ShaderDataType::Float3, "a_position"}
                }
            );

            s_data.circleVertexArray->addVertexBuffer(circleVB);

        std::vector<unsigned int> circleIndices;
        circleIndices.resize(3 * triangleCount);
            for (size_t i = 0; i < triangleCount; ++i) {
                circleIndices.push_back(0); // origin
                circleIndices.push_back(i + 1);
                circleIndices.push_back(i + 2);
            }

            Ref<IndexBuffer> circleIB;
            circleIB = IndexBuffer::create(circleIndices.data(), sizeof(unsigned int) * circleIndices.size() / sizeof(unsigned int));

            s_data.circleVertexArray->setIndexBuffer(circleIB);
        }
    }

    void Renderer2D::shutdown() {
        DM_PROFILE_FUNCTION();
    }

    void Renderer2D::beginScene(const OrthographicCamera &camera) {
        DM_PROFILE_FUNCTION();

        s_data.quadIndexCount = 0;
        s_data.quadVertexBufferPtr = s_data.quadVertexBufferBase;

        s_data.textureShader->bind();
        s_data.textureShader->setMat4("u_viewProjection", camera.getViewProjectionMatrix());

        s_data.plainColorShader->bind();
        s_data.plainColorShader->setMat4("u_viewProjection", camera.getViewProjectionMatrix());
    }

    void Renderer2D::endScene() {
        DM_PROFILE_FUNCTION();

        uint32_t size = (uint8_t*)s_data.quadVertexBufferPtr - (uint8_t*)s_data.quadVertexBufferBase;
        s_data.quadVB->setData(s_data.quadVertexBufferBase, size);
        
        s_data.quadVertexArray->bind();
         // Bind textures to some slots
        for (uint32_t i = 0; i < s_data.index; ++i) {
            s_data.textures[i]->bind(i);
        }
        
        RenderCommand::drawIndexed(s_data.quadVertexArray, s_data.quadIndexCount);
    }

    void Renderer2D::drawLine(const glm::vec2 &start, const glm::vec2 &end, float thickness, const glm::vec4 &color, float tilingFactor, const glm::vec4 &tintColor) {
        drawLine({ start.x, start.y, 0.f }, { end.x, end.y, 0.f }, thickness, color, tilingFactor, tintColor );
    }

    void Renderer2D::drawLine(const glm::vec3 &start, const glm::vec3 &end, float thickness, const glm::vec4 &color, float tilingFactor, const glm::vec4 &tintColor) {
        DM_PROFILE_FUNCTION();

        s_data.plainColorShader->bind();
        glm::vec3 direction = end - start; // direction vector
        float length = glm::length(direction);

        // calculate the angle of rotation in radians
        float angle = glm::atan(direction.y, direction.x);

        // create transformation matrix
        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, start); // move origin
        transform = glm::rotate(transform, angle, glm::vec3(0, 0, 1));
        transform = glm::scale(transform, glm::vec3(length, 1.f, 1.0f)); // strech along the x axis


        s_data.plainColorShader->setMat4("u_transform", transform);
        s_data.plainColorShader->setFloat4("u_color", color * tintColor);

        s_data.lineVertexArray->bind();
        RenderCommand::drawLine(s_data.lineVertexArray, thickness);
    }

    void Renderer2D::drawQuad(const glm::vec2 &position, const glm::vec2 &size, const glm::vec4 &color, float tilingFactor, const glm::vec4& tintColor) {
        drawQuad({ position.x, position.y, 0.f }, size, color, tilingFactor, tintColor);
    }

    void Renderer2D::drawQuad(const glm::vec3 &position, const glm::vec2 &size, const glm::vec4 &color, float tilingFactor, const glm::vec4& tintColor) {
        DM_PROFILE_FUNCTION();

        const float textureIdex = 0.f; // white texture

        glm::mat4 transfrom = glm::translate(glm::mat4(1.f), position) * glm::scale(glm::mat4(1.f), { size.x, size.y, 1.f });

        s_data.textureShader->bind();

        s_data.quadVertexBufferPtr->position = transfrom * s_data.QuadVertexPositions[0];
        s_data.quadVertexBufferPtr->color = color;
        s_data.quadVertexBufferPtr->texCoord = { 0, 0 };
        s_data.quadVertexBufferPtr->texID = textureIdex;
        s_data.quadVertexBufferPtr++;

        s_data.quadVertexBufferPtr->position = transfrom * s_data.QuadVertexPositions[1];
        s_data.quadVertexBufferPtr->color = color;
        s_data.quadVertexBufferPtr->texCoord = { 1, 0 };
        s_data.quadVertexBufferPtr->texID = textureIdex;
        s_data.quadVertexBufferPtr++;

        s_data.quadVertexBufferPtr->position = transfrom * s_data.QuadVertexPositions[2];
        s_data.quadVertexBufferPtr->color = color;
        s_data.quadVertexBufferPtr->texCoord = { 1, 1 };
        s_data.quadVertexBufferPtr->texID = textureIdex;
        s_data.quadVertexBufferPtr++;

        s_data.quadVertexBufferPtr->position = transfrom * s_data.QuadVertexPositions[3];
        s_data.quadVertexBufferPtr->color = color;
        s_data.quadVertexBufferPtr->texCoord = { 0, 1 };
        s_data.quadVertexBufferPtr->texID = textureIdex;
        s_data.quadVertexBufferPtr++;

        s_data.quadIndexCount += 6;
    }

    /**@param rotation The rotation of the quad in radians*/
    void Renderer2D::drawRotatedQuad(const glm::vec2 &position, const glm::vec2 &size, const glm::vec4 &color, float rotation, float tilingFactor, const glm::vec4& tintColor) {
        drawRotatedQuad({ position.x, position.y, 0 }, size, color, rotation, tilingFactor, tintColor);
    }

    /**@param rotation The rotation of the quad in radians*/
    void Renderer2D::drawRotatedQuad(const glm::vec3 &position, const glm::vec2 &size, const glm::vec4 &color, float rotation, float tilingFactor, const glm::vec4& tintColor) {
        DM_PROFILE_FUNCTION()

        const float textureIdex = 0.f; // white texture

        glm::mat4 transfrom = glm::translate(glm::mat4(1.f), position)
                              * glm::rotate(glm::mat4(1.f), glm::radians(rotation), { 0.f, 0.f, 1.f })
                              * glm::scale(glm::mat4(1.f), { size.x, size.y, 1.f });

        s_data.textureShader->bind();

        s_data.quadVertexBufferPtr->position = transfrom * s_data.QuadVertexPositions[0];
        s_data.quadVertexBufferPtr->color = color;
        s_data.quadVertexBufferPtr->texCoord = { 0, 0 };
        s_data.quadVertexBufferPtr->texID = textureIdex;
        s_data.quadVertexBufferPtr++;

        s_data.quadVertexBufferPtr->position = transfrom * s_data.QuadVertexPositions[1];
        s_data.quadVertexBufferPtr->color = color;
        s_data.quadVertexBufferPtr->texCoord = { 1, 0 };
        s_data.quadVertexBufferPtr->texID = textureIdex;
        s_data.quadVertexBufferPtr++;

        s_data.quadVertexBufferPtr->position = transfrom * s_data.QuadVertexPositions[2];
        s_data.quadVertexBufferPtr->color = color;
        s_data.quadVertexBufferPtr->texCoord = { 1, 1 };
        s_data.quadVertexBufferPtr->texID = textureIdex;
        s_data.quadVertexBufferPtr++;

        s_data.quadVertexBufferPtr->position = transfrom * s_data.QuadVertexPositions[3];
        s_data.quadVertexBufferPtr->color = color;
        s_data.quadVertexBufferPtr->texCoord = { 0, 1 };
        s_data.quadVertexBufferPtr->texID = textureIdex;
        s_data.quadVertexBufferPtr++;

        s_data.quadIndexCount += 6;
    }

    void Renderer2D::drawQuad(const glm::vec2 &position, const glm::vec2 &size, const Ref<Texture> &texture, float tilingFactor, const glm::vec4& tintColor) {
        drawQuad({ position.x, position.y, 0.f }, size, texture, tilingFactor, tintColor);
    }

    void Renderer2D::drawQuad(const glm::vec3 &position, const glm::vec2 &size, const Ref<Texture> &texture, float tilingFactor, const glm::vec4& tintColor) {
        DM_PROFILE_FUNCTION();

        glm::mat4 transfrom = glm::translate(glm::mat4(1.f), position) * glm::scale(glm::mat4(1.f), { size.x, size.y, 1.f });

        uint32_t index = 0;
        for (uint32_t i = 1; i < s_data.index; ++i) {
            if (*s_data.textures[i].get() == *texture.get()) {
                index = i;
                break;
            }
        }
        if (!index) {
            s_data.textures[s_data.index] = texture;
            index = s_data.index;
            s_data.index++;
        }

        s_data.textureShader->bind();

        s_data.quadVertexBufferPtr->position = transfrom * s_data.QuadVertexPositions[0];
        s_data.quadVertexBufferPtr->color = tintColor;
        s_data.quadVertexBufferPtr->texCoord = { 0, 0 };
        s_data.quadVertexBufferPtr->texID = index;
        s_data.quadVertexBufferPtr++;

        s_data.quadVertexBufferPtr->position = transfrom * s_data.QuadVertexPositions[1];
        s_data.quadVertexBufferPtr->color = tintColor;
        s_data.quadVertexBufferPtr->texCoord = { 1, 0 };
        s_data.quadVertexBufferPtr->texID = index;
        s_data.quadVertexBufferPtr++;

        s_data.quadVertexBufferPtr->position = transfrom * s_data.QuadVertexPositions[2];
        s_data.quadVertexBufferPtr->color = tintColor;
        s_data.quadVertexBufferPtr->texCoord = { 1, 1 };
        s_data.quadVertexBufferPtr->texID = index;
        s_data.quadVertexBufferPtr++;

        s_data.quadVertexBufferPtr->position = transfrom * s_data.QuadVertexPositions[3];
        s_data.quadVertexBufferPtr->color = tintColor;
        s_data.quadVertexBufferPtr->texCoord = { 0, 1 };
        s_data.quadVertexBufferPtr->texID = index;
        s_data.quadVertexBufferPtr++;

        s_data.quadIndexCount += 6;
    }

    /**@param rotation The rotation of the quad in degrees*/
    void Renderer2D::drawRotatedQuad(const glm::vec2 &position, const glm::vec2 &size, const Ref<Texture> &texture, float rotation, float tilingFactor, const glm::vec4& tintColor) {
        drawRotatedQuad({ position.x, position.y, 0.f }, size, texture, rotation, tilingFactor, tintColor);
    }

    /**@param rotation The rotation of the quad in degrees*/
    void Renderer2D::drawRotatedQuad(const glm::vec3 &position, const glm::vec2 &size, const Ref<Texture> &texture, float rotation, float tilingFactor, const glm::vec4& tintColor) {
        DM_PROFILE_FUNCTION()

        glm::mat4 transfrom = glm::translate(glm::mat4(1.f), position)
                              * glm::rotate(glm::mat4(1.f), glm::radians(rotation), { 0.f, 0.f, 1.f })
                              * glm::scale(glm::mat4(1.f), { size.x, size.y, 1.f });

        uint32_t index = 0;
        for (uint32_t i = 1; i < s_data.index; ++i) {
            if (*s_data.textures[i].get() == *texture.get()) {
                index = i;
                break;
            }
        }
        if (!index) {
            s_data.textures[s_data.index] = texture;
            index = s_data.index;
            s_data.index++;
        }

        s_data.textureShader->bind();

        s_data.quadVertexBufferPtr->position = transfrom * s_data.QuadVertexPositions[0];
        s_data.quadVertexBufferPtr->color = tintColor;
        s_data.quadVertexBufferPtr->texCoord = { 0, 0 };
        s_data.quadVertexBufferPtr->texID = index;
        s_data.quadVertexBufferPtr++;

        s_data.quadVertexBufferPtr->position = transfrom * s_data.QuadVertexPositions[1];
        s_data.quadVertexBufferPtr->color = tintColor;
        s_data.quadVertexBufferPtr->texCoord = { 1, 0 };
        s_data.quadVertexBufferPtr->texID = index;
        s_data.quadVertexBufferPtr++;

        s_data.quadVertexBufferPtr->position = transfrom * s_data.QuadVertexPositions[2];
        s_data.quadVertexBufferPtr->color = tintColor;
        s_data.quadVertexBufferPtr->texCoord = { 1, 1 };
        s_data.quadVertexBufferPtr->texID = index;
        s_data.quadVertexBufferPtr++;

        s_data.quadVertexBufferPtr->position = transfrom * s_data.QuadVertexPositions[3];
        s_data.quadVertexBufferPtr->color = tintColor;
        s_data.quadVertexBufferPtr->texCoord = { 0, 1 };
        s_data.quadVertexBufferPtr->texID = index;
        s_data.quadVertexBufferPtr++;

        s_data.quadIndexCount += 6;
       
    }

    void Renderer2D::drawTriangle(const glm::vec2 &position, const glm::vec2 &size, const glm::vec4 &color, float tilingFactor, const glm::vec4 &tintColor) {
        drawTriangle({ position.x, position.y, 0}, size, color, tilingFactor, tintColor);
    }

    void Renderer2D::drawTriangle(const glm::vec3 &position, const glm::vec2 &size, const glm::vec4 &color, float tilingFactor, const glm::vec4 &tintColor) {
        DM_PROFILE_FUNCTION();

        s_data.plainColorShader->bind();

        glm::mat4 transform = glm::translate(glm::mat4(1.f), position) * glm::scale(glm::mat4(1.f), { size.x, size.y, 1.f} );
        s_data.plainColorShader->setMat4("u_transform", transform);
        s_data.plainColorShader->setFloat4("u_color", color * tintColor);

        s_data.triangleVertexArray->bind();
        RenderCommand::drawIndexed(s_data.triangleVertexArray);
    }

    /**@param rotation The rotation of the triangle in radians*/
    void Renderer2D::drawRotatedTriangle(const glm::vec2 &position, const glm::vec2 &size, const glm::vec4 &color, float rotation, float tilingFactor, const glm::vec4 &tintColor) {
        drawRotatedTriangle({ position.x, position.y, 0}, size, color, rotation, tilingFactor, tintColor);
    }

    /**@param rotation The rotation of the triangle in radians*/
    void Renderer2D::drawRotatedTriangle(const glm::vec3 &position, const glm::vec2 &size, const glm::vec4 &color, float rotation, float tilingFactor, const glm::vec4 &tintColor) {
        DM_PROFILE_FUNCTION();

        s_data.plainColorShader->bind();

        glm::mat4 transform = glm::translate(glm::mat4(1.f), position) * glm::rotate(glm::mat4(1.f), rotation, { 0, 0, 1}) * glm::scale(glm::mat4(1.f), { size.x, size.y, 1.f} );
        s_data.plainColorShader->setMat4("u_transform", transform);
        s_data.plainColorShader->setFloat4("u_color", color * tintColor);

        s_data.triangleVertexArray->bind();
        RenderCommand::drawIndexed(s_data.triangleVertexArray);
    }
    
    void Renderer2D::drawCircle(const glm::vec2 &position, float radius, int vCount, const glm::vec4 &color, float tilingFactor, const glm::vec4 &tintColor) {
        drawCircle({ position.x, position.y, 0.f }, radius, vCount, color, tilingFactor, tintColor);
    }

    void Renderer2D::drawCircle(const glm::vec3 &position, float radius, int vCount, const glm::vec4 &color, float tilingFactor, const glm::vec4 &tintColor) {
        DM_PROFILE_FUNCTION();

        s_data.plainColorShader->bind();

        glm::mat4 transform = glm::translate(glm::mat4(1.f), position) * glm::scale(glm::mat4(1.f), { radius, radius, 1.f} );
        s_data.plainColorShader->setMat4("u_transform", transform);
        s_data.plainColorShader->setFloat4("u_color", color * tintColor);

        s_data.circleVertexArray->bind();
        RenderCommand::drawIndexed(s_data.circleVertexArray);
    }

    /**@param rotation The rotation of the oval in radians*/
    void Renderer2D::drawOval(const glm::vec2 &center, float a, float b, float rotation, const glm::vec4 &color, float tilingFactor, const glm::vec4 &tintColor) {
        drawOval({ center.x, center.y, 0.f }, a, b, rotation, color, tilingFactor, tintColor);
    }

    /**@param rotation The rotation of the oval in radians*/
    void Renderer2D::drawOval(const glm::vec3 &center, float a, float b, float rotation, const glm::vec4 &color, float tilingFactor, const glm::vec4 &tintColor) {
        DM_PROFILE_FUNCTION();

        s_data.ovalVertexArray = VertexArray::create();

        float h = center.x;
        float k = center.y;

        int vCount = 32; // number of vertices
        float angleIncrement = 2.0f * glm::pi<float>() / vCount;

        std::vector<glm::vec3> temp(vCount);
        for (int i = 0; i < vCount; ++i) {
            float theta = i * angleIncrement;
            float x = a * cos(theta);
            float y = b * sin(theta);
            temp[i] = glm::vec3(x, y, 0.0f);
        }

        // apply transformations
        glm::mat4 transform = glm::translate(glm::mat4(1.f), center)
                            * glm::rotate(glm::mat4(1.f), rotation, glm::vec3(0, 0, 1))
                            * glm::scale(glm::mat4(1.f), glm::vec3(1.f, 1.f, 1.f));

        std::vector<float> ovalVertices;
        for (const auto& vertex : temp) {
            glm::vec4 transformedVertex = transform * glm::vec4(vertex, 1.0f);
            ovalVertices.push_back(transformedVertex.x);
            ovalVertices.push_back(transformedVertex.y);
            ovalVertices.push_back(transformedVertex.z);
        }

        Ref<VertexBuffer> ovalVB = VertexBuffer::create(ovalVertices.data(), ovalVertices.size() * sizeof(float));
        ovalVB->setLayout({ { ShaderDataType::Float3, "a_position" } });
        s_data.ovalVertexArray->addVertexBuffer(ovalVB);

        std::vector<unsigned int> ovalIndices;
        ovalIndices.resize(3 * (vCount - 2));
        for (size_t i = 0; i < vCount - 2; ++i) {
            ovalIndices.push_back(0); // origin point
            ovalIndices.push_back(i + 1);
            ovalIndices.push_back(i + 2);
        }

        Ref<IndexBuffer> ovalIB = IndexBuffer::create(ovalIndices.data(), sizeof(unsigned int) * ovalIndices.size() / sizeof(unsigned int));
        s_data.ovalVertexArray->setIndexBuffer(ovalIB);
        s_data.plainColorShader->bind();
        s_data.plainColorShader->setMat4("u_transform", glm::mat4(1.0f));
        s_data.plainColorShader->setFloat4("u_color", color * tintColor);

        s_data.ovalVertexArray->bind();
        RenderCommand::drawIndexed(s_data.ovalVertexArray);
    }


    void Renderer2D::drawPolygon(const glm::vec3 *vertices, int vCount, const glm::vec4 &color, float tilingFactor, const glm::vec4& tintColor) {
        DM_PROFILE_FUNCTION();

        std::vector<float> polygonVertices;
        polygonVertices.resize(vCount * 3);
        int triangleCount = vCount - 2; // the num of triangles that need to be drawn to make up the shape

        s_data.polygonVertexArray = Deimos::VertexArray::create();

        Ref<VertexBuffer> polygonVB; 
        for (size_t i = 0, j = 0; i < vCount; ++i) {
            polygonVertices[j++] = vertices[i].x;
            polygonVertices[j++] = vertices[i].y;
            polygonVertices[j++] = vertices[i].z;
        }

        polygonVB = VertexBuffer::create(polygonVertices.data(), sizeof(float) * polygonVertices.size());
        polygonVB->setLayout(
                {
                        { ShaderDataType::Float3, "a_position" },
                });
        s_data.polygonVertexArray->addVertexBuffer(polygonVB);

        std::vector<unsigned int> polygonIndices;
        polygonIndices.resize(3 * triangleCount);
        int left = 0;
        int right = vCount - 1;
        int index = 0;

        // filled by alternating between vertices from the left and right ends of the vertex list, progressing towards the middle
        // pattern: 0 1 17
        //         17 1 16
        //          1 2 16
        //         16 2 15
        //          2 3 15 
        //             ...
        //          10 8 9
        while (left < right) {
            polygonIndices.push_back(left);
            polygonIndices.push_back(left + 1);
            polygonIndices.push_back(right);

            if (left + 1 < right - 1) { // fails for odd number of triangles
                polygonIndices.push_back(right);
                polygonIndices.push_back(left + 1);
                polygonIndices.push_back(right - 1);
            }

            left++;
            right--;
        }

        Ref<IndexBuffer> polygonIB;
        polygonIB = IndexBuffer::create(polygonIndices.data(), sizeof(unsigned int) * polygonIndices.size() / sizeof(unsigned int));

        s_data.polygonVertexArray->setIndexBuffer(polygonIB);

        s_data.plainColorShader->bind();

        glm::mat4 transform = glm::mat3(1.f);
        s_data.plainColorShader->setMat4("u_transform", transform);
        s_data.plainColorShader->setFloat4("u_color", color * tintColor);

        s_data.polygonVertexArray->bind();
        RenderCommand::drawIndexed(s_data.polygonVertexArray);
    }

    void Renderer2D::drawBezier(const glm::vec3 &anchor1, const glm::vec3 &control, const glm::vec3 &anchor2, const glm::vec4 &color, float tilingFactor, const glm::vec4& tintColor) {
        DM_PROFILE_FUNCTION();

        s_data.bezierVertexArray = VertexArray::create();

        float delta = 0.05; // distance between two consequential points
        int numComposingPoints = 1.f / delta + 1;
        int triangleCount = numComposingPoints - 2;
        std::vector<float> bezierVertecies;
        bezierVertecies.resize(numComposingPoints * 3);
       
        Ref<VertexBuffer> bezierVB;
        for (size_t i = 0; i < numComposingPoints; ++i) {
            float t = i * delta;
            float x1 = glm::lerp(anchor1.x, control.x, t);
            float y1 = glm::lerp(anchor1.y, control.y, t);
            float x2 = glm::lerp(control.x, anchor2.x, t);
            float y2 = glm::lerp(control.y, anchor2.y, t);
            float x = glm::lerp(x1, x2, t);
            float y = glm::lerp(y1, y2, t);
            float z = anchor1.z;
            bezierVertecies[3 * i] = x;
            bezierVertecies[3 * i + 1] = y;
            bezierVertecies[3 * i + 2] = z;
        }

        bezierVB = VertexBuffer::create(bezierVertecies.data(), sizeof(float) * bezierVertecies.size());
        bezierVB->setLayout(
                {
                        { ShaderDataType::Float3, "a_position" },
                });
        s_data.bezierVertexArray->addVertexBuffer(bezierVB);

        std::vector<unsigned int> bezierIndices;
        bezierIndices.resize(3 * triangleCount);
        for (size_t i = 0; i < triangleCount; ++i) {
            bezierIndices.push_back(0); // origin
            bezierIndices.push_back(i + 1);
            bezierIndices.push_back(i + 2);
        }
        Ref<IndexBuffer> bezierIB;
        bezierIB = IndexBuffer::create(bezierIndices.data(), sizeof(unsigned int) * bezierIndices.size() / sizeof(unsigned int));
        s_data.bezierVertexArray->setIndexBuffer(bezierIB);

        s_data.plainColorShader->bind();

        glm::mat4 transform = glm::mat3(1.f);
        s_data.plainColorShader->setMat4("u_transform", transform);
        s_data.plainColorShader->setFloat4("u_color", color * tintColor);

        s_data.bezierVertexArray->bind();
        RenderCommand::drawIndexed(s_data.bezierVertexArray);
    }
}
