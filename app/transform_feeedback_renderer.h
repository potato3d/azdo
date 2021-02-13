#pragma once
#include <glb/irenderer.h>

namespace app
{
	class transform_feedback_renderer : public glb::irenderer
	{
	public:
		virtual bool initialize(glb::framebuffer& fbuffer, glb::camera& cam) override;
		virtual bool finalize() override;
		virtual void render() override;

	private:
		static const unsigned int NUM_LODS = 3;

		unsigned int _instance_count;

		unsigned int _cull_vao;
		unsigned int _cull_shader;
		unsigned int _feedback;
		unsigned int _visible_query[NUM_LODS];

		unsigned int _visible_ssbo[NUM_LODS];

		unsigned int _draw_vao[NUM_LODS];
		unsigned int _draw_elem_count[NUM_LODS];
		unsigned int _draw_shader;
	};
}
