/*
Copyright(c) 2016-2020 Panos Karabelas

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

#pragma once

//= INCLUDES ======================
#include <array>
#include <atomic>
#include "RHI_Definition.h"
#include "../Core/Spartan_Object.h"
//=================================

namespace Spartan
{
    // Forward declarations
	class Profiler;
    class Renderer;
    class Context;
    namespace Math
    {
        class Rectangle;
    }

    enum RHI_Cmd_List_State
    {
        RHI_Cmd_List_Idle,
        RHI_Cmd_List_Recording,
        RHI_Cmd_List_Submittable,
        RHI_Cmd_List_Pending
    };

	class SPARTAN_CLASS RHI_CommandList : public Spartan_Object
	{
	public:
		RHI_CommandList(uint32_t index, RHI_SwapChain* swap_chain, Context* context);
		~RHI_CommandList();

        // Command list
        bool Begin();
        bool Stop();
        bool Submit();
        bool Wait();
        bool Reset();
        bool Flush()
        {
            if (m_cmd_state == RHI_Cmd_List_Recording)
            {
                bool has_render_pass = m_render_pass_active;
                if (has_render_pass)
                {
                    if (!EndRenderPass())
                        return false;
                }

                if (!Stop())
                    return false;

                if (!Submit())
                    return false;

                if (!Begin())
                    return false;

                if (has_render_pass)
                {
                    if (!BeginRenderPass(*m_pipeline_state))
                        return false;
                }
            }

            m_flushed = true;
            return true;
        }

        // Render pass
        bool BeginRenderPass(RHI_PipelineState& pipeline_state);
        bool EndRenderPass();

        // Clear
        void Clear(RHI_PipelineState& pipeline_state);

		// Draw/Dispatch
        bool Draw(uint32_t vertex_count);
		bool DrawIndexed(uint32_t index_count, uint32_t index_offset = 0, uint32_t vertex_offset = 0);
        void Dispatch(uint32_t x, uint32_t y, uint32_t z = 1) const;

		// Viewport
		void SetViewport(const RHI_Viewport& viewport) const;

        // Scissor
		void SetScissorRectangle(const Math::Rectangle& scissor_rectangle) const;

		// Vertex buffer
		void SetBufferVertex(const RHI_VertexBuffer* buffer, const uint64_t offset = 0);

		// Index buffer
		void SetBufferIndex(const RHI_IndexBuffer* buffer, const uint64_t offset = 0);

		// Constant buffer
        bool SetConstantBuffer(const uint32_t slot, const uint8_t scope, RHI_ConstantBuffer* constant_buffer) const;
        inline bool SetConstantBuffer(const uint32_t slot, const uint8_t scope, const std::shared_ptr<RHI_ConstantBuffer>& constant_buffer) const { return SetConstantBuffer(slot, scope, constant_buffer.get()); }

		// Sampler
        void SetSampler(const uint32_t slot, RHI_Sampler* sampler) const;
        inline void SetSampler(const uint32_t slot, const std::shared_ptr<RHI_Sampler>& sampler) const { SetSampler(slot, sampler.get()); }

		// Texture
        void SetTexture(const uint32_t slot, RHI_Texture* texture, const uint8_t scope = RHI_Shader_Pixel);
        inline void SetTexture(const uint32_t slot, const std::shared_ptr<RHI_Texture>& texture, const uint8_t scope = RHI_Shader_Pixel) { SetTexture(slot, texture.get(), scope); }
        
        // Timestamps
        bool Timestamp_Start(void* query_disjoint = nullptr, void* query_start = nullptr);
        bool Timestamp_End(void* query_disjoint = nullptr, void* query_end = nullptr);
        float Timestamp_GetDuration(void* query_disjoint, void* query_start, void* query_end, const uint32_t pass_index);

        static uint32_t Gpu_GetMemory(RHI_Device* rhi_device);
        static uint32_t Gpu_GetMemoryUsed(RHI_Device* rhi_device);
        static bool Gpu_QueryCreate(RHI_Device* rhi_device, void** query = nullptr, RHI_Query_Type type = RHI_Query_Timestamp);
        static void Gpu_QueryRelease(void*& query_object);
        
        // Misc
        void* GetResource_CommandBuffer() const { return m_cmd_buffer; }
        bool IsRecording() const;
        bool IsPending() const;
        bool IsIdle() const;
        void*& GetProcessedSemaphore() { return m_processed_semaphore; }

	private:
        void Timeblock_Start(const RHI_PipelineState* pipeline_state);
        void Timeblock_End(const RHI_PipelineState* pipeline_state);
        bool Deferred_BeginRenderPass();
        bool Deferred_BindPipeline();
        bool Deferred_BindDescriptorSet();
        bool OnDraw();

        std::atomic<RHI_Cmd_List_State> m_cmd_state = RHI_Cmd_List_Idle;
		RHI_Pipeline* m_pipeline	                = nullptr; 
        RHI_SwapChain* m_swap_chain                 = nullptr;
        Renderer* m_renderer                        = nullptr;
        RHI_PipelineCache* m_pipeline_cache         = nullptr;
        RHI_DescriptorCache* m_descriptor_cache     = nullptr;
        RHI_PipelineState* m_pipeline_state         = nullptr;
        RHI_Device* m_rhi_device                    = nullptr;
        Profiler* m_profiler                        = nullptr;
        void* m_cmd_buffer                          = nullptr;
        void* m_processed_fence                     = nullptr;
        void* m_processed_semaphore                 = nullptr;
        void* m_query_pool                          = nullptr;
        bool m_render_pass_active                   = false;
        bool m_pipeline_active                      = false;
        bool m_flushed                              = false;
        std::mutex m_mutex_reset;

        // Profiling
        uint32_t m_timestamp_index = 0;
        static const uint32_t m_max_timestamps = 256;
        std::array<uint64_t, m_max_timestamps> m_timestamps;

        // Variables to minimise state changes
        uint32_t m_vertex_buffer_id     = 0;
        uint64_t m_vertex_buffer_offset = 0;
        uint32_t m_index_buffer_id      = 0;
        uint64_t m_index_buffer_offset  = 0;
	};
}
