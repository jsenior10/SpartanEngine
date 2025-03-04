/*
Copyright(c) 2016-2024 Panos Karabelas

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

//= INCLUDES ==================================
#include "pch.h"
#include "Font.h"
#include "../Renderer.h"
#include "../../Core/Stopwatch.h"
#include "../../Resource/Import/FontImporter.h"
#include "../../RHI/RHI_Vertex.h"
#include "../../RHI/RHI_Buffer.h"
#include "../../RHI/RHI_CommandList.h"
//=============================================

//= NAMESPACES ===============
using namespace std;
using namespace Spartan::Math;
//============================

namespace Spartan
{
    namespace
    {
        constexpr uint8_t ASCII_TAB      = 9;
        constexpr uint8_t ASCII_NEW_LINE = 10;
        constexpr uint8_t ASCII_SPACE    = 32;
    }

    Font::Font(const string& file_path, const uint32_t font_size, const Color& color) : IResource(ResourceType::Font)
    {
        m_buffer_vertex   = make_shared<RHI_Buffer>();
        m_buffer_index    = make_shared<RHI_Buffer>();
        m_char_max_width  = 0;
        m_char_max_height = 0;
        m_color           = color;

        SetSize(font_size);
        LoadFromFile(file_path);
    }

    void Font::SaveToFile(const string& file_path)
    {

    }

    void Font::LoadFromFile(const string& file_path)
    {
        const Stopwatch timer;

        // load
        if (!FontImporter::LoadFromFile(this, file_path))
        {
            SP_LOG_ERROR("Failed to load font \"%s\"", file_path.c_str());
            return;
        }

        // find max character height (todo, actually get spacing from FreeType)
        for (const auto& char_info : m_glyphs)
        {
            m_char_max_width    = Helper::Max<int>(char_info.second.width, m_char_max_width);
            m_char_max_height   = Helper::Max<int>(char_info.second.height, m_char_max_height);
        }

        SP_LOG_INFO("Loading \"%s\" took %d ms", FileSystem::GetFileNameFromFilePath(file_path).c_str(), static_cast<int>(timer.GetElapsedTimeMs()));
    }

    void Font::AddText(const string& text, const Vector2& position_screen_percentage)
    {
        // don't accumulate text if the renderer hasn't rendered a frame yet
        if (Renderer::GetFrameNumber() < 1)
            return;

        const float viewport_width  = Renderer::GetViewport().width;
        const float viewport_height = Renderer::GetViewport().height;
        const float aspect_ratio    = viewport_width / viewport_height;

        // adjust the screen percentage position to compensate for the aspect ratio
        Vector2 adjusted_position_percentage;
        adjusted_position_percentage.x = position_screen_percentage.x / aspect_ratio;
        adjusted_position_percentage.y = position_screen_percentage.y;

        // convert the adjusted screen percentage position to actual screen coordinates
        Vector2 position;
        position.x = viewport_width  * adjusted_position_percentage.x;
        position.y = viewport_height * adjusted_position_percentage.y;

        // make the origin be the top left corner
        position.x -= 0.5f * viewport_width;
        position.y += 0.5f * viewport_height;

        // don't yet understand why this is needed, but it corrects a slight y offset
        position.y -= m_char_max_height * 1.5f;

        // set the cursor to the starting position
        Vector2 cursor       = position;
        float starting_pos_x = cursor.x;

        // allocate ahead of time
        {
            // calculate the required size for vertices and indices
            size_t required_vertices_count = text.length() * 6;       // 6 vertices per character
            size_t required_indices_count  = required_vertices_count; // 1 index per vertex

            // reserve memory for class members
            m_vertices.reserve(required_vertices_count);
            m_indices.reserve(required_indices_count);

            // clear existing data (reuse preallocated memory)
            m_vertices.clear();
            m_indices.clear();
        }

        // generate vertices - draw each latter onto a quad
        for (char character : text)
        {
            Glyph& glyph = m_glyphs[character];

            if (character == ASCII_TAB)
            {
                const uint32_t space_offset = m_glyphs[ASCII_SPACE].horizontal_advance;
                const uint32_t tab_spacing  = space_offset * 4; // spaces in a typical editor
                float offset_from_start     = cursor.x - starting_pos_x; // keep as float for precision
                uint32_t next_column_index  = tab_spacing == 0 ? 4 : static_cast<uint32_t>((offset_from_start / tab_spacing) + 1);
                float offset_to_column      = (next_column_index * tab_spacing) - offset_from_start;
                cursor.x                    += offset_to_column; // apply offset to align to next tab stop
            }
            else if (character == ASCII_NEW_LINE)
            {
                cursor.y -= m_char_max_height;
                cursor.x = starting_pos_x;
            }
            else if (character == ASCII_SPACE)
            {
                cursor.x += glyph.horizontal_advance;
            }
            else
            {
                // first triangle in quad.
                m_vertices.emplace_back(cursor.x + glyph.offset_x,                cursor.y + glyph.offset_y,                0.0f, glyph.uv_x_left,  glyph.uv_y_top);    // top left
                m_vertices.emplace_back(cursor.x + glyph.offset_x + glyph.width,  cursor.y + glyph.offset_y - glyph.height, 0.0f, glyph.uv_x_right, glyph.uv_y_bottom); // bottom right
                m_vertices.emplace_back(cursor.x + glyph.offset_x,                cursor.y + glyph.offset_y - glyph.height, 0.0f, glyph.uv_x_left,  glyph.uv_y_bottom); // bottom left

                // second triangle in quad
                m_vertices.emplace_back(cursor.x + glyph.offset_x,                cursor.y + glyph.offset_y,                0.0f, glyph.uv_x_left,  glyph.uv_y_top);    // top left
                m_vertices.emplace_back(cursor.x + glyph.offset_x  + glyph.width, cursor.y + glyph.offset_y,                0.0f, glyph.uv_x_right, glyph.uv_y_top);    // top right
                m_vertices.emplace_back(cursor.x + glyph.offset_x  + glyph.width, cursor.y + glyph.offset_y - glyph.height, 0.0f, glyph.uv_x_right, glyph.uv_y_bottom); // bottom right

                // advance
                cursor.x += glyph.horizontal_advance;
            }
        }

        // generate indices
        for (uint32_t i = 0; i < static_cast<uint32_t>(m_vertices.size()); i++)
        {
            m_indices.emplace_back(i);
        }

        // store the generated data for this text
        m_font_data.emplace_back(m_vertices, m_indices, position);
    }

    bool Font::HasText() const
    {
        return !m_font_data.empty();
    }

    void Font::SetSize(const uint32_t size)
    {
        m_font_size = Helper::Clamp<uint32_t>(size, 8, 50);
    }

    void Font::UpdateVertexAndIndexBuffers(RHI_CommandList* cmd_list)
    {
        if (m_font_data.empty())
            return;
    
        // combine all vertices/indices into one
        vector<RHI_Vertex_PosTex> vertices;
        vector<uint32_t> indices;
        uint32_t vertex_offset = 0;
    
        for (const FontData& text_data : m_font_data)
        {
            // insert vertices for this text entry
            vertices.insert(vertices.end(), text_data.vertices.begin(), text_data.vertices.end());
    
            // adjust indices to reference the correct vertices
            for (uint32_t index : text_data.indices)
            {
                indices.push_back(index + vertex_offset);
            }
    
            // update vertex offset for the next text entry
            vertex_offset += static_cast<uint32_t>(text_data.vertices.size());
        }
    
        // grow buffers
        if (vertices.size() > m_buffer_vertex->GetElementCount())
        {
            m_buffer_vertex = make_shared<RHI_Buffer>(RHI_Buffer_Type::Vertex,
                sizeof(vertices[0]),
                static_cast<uint32_t>(vertices.size()),
                static_cast<void*>(&vertices[0]),
                true,
                "font_vertex"
            );
    
             m_buffer_index = make_shared<RHI_Buffer>(RHI_Buffer_Type::Index,
                sizeof(indices[0]),
                static_cast<uint32_t>(indices.size()),
                static_cast<void*>(&indices[0]),
                true,
                "font_index"
            );
        }
    
        // copy the data over to the gpu
        cmd_list->UpdateBuffer(m_buffer_vertex.get(), 0, m_buffer_vertex->GetObjectSize(), vertices.data());
        cmd_list->UpdateBuffer(m_buffer_index.get(), 0, m_buffer_index->GetObjectSize(), indices.data());

        m_font_data.clear();
    }

    uint32_t Font::GetIndexCount()
    {
        SP_ASSERT(m_buffer_index);
        return m_buffer_index->GetElementCount();
    }
}
