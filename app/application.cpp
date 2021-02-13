#include <app/application.h>
#include <glv/viewer.h>

namespace app
{
	void application::display(float* view_matrix)
	{
		_engine.set_view(view_matrix);
		_engine.render();
	}

	void application::reshape(int w, int h)
	{
		_engine.resize_screen(w, h);
	}

	bool application::key_press(unsigned char /*key*/, int /*x*/, int /*y*/)
	{
		return false;
	}

	bool application::initialize()
	{
		glv::viewer::set_default_camera_look_at(-165.036, -61.5545, -164.548,  -164.447, -61.0983, -163.881,  0.0f, 0.0f, 1.0f);

		//		return _engine.initialize(&_transform_feedback_renderer);
		return _engine.initialize(&_compute_renderer);
	}

	bool application::finalize()
	{
		return _engine.finalize();
	}
}
