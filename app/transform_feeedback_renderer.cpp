#include <app/transform_feeedback_renderer.h>
#include <glb/opengl.h>
#include <glb/shader_program_builder.h>
#include <glb/camera.h>
#include <glb/framebuffer.h>
#include <tess/tessellator.h>

namespace app
{
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

	struct bbox
	{
		vec3 min = vec3(math::limit_posf());
		vec3 max = vec3(math::limit_negf());

		void expand(const vec3& v)
		{
			min.x = math::min(min.x, v.x);
			min.y = math::min(min.y, v.y);
			min.z = math::min(min.z, v.z);
			max.x = math::max(max.x, v.x);
			max.y = math::max(max.y, v.y);
			max.z = math::max(max.z, v.z);
		}
	};

	static void to_3D(int idx, int max_x, int max_y, int& x, int& y, int& z)
	{
		x = idx % (max_x+1);
		idx /= (max_x+1);
		y = idx % (max_y+1);
		idx /= (max_y+1);
		z = idx;
	}

	bool transform_feedback_renderer::initialize(glb::framebuffer& fbuffer, glb::camera& cam)
	{
		// ----------------------------------------------------------------------------------------------------------------------
		// scene setup
		// ----------------------------------------------------------------------------------------------------------------------

		int n_x = 100;
		int n_y = 100;
		int n_z = 100;
		_instance_count = n_x*n_y*n_z;

		tess::triangle_mesh drawable_lods[NUM_LODS] = {tess::tessellate_cylinder(1, 5, 32),
													   tess::tessellate_cylinder(1, 5, 16),
													   tess::tessellate_cylinder(1, 5, 8)};

		// ----------------------------------------------------------------------------------------------------------------------
		// matrix transforms
		// ----------------------------------------------------------------------------------------------------------------------

		std::vector<mat34> transforms(_instance_count);
		for(unsigned int i = 0; i < transforms.size(); ++i)
		{
			int x, y, z;
			to_3D(i, n_x, n_y, x, y, z);
			transforms[i] = mat34(mat4::translation(vec3((float)x, (float)y, (float)z)*6.0f));
		}

		GLuint tvbo;
		glGenBuffers(1, &tvbo);
		glBindBuffer(GL_ARRAY_BUFFER, tvbo);
		glBufferData(GL_ARRAY_BUFFER, transforms.size()*sizeof(mat34), transforms.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// ----------------------------------------------------------------------------------------------------------------------
		// bounding boxes
		// ----------------------------------------------------------------------------------------------------------------------

		std::vector<bbox> bounds(_instance_count);
		for(unsigned int i = 0; i < bounds.size(); ++i)
		{
			bbox& b = bounds[i];
			for(unsigned int v = 0; v < drawable_lods[0].vertices.size(); ++v)
			{
				b.expand(transforms[i].as_mat4().mul(drawable_lods[0].vertices[v].position));
			}
		}

		GLuint bvbo;
		glGenBuffers(1, &bvbo);
		glBindBuffer(GL_ARRAY_BUFFER, bvbo);
		glBufferData(GL_ARRAY_BUFFER, bounds.size()*sizeof(bbox), bounds.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// ----------------------------------------------------------------------------------------------------------------------
		// cull vao
		// ----------------------------------------------------------------------------------------------------------------------

		glGenVertexArrays(1, &_cull_vao);
		glBindVertexArray(_cull_vao);

		// bounding box
		glBindBuffer(GL_ARRAY_BUFFER, bvbo);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(bbox), GLB_BYTE_OFFSET(0));

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(bbox), GLB_BYTE_OFFSET(sizeof(vec3)));

		// transform
		glBindBuffer(GL_ARRAY_BUFFER, tvbo);

		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(mat34), GLB_BYTE_OFFSET(0));

		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(mat34), GLB_BYTE_OFFSET(sizeof(float)*4));

		glEnableVertexAttribArray(4);
		glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(mat34), GLB_BYTE_OFFSET(sizeof(float)*8));

		glBindVertexArray(0);

		// ----------------------------------------------------------------------------------------------------------------------
		// cull shader
		// ----------------------------------------------------------------------------------------------------------------------

		glb::shader_program_builder spb;
		spb.begin();
		if(!spb.add_file(glb::shader_vertex, "../shaders/lod/cull.vert"))
		{
			return false;
		}
		if(!spb.add_file(glb::shader_geometry, "../shaders/lod/cull.geom"))
		{
			return false;
		}
		spb.bind_vertex_attrib("in_bmin", 0);
		spb.bind_vertex_attrib("in_bmax", 1);
		spb.bind_vertex_attrib("in_transform_0", 2);
		spb.bind_vertex_attrib("in_transform_1", 3);
		spb.bind_vertex_attrib("in_transform_2", 4);
		std::vector<const char*> outputs = {"fdata0.transform_0", "fdata0.transform_1", "fdata0.transform_2", "gl_NextBuffer",
											"fdata1.transform_0", "fdata1.transform_1", "fdata1.transform_2", "gl_NextBuffer",
											"fdata2.transform_0", "fdata2.transform_1", "fdata2.transform_2"};
		glTransformFeedbackVaryings(spb.get_shader_program().get_id(), outputs.size(), outputs.data(), GL_INTERLEAVED_ATTRIBS);
		if(!spb.end())
		{
			return false;
		}
		auto cull_shader = spb.get_shader_program();
		cull_shader.bind_uniform_buffer("cdata", cam.get_uniform_buffer());
		_cull_shader = cull_shader.get_id();

		// ----------------------------------------------------------------------------------------------------------------------
		// visible instance data (transform feedback output, draw input)
		// ----------------------------------------------------------------------------------------------------------------------

		std::vector<mat34> empty(transforms.size());

		for(unsigned int i = 0; i < NUM_LODS; ++i)
		{
			glGenBuffers(1, &_visible_ssbo[i]);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, _visible_ssbo[i]);
			glBufferData(GL_SHADER_STORAGE_BUFFER, empty.size()*sizeof(mat34), empty.data(), GL_STREAM_COPY);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}

		// ----------------------------------------------------------------------------------------------------------------------
		// transform feedback
		// ----------------------------------------------------------------------------------------------------------------------

		glGenTransformFeedbacks(1, &_feedback);
		glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, _feedback);
		for(unsigned int i = 0; i < NUM_LODS; ++i)
		{
			glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, i, _visible_ssbo[i]);
		}
		glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

		// ----------------------------------------------------------------------------------------------------------------------
		// visible query
		// ----------------------------------------------------------------------------------------------------------------------

		for(unsigned int i = 0; i < NUM_LODS; ++i)
		{
			glGenQueries(1, &_visible_query[i]);
		}

		// ----------------------------------------------------------------------------------------------------------------------
		// draw vao
		// ----------------------------------------------------------------------------------------------------------------------

		for(unsigned int i = 0; i < NUM_LODS; ++i)
		{
			_draw_elem_count[i] = drawable_lods[i].elements.size();

			glGenVertexArrays(1, &_draw_vao[i]);
			glBindVertexArray(_draw_vao[i]);

			GLuint vbo;
			glGenBuffers(1, &vbo);
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBufferData(GL_ARRAY_BUFFER, drawable_lods[i].vertices.size()*sizeof(tess::vertex), drawable_lods[i].vertices.data(), GL_STATIC_DRAW);

			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(tess::vertex), GLB_BYTE_OFFSET(0));

			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(tess::vertex), GLB_BYTE_OFFSET(sizeof(vec3)));

			GLuint ebo;
			glGenBuffers(1, &ebo);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, drawable_lods[i].elements.size()*sizeof(tess::element), drawable_lods[i].elements.data(), GL_STATIC_DRAW);

			glBindVertexArray(0);
		}

		// ----------------------------------------------------------------------------------------------------------------------
		// draw shader
		// ----------------------------------------------------------------------------------------------------------------------

		spb.begin();
		if(!spb.add_file(glb::shader_vertex, "../shaders/lod/draw.vert"))
		{
			return false;
		}
		if(!spb.add_file(glb::shader_fragment, "../shaders/lod/draw.frag"))
		{
			return false;
		}
		spb.bind_vertex_attrib("in_position", 0);
		spb.bind_vertex_attrib("in_normal", 1);
		spb.bind_draw_buffer("out_color", fbuffer.get_color_buffer_to_display());
		if(!spb.end())
		{
			return false;
		}
		auto draw_shader = spb.get_shader_program();
		draw_shader.bind_uniform_buffer("cdata", cam.get_uniform_buffer());
		draw_shader.set_uniform("tex_transforms", 0);
		_draw_shader = draw_shader.get_id();

		GLuint block_index = glGetProgramResourceIndex(_draw_shader, GL_SHADER_STORAGE_BLOCK, "tdata");
		glShaderStorageBlockBinding(_draw_shader, block_index, 0);

		return true;
	}

	bool transform_feedback_renderer::finalize()
	{
		return true;
	}

	void transform_feedback_renderer::render()
	{
		// ----------------------------------------------------------------------------------------------------------------------
		// cull and select lod
		// ----------------------------------------------------------------------------------------------------------------------

		glEnable(GL_RASTERIZER_DISCARD);
		glUseProgram(_cull_shader);
		glBindVertexArray(_cull_vao);
		glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, _feedback);
		for(unsigned int i = 0; i < NUM_LODS; ++i)
		{
			glBeginQueryIndexed(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, i, _visible_query[i]);
		}
		glBeginTransformFeedback(GL_POINTS);
		glDrawArrays(GL_POINTS, 0, _instance_count);
		glEndTransformFeedback();
		for(unsigned int i = 0; i < NUM_LODS; ++i)
		{
			glEndQueryIndexed(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, i);
		}
		glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
		glDisable(GL_RASTERIZER_DISCARD);

		// ----------------------------------------------------------------------------------------------------------------------
		// draw visible
		// ----------------------------------------------------------------------------------------------------------------------

		glUseProgram(_draw_shader);

		for(unsigned int i = 0; i < NUM_LODS; ++i)
		{
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _visible_ssbo[i]);
			glBindVertexArray(_draw_vao[i]);
			GLuint visible_instance_count = 0;
			glGetQueryObjectuiv(_visible_query[i], GL_QUERY_RESULT, &visible_instance_count);
			glDrawElementsInstanced(GL_TRIANGLES, _draw_elem_count[i], GL_UNSIGNED_INT, NULL, visible_instance_count);
//			io::print(visible_instance_count, "of", _instance_count, "(", i ,")");
		}
	}
}
