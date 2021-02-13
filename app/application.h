#pragma once
#include <glv/iapplication.h>
#include <glb/engine.h>
#include <app/transform_feeedback_renderer.h>
#include <app/compute_renderer.h>

namespace app
{
	class application : public glv::iapplication
	{
	public:
		virtual void display(float* view_matrix) override;
		virtual void reshape(int w, int h) override;
		virtual bool key_press(unsigned char key, int x, int y) override;
		virtual bool initialize() override;
		virtual bool finalize() override;

	private:
		app::transform_feedback_renderer _transform_feedback_renderer;
		app::compute_renderer _compute_renderer;
		glb::engine _engine;
	};
}
