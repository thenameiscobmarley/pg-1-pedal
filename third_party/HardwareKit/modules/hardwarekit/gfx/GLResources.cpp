
using namespace juce::gl;

namespace hwk::gfx
{
    //==============================================================================
    void MeshData::append (const MeshData& other, const Mat4& transform)
    {
        const auto base = (juce::uint32) vertices.size();

        for (auto v : other.vertices)
        {
            const auto p = transform.transformPoint ({ v.px, v.py, v.pz });
            const auto n = normalise (transform.transformDir ({ v.nx, v.ny, v.nz }));
            vertices.push_back ({ p.x, p.y, p.z, n.x, n.y, n.z, v.u, v.v });
        }

        for (auto i : other.indices)
            indices.push_back (base + i);
    }

    //==============================================================================
    void GpuMesh::upload (const MeshData& data)
    {
        release();

        if (data.isEmpty())
            return;

        glGenVertexArrays (1, &vao);
        glBindVertexArray (vao);

        glGenBuffers (1, &vbo);
        glBindBuffer (GL_ARRAY_BUFFER, vbo);
        glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (data.vertices.size() * sizeof (Vertex)), data.vertices.data(), GL_STATIC_DRAW);

        glGenBuffers (1, &ibo);
        glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData (GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) (data.indices.size() * sizeof (juce::uint32)), data.indices.data(), GL_STATIC_DRAW);

        const auto stride = (GLsizei) sizeof (Vertex);
        glEnableVertexAttribArray (0);
        glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, stride, (void*) offsetof (Vertex, px));
        glEnableVertexAttribArray (1);
        glVertexAttribPointer (1, 3, GL_FLOAT, GL_FALSE, stride, (void*) offsetof (Vertex, nx));
        glEnableVertexAttribArray (2);
        glVertexAttribPointer (2, 2, GL_FLOAT, GL_FALSE, stride, (void*) offsetof (Vertex, u));

        glBindVertexArray (0);
        count = (GLsizei) data.indices.size();
    }

    void GpuMesh::draw() const
    {
        if (! isValid())
            return;

        glBindVertexArray (vao);
        glDrawElements (GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
    }

    void GpuMesh::release()
    {
        if (ibo != 0) glDeleteBuffers (1, &ibo);
        if (vbo != 0) glDeleteBuffers (1, &vbo);
        if (vao != 0) glDeleteVertexArrays (1, &vao);
        vao = vbo = ibo = 0;
        count = 0;
    }

    //==============================================================================
    static GLuint compileStage (GLenum type, const char* source, juce::String& error)
    {
        const GLuint shader = glCreateShader (type);
        glShaderSource (shader, 1, &source, nullptr);
        glCompileShader (shader);

        GLint ok = 0;
        glGetShaderiv (shader, GL_COMPILE_STATUS, &ok);

        if (ok == 0)
        {
            char log[2048] = {};
            glGetShaderInfoLog (shader, sizeof (log) - 1, nullptr, log);
            error << (type == GL_VERTEX_SHADER ? "vertex: " : "fragment: ") << log;
            glDeleteShader (shader);
            return 0;
        }

        return shader;
    }

    bool ShaderProgram::build (const char* vs, const char* fs, juce::String& error)
    {
        release();

        const auto v = compileStage (GL_VERTEX_SHADER, vs, error);
        const auto f = compileStage (GL_FRAGMENT_SHADER, fs, error);

        if (v == 0 || f == 0)
        {
            if (v != 0) glDeleteShader (v);
            if (f != 0) glDeleteShader (f);
            return false;
        }

        program = glCreateProgram();
        glAttachShader (program, v);
        glAttachShader (program, f);
        glBindAttribLocation (program, 0, "aPos");
        glBindAttribLocation (program, 1, "aNormal");
        glBindAttribLocation (program, 2, "aUV");
        glBindFragDataLocation (program, 0, "fragColor");
        glLinkProgram (program);
        glDeleteShader (v);
        glDeleteShader (f);

        GLint ok = 0;
        glGetProgramiv (program, GL_LINK_STATUS, &ok);

        if (ok == 0)
        {
            char log[2048] = {};
            glGetProgramInfoLog (program, sizeof (log) - 1, nullptr, log);
            error << "link: " << log;
            release();
            return false;
        }

        return true;
    }

    void ShaderProgram::use() const { glUseProgram (program); }

    void ShaderProgram::release()
    {
        if (program != 0)
            glDeleteProgram (program);

        program = 0;
        uniformCache.clear();
    }

    GLint ShaderProgram::uniform (const char* name)
    {
        auto it = uniformCache.find (name);
        if (it != uniformCache.end())
            return it->second;

        const auto loc = glGetUniformLocation (program, name);
        uniformCache.emplace (name, loc);
        return loc;
    }

    void ShaderProgram::set (const char* n, float v)            { glUniform1f (uniform (n), v); }
    void ShaderProgram::set (const char* n, int v)              { glUniform1i (uniform (n), v); }
    void ShaderProgram::set (const char* n, Vec3 v)             { glUniform3f (uniform (n), v.x, v.y, v.z); }
    void ShaderProgram::set (const char* n, float a, float b)   { glUniform2f (uniform (n), a, b); }
    void ShaderProgram::set (const char* n, float a, float b, float c, float d) { glUniform4f (uniform (n), a, b, c, d); }
    void ShaderProgram::set (const char* n, const Mat4& m)      { glUniformMatrix4fv (uniform (n), 1, GL_FALSE, m.m.data()); }
    void ShaderProgram::setArray (const char* n, const float* v, int count) { glUniform1fv (uniform (n), count, v); }
    void ShaderProgram::setArray3 (const char* n, const float* v, int count) { glUniform3fv (uniform (n), count, v); }
    void ShaderProgram::setArray4 (const char* n, const float* v, int count) { glUniform4fv (uniform (n), count, v); }

    //==============================================================================
    void Texture2D::updateRegion (const juce::uint8* data, int x, int y, int width, int height)
    {
        if (id == 0 || width <= 0 || height <= 0 || x < 0 || y < 0 || x + width > w || y + height > h)
            return;
        glBindTexture (GL_TEXTURE_2D, id);
        glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D (GL_TEXTURE_2D, 0, x, y, width, height, ch == 1 ? (GLenum) GL_RED : (GLenum) GL_RGBA, GL_UNSIGNED_BYTE, data);
        if (mips)
            glGenerateMipmap (GL_TEXTURE_2D);
    }

    void Texture2D::upload (const juce::uint8* data, int width, int height, int channels, bool mipmaps, int anisotropy)
    {
        const bool sameShape = (id != 0 && width == w && height == h && channels == ch);

        if (! sameShape)
        {
            release();
            glGenTextures (1, &id);
        }

        w = width; h = height; ch = channels; mips = mipmaps;

        glBindTexture (GL_TEXTURE_2D, id);
        glPixelStorei (GL_UNPACK_ALIGNMENT, 1);

        const auto internalFormat = channels == 1 ? (GLenum) GL_R8 : (GLenum) GL_RGBA8;
        const auto format         = channels == 1 ? (GLenum) GL_RED : (GLenum) GL_RGBA;

        if (sameShape)
            glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE, data);
        else
            glTexImage2D (GL_TEXTURE_2D, 0, (GLint) internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);

        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (mipmaps)
        {
            glGenerateMipmap (GL_TEXTURE_2D);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

            if (anisotropy > 1)
            {
                constexpr GLenum maxAnisotropyExt = 0x84FE; // GL_TEXTURE_MAX_ANISOTROPY(_EXT), core since 4.6
                glTexParameterf (GL_TEXTURE_2D, maxAnisotropyExt, (GLfloat) anisotropy);
                glGetError(); // harmless if unsupported
            }
        }
        else
        {
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }
    }

    void Texture2D::bind (int unit) const
    {
        glActiveTexture ((GLenum) ((int) GL_TEXTURE0 + unit));
        glBindTexture (GL_TEXTURE_2D, id);
    }

    void Texture2D::release()
    {
        if (id != 0)
            glDeleteTextures (1, &id);
        id = 0;
    }

    //==============================================================================
    bool RenderTarget::ensureSize (int width, int height, int samples)
    {
        width = std::max (1, width);
        height = std::max (1, height);
        if (samples > 1)
        {
            GLint maxSamples = 1;
            glGetIntegerv (GL_MAX_SAMPLES, &maxSamples);
            samples = std::min (samples, (int) maxSamples);
        }
        samples = samples > 1 ? samples : 0;
        if (fbo != 0 && width == w && height == h && samples == sampleCount)
            return complete;

        release();
        w = width;
        h = height;
        sampleCount = samples;

        glGenTextures (1, &colour);
        glBindTexture (GL_TEXTURE_2D, colour);
        glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenRenderbuffers (1, &depth);
        glBindRenderbuffer (GL_RENDERBUFFER, depth);
        glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);

        glGenFramebuffers (1, &fbo);
        glBindFramebuffer (GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colour, 0);
        glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
        complete = glCheckFramebufferStatus (GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

        if (complete && sampleCount > 1)
        {
            glGenRenderbuffers (1, &msColour);
            glBindRenderbuffer (GL_RENDERBUFFER, msColour);
            glRenderbufferStorageMultisample (GL_RENDERBUFFER, sampleCount, GL_RGBA8, w, h);
            glGenRenderbuffers (1, &msDepth);
            glBindRenderbuffer (GL_RENDERBUFFER, msDepth);
            glRenderbufferStorageMultisample (GL_RENDERBUFFER, sampleCount, GL_DEPTH_COMPONENT24, w, h);

            glGenFramebuffers (1, &msFbo);
            glBindFramebuffer (GL_FRAMEBUFFER, msFbo);
            glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColour);
            glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth);

            // No multisampling to be had: fall back to the plain target rather than fail
            if (glCheckFramebufferStatus (GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                glDeleteFramebuffers (1, &msFbo);
                glDeleteRenderbuffers (1, &msColour);
                glDeleteRenderbuffers (1, &msDepth);
                msFbo = msColour = msDepth = 0;
                sampleCount = 0;
            }
        }

        unbind();
        return complete;
    }

    void RenderTarget::resolve() const
    {
        if (msFbo == 0)
            return;

        glBindFramebuffer (GL_READ_FRAMEBUFFER, msFbo);
        glBindFramebuffer (GL_DRAW_FRAMEBUFFER, fbo);
        glBlitFramebuffer (0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        unbind();
    }

    void RenderTarget::bind() const
    {
        glBindFramebuffer (GL_FRAMEBUFFER, msFbo != 0 ? msFbo : fbo);
        glViewport (0, 0, w, h);
    }

    void RenderTarget::unbind()
    {
        // JUCE may render through its own framebuffer; hand control back to it
        if (auto* ctx = juce::OpenGLContext::getCurrentContext())
            ctx->getFrameBufferID() != 0 ? glBindFramebuffer (GL_FRAMEBUFFER, ctx->getFrameBufferID())
                                         : glBindFramebuffer (GL_FRAMEBUFFER, 0);
        else
            glBindFramebuffer (GL_FRAMEBUFFER, 0);
    }

    void RenderTarget::bindColour (int unit) const
    {
        glActiveTexture ((GLenum) ((int) GL_TEXTURE0 + unit));
        glBindTexture (GL_TEXTURE_2D, colour);
    }

    void RenderTarget::release()
    {
        if (fbo != 0)    glDeleteFramebuffers (1, &fbo);
        if (depth != 0)  glDeleteRenderbuffers (1, &depth);
        if (colour != 0) glDeleteTextures (1, &colour);
        if (msFbo != 0)    glDeleteFramebuffers (1, &msFbo);
        if (msColour != 0) glDeleteRenderbuffers (1, &msColour);
        if (msDepth != 0)  glDeleteRenderbuffers (1, &msDepth);
        fbo = depth = colour = msFbo = msColour = msDepth = 0;
        sampleCount = 0;
        complete = false;
    }
}
