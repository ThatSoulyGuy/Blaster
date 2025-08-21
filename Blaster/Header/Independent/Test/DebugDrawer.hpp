#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Independent/Math/Matrix.hpp"
#include "Independent/Utility/BulletSterilized.hpp"

namespace Blaster::Independent::Test
{
    class DebugDrawer final : public btIDebugDraw
    {

    public:

        DebugDrawer()
        {
            m_debugMode = DBG_DrawWireframe | DBG_DrawAabb;

            glGenVertexArrays(1, &vao);
            glGenBuffers(1, &vbo);

            shader = buildShader();
        }

        void drawLine(const btVector3& from, const btVector3& to, const btVector3& colour) override
        {
            lines.emplace_back(Line{ from, to, colour });
        }

        void drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int, const btVector3& colour) override
        {
            constexpr btScalar kContactSize = 0.05f;

            btVector3 p1 = PointOnB + btVector3(kContactSize, 0, 0);
            btVector3 p2 = PointOnB - btVector3(kContactSize, 0, 0);

            drawLine(p1, p2, colour);

            p1 = PointOnB + btVector3(0, kContactSize, 0);
            p2 = PointOnB - btVector3(0, kContactSize, 0);

            drawLine(p1, p2, colour);

            p1 = PointOnB + btVector3(0, 0, kContactSize);
            p2 = PointOnB - btVector3(0, 0, kContactSize);

            drawLine(p1, p2, colour);

            btScalar len = (distance > btScalar(0.0)) ? distance : btScalar(0.2);
            drawLine(PointOnB, PointOnB + normalOnB.normalized() * len, colour);
        }

        void reportErrorWarning(const char* warningString) override
        {
            std::cerr << "[Bullet] " << warningString << '\n';
        }

        void draw3dText(const btVector3&, const char*) override
        {
            
        }

        void setDebugMode(int debugMode) override
        {
            m_debugMode = debugMode;
        }

        int getDebugMode() const override
        {
            return m_debugMode;
        }

        void flush(const Blaster::Independent::Math::Matrix<float, 4, 4>& mvpMatrix)
        {
            if (lines.empty()) return;

            /* --- assemble CPU buffer ------------------------------------------------ */
            std::vector<float> cpu;
            cpu.reserve(lines.size() * 12);              // 2 vertices � (3+3)

            for (const auto& l : lines)
            {
                push(cpu, l.a); push(cpu, l.col);
                push(cpu, l.b); push(cpu, l.col);
            }

            /* --- (re)allocate VBO if necessary -------------------------------------- */
            const size_t bytes = cpu.size() * sizeof(float);
            if (bytes > vboCapacity)
            {
                vboCapacity = std::max(bytes, vboCapacity * 2);   // exponential growth
                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glBufferData(GL_ARRAY_BUFFER, vboCapacity, nullptr, GL_DYNAMIC_DRAW);
            }

            /* --- upload & render ---------------------------------------------------- */
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, cpu.data());

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

            glUseProgram(shader);
            glUniformMatrix4fv(glGetUniformLocation(shader, "uMVP"), 1, GL_FALSE, &mvpMatrix[0][0]);

            glEnable(GL_DEPTH_TEST);                  // keep the world readable
            glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lines.size() * 2));
            glDisable(GL_DEPTH_TEST);

            glBindVertexArray(0);
            glUseProgram(0);

            lines.clear();
        }

    private:

        struct Line
        {
            btVector3 a, b, col;
        };

        static void push(std::vector<float>& v, const btVector3& p)
        {
            v.emplace_back(p.x());
            v.emplace_back(p.y());
            v.emplace_back(p.z());
        }

        GLuint buildShader()
        {
            auto compile = [](GLenum type, const char* src)
                {
                    GLuint s = glCreateShader(type);
                    glShaderSource(s, 1, &src, nullptr);
                    glCompileShader(s);
                    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
                    if (!ok) {
                        char log[512]; glGetShaderInfoLog(s, 512, nullptr, log);
                        throw std::runtime_error(log);
                    }
                    return s;
                };
            GLuint vs = compile(GL_VERTEX_SHADER, kVs);
            GLuint fs = compile(GL_FRAGMENT_SHADER, kFs);
            GLuint prog = glCreateProgram();
            glAttachShader(prog, vs); glAttachShader(prog, fs);
            glLinkProgram(prog);
            GLint ok; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
            if (!ok) {
                char log[512]; glGetProgramInfoLog(prog, 512, nullptr, log);
                throw std::runtime_error(log);
            }
            glDeleteShader(vs); glDeleteShader(fs);
            return prog;
        }

        std::vector<Line> lines;

        GLuint vao{ 0 }, vbo{ 0 };
        GLuint shader{ 0 };
        size_t vboCapacity = 0;

        int m_debugMode;

        inline static const char* kVs = R"(#version 330 core
layout(location=0) in vec3 inPos;
layout(location=1) in vec3 inCol;
uniform mat4 uMVP;
out vec3 vCol;
void main() { gl_Position = uMVP*vec4(inPos,1.0); vCol = inCol; })";

        inline static const char* kFs = R"(#version 330 core
in  vec3 vCol;
out vec4 fragColor;
void main() { fragColor = vec4(vCol,1.0); })";
    };
}