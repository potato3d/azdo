#pragma once
#include <glb/irenderer.h>
#include <glb/opengl.h>

namespace glb
{
	class framebuffer;
}

namespace app
{
	class compute_renderer : public glb::irenderer
	{
	public:
		virtual bool initialize(glb::framebuffer& fbuffer, glb::camera& cam) override;
		virtual bool finalize() override;
		virtual void render() override;

	private:
		struct mat34
		{
			mat34(){}

			explicit mat34(const mat4& m)
			{
				int i = 0;
				data[i++] = m.at(0,0);
				data[i++] = m.at(0,1);
				data[i++] = m.at(0,2);
				data[i++] = m.at(0,3);

				data[i++] = m.at(1,0);
				data[i++] = m.at(1,1);
				data[i++] = m.at(1,2);
				data[i++] = m.at(1,3);

				data[i++] = m.at(2,0);
				data[i++] = m.at(2,1);
				data[i++] = m.at(2,2);
				data[i++] = m.at(2,3);
			}

			mat4 as_mat4() const
			{
				return mat4(data[0], data[1], data[2], data[3],
							data[4], data[5], data[6], data[7],
							data[8], data[9], data[10], data[11],
							0,0,0,1);
			}

			float data[12];
		};

		struct DrawElementsIndirectCommand
		{
			GLuint  elementCount;
			GLuint  instanceCount;
			GLuint  firstElement;
			GLuint  baseVertex;
			GLuint  baseInstance;
		};

		bool initialize_from_file();
		bool initialize_single_mesh();
		bool initialize_common(const vector<mat34>& transforms);
		void render_hiz_last_frame();
		void render_hiz_temporal();
		void render_raster_temporal();
		void print_per_lod_instance_count();

		static const int NUM_LODS = 4;
		unsigned int _instance_count;
		unsigned int _compute_count;

		GLuint _draw_vao;
		GLuint _draw_program;
		GLuint _input_transform_ssbo;
		GLuint _input_bound_ssbo;
		GLuint _visible_instance_id_ssbo[NUM_LODS];
		GLuint _draw_indirect_buffer;
		GLuint _hiz_cull_lastframe_program;

		DrawElementsIndirectCommand _draw_commands[NUM_LODS];

		glb::framebuffer* _fbuffer;
		glb::camera* _camera;

		GLuint _hiz_cull_program;
		GLuint _raster_cull_program;

		GLuint _collect_curr_program;
		GLuint _collect_curr_notlast_program;

		GLuint _curr_visible_ssbo;
		GLuint _last_visible_ssbo;

		GLuint _raster_curr_ssbo;

		GLuint _instance_offset_ubo;

		int _unique_count;
		vector<int> _unique_compute_counts;
		vector<int> _instance_offsets;
	};
}
