#include <app/compute_renderer.h>
#include <glb/opengl.h>
#include <glb/camera.h>
#include <glb/shader_program_builder.h>
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
		float pad0 = 1.0f;
		vec3 max = vec3(math::limit_negf());
		float pad1 = 1.0f;

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
		x = idx % (max_x);
		idx /= (max_x);
		y = idx % (max_y);
		idx /= (max_y);
		z = idx;
	}

	static int get_num_blocks(int total, int block_size)
	{
		return (total + block_size - 1) / block_size;
	}

	static void print_compute_info()
	{
		GLint c = 0;
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &c);
		io::print("GL_MAX_COMPUTE_WORK_GROUP_COUNT in X:", c);
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &c);
		io::print("GL_MAX_COMPUTE_WORK_GROUP_SIZE in X:", c);
		glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &c);
		io::print("GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS:", c);
	}

	bool compute_renderer::initialize(glb::framebuffer& fbuffer, glb::camera& cam)
	{
		// ----------------------------------------------------------------------------------------------------------------------
		// configure framebuffer for hi-z map
		// ----------------------------------------------------------------------------------------------------------------------

		fbuffer.set_use_textures(true);
		fbuffer.set_create_depth_mipmaps(true);
		_fbuffer = &fbuffer;

		// ----------------------------------------------------------------------------------------------------------------------
		// scene setup
		// ----------------------------------------------------------------------------------------------------------------------

		int n_x = 100;
		int n_y = 100;
		int n_z = 100;
		_instance_count = n_x*n_y*n_z;

		_compute_count = get_num_blocks(_instance_count, 256);
		_bit_compute_count = get_num_blocks(_instance_count, 256);

		tess::triangle_mesh drawable_lods[NUM_LODS];

		int resolution = pow(2, NUM_LODS+1);
		unsigned int total_vertex_count = 0;
		unsigned int total_element_count = 0;
		unsigned int element_offsets[NUM_LODS] = {0};
		unsigned int eoffset = 0;
		unsigned int vertex_offsets[NUM_LODS] = {0};
		unsigned int voffset = 0;

		for(int i = 0; i < NUM_LODS; ++i)
		{
			drawable_lods[i] = tess::tessellate_cylinder(1, 5, resolution);
			total_vertex_count += drawable_lods[i].vertices.size();
			total_element_count += drawable_lods[i].elements.size();
			element_offsets[i] = eoffset;
			eoffset += drawable_lods[i].elements.size();
			vertex_offsets[i] = voffset;
			voffset += drawable_lods[i].vertices.size();
			resolution >>= 1;
		}

		// ----------------------------------------------------------------------------------------------------------------------
		// compute input 1: matrix transforms
		// ----------------------------------------------------------------------------------------------------------------------

		std::vector<mat34> transforms(_instance_count);
		for(unsigned int i = 0; i < transforms.size(); ++i)
		{
			int x, y, z;
			to_3D(i, n_x, n_y, x, y, z);
			transforms[i] = mat34(mat4::translation(vec3((float)x, (float)y, (float)z)*6.0f));
		}

		glGenBuffers(1, &_input_transform_ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, _input_transform_ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER, _instance_count*sizeof(mat34), transforms.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		// ----------------------------------------------------------------------------------------------------------------------
		// compute input 2: bounding boxes
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

		glGenBuffers(1, &_input_bound_ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, _input_bound_ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER, _instance_count*sizeof(bbox), bounds.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		// ----------------------------------------------------------------------------------------------------------------------
		// compute output 1: instance IDs for each LOD
		// ----------------------------------------------------------------------------------------------------------------------

		vector<GLuint> ids(_instance_count);
		for(unsigned int i = 0; i < ids.size(); ++i)
		{
			ids[i] = i;
		}

		for(int i = 0; i < NUM_LODS; ++i)
		{
			glGenBuffers(1, &_visible_instance_id_ssbo[i]);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, _visible_instance_id_ssbo[i]);
			glBufferData(GL_SHADER_STORAGE_BUFFER, _instance_count*sizeof(GLuint), ids.data(), GL_STATIC_DRAW);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}

		// ----------------------------------------------------------------------------------------------------------------------
		// compute output 2: indirect draw commands
		// ----------------------------------------------------------------------------------------------------------------------

		for(int i = 0; i < NUM_LODS; ++i)
		{
			DrawElementsIndirectCommand cmd;
			cmd.elementCount = drawable_lods[i].elements.size();
			cmd.instanceCount = 0; // atomic counter incremented by the compute shader
			cmd.firstElement = element_offsets[i];
			cmd.baseVertex = vertex_offsets[i];
			cmd.baseInstance = i; // references lod level vertex attribute
			_draw_commands[i] = cmd;
		}

		_draw_commands[NUM_LODS-1].instanceCount = _instance_count;

		glGenBuffers(1, &_draw_indirect_buffer);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _draw_indirect_buffer);
		glBufferData(GL_DRAW_INDIRECT_BUFFER, NUM_LODS*sizeof(DrawElementsIndirectCommand), _draw_commands, GL_STATIC_DRAW);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

		_draw_commands[NUM_LODS-1].instanceCount = 0;

		// ----------------------------------------------------------------------------------------------------------------------
		// compute shader
		// ----------------------------------------------------------------------------------------------------------------------

		glb::shader_program_builder compute_shader_builder;
		compute_shader_builder.begin();
		if(!compute_shader_builder.add_file(glb::shader_compute, "../shaders/lod/cull.cs"))
		{
			return false;
		}
		if(!compute_shader_builder.end())
		{
			return false;
		}
		auto program = compute_shader_builder.get_shader_program();
		program.bind_uniform_buffer("camera_data", cam.get_uniform_buffer());
		_hiz_cull_lastframe_program = program.get_id();

		glProgramUniform1ui(_hiz_cull_lastframe_program, 0, _instance_count);

		// ----------------------------------------------------------------------------------------------------------------------
		// drawing vertex arrays
		// ----------------------------------------------------------------------------------------------------------------------

		glGenVertexArrays(1, &_draw_vao);
		glBindVertexArray(_draw_vao);

		GLuint vbo;
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, total_vertex_count*sizeof(tess::vertex), nullptr, GL_STATIC_DRAW);

		unsigned int offset = 0;
		for(int i = 0; i < NUM_LODS; ++i)
		{
			unsigned int size = drawable_lods[i].vertices.size()*sizeof(tess::vertex);
			glBufferSubData(GL_ARRAY_BUFFER, offset, size, drawable_lods[i].vertices.data());
			offset += size;
		}

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(tess::vertex), GLB_BYTE_OFFSET(0));

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(tess::vertex), GLB_BYTE_OFFSET(sizeof(vec3)));

		GLuint lbo;
		glGenBuffers(1, &lbo);
		glBindBuffer(GL_ARRAY_BUFFER, lbo);

		GLuint lod_levels[NUM_LODS];
		for(int i = 0; i < NUM_LODS; ++i)
		{
			lod_levels[i] = i;
		}
		glBufferData(GL_ARRAY_BUFFER, NUM_LODS*sizeof(GLuint), lod_levels, GL_STATIC_DRAW);

		glEnableVertexAttribArray(2);
		glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, 0, GLB_BYTE_OFFSET(0));
		glVertexAttribDivisor(2, _instance_count + 1); // make sure (instanceID / divisor) is 0 so lod_level vertex attrib equals drawCmd.baseInstance

		GLuint ebo;
		glGenBuffers(1, &ebo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, total_element_count*sizeof(tess::element), nullptr, GL_STATIC_DRAW);

		offset = 0;
		for(int i = 0; i < NUM_LODS; ++i)
		{
			unsigned int size = drawable_lods[i].elements.size()*sizeof(tess::element);
			glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset, size, drawable_lods[i].elements.data());
			offset += size;
		}

		glBindVertexArray(0);

		// ----------------------------------------------------------------------------------------------------------------------
		// drawing shader
		// ----------------------------------------------------------------------------------------------------------------------

		glb::shader_program_builder draw_shader_builder;
		draw_shader_builder.begin();
		if(!draw_shader_builder.add_file(glb::shader_vertex, "../shaders/lod/draw.vs"))
		{
			return false;
		}
		if(!draw_shader_builder.add_file(glb::shader_fragment, "../shaders/lod/draw.fs"))
		{
			return false;
		}
		if(!draw_shader_builder.end())
		{
			return false;
		}
		auto draw_shader = draw_shader_builder.get_shader_program();
		draw_shader.bind_uniform_buffer("cdata", cam.get_uniform_buffer());
		_draw_program = draw_shader.get_id();

		// ----------------------------------------------------------------------------------------------------------------------
		// associate buffers with indexed binding points
		// ----------------------------------------------------------------------------------------------------------------------

		unsigned int ssbo_binding = 0;

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, ssbo_binding++, _input_bound_ssbo);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, ssbo_binding++, _input_transform_ssbo);

		for(int i = 0; i < NUM_LODS; ++i)
		{
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, ssbo_binding++, _visible_instance_id_ssbo[i]);
			glBindBufferRange(GL_ATOMIC_COUNTER_BUFFER, i, _draw_indirect_buffer, 4+sizeof(DrawElementsIndirectCommand)*i, sizeof(GLuint));
		}








		// ----------------------------------------------------------------------------------------------------------------------
		// temporal hi-z culling shader
		// ----------------------------------------------------------------------------------------------------------------------

		compute_shader_builder.begin();
		if(!compute_shader_builder.add_file(glb::shader_compute, "../shaders/lod/temporal/hiz_cull.cs"))
		{
			return false;
		}
		if(!compute_shader_builder.end())
		{
			return false;
		}
		program = compute_shader_builder.get_shader_program();
		program.bind_uniform_buffer("camera_data", cam.get_uniform_buffer());
		_hiz_cull_program = program.get_id();

		glProgramUniform1ui(_hiz_cull_program, 0, _instance_count);

		// ----------------------------------------------------------------------------------------------------------------------
		// bit-packing current shader
		// ----------------------------------------------------------------------------------------------------------------------

		compute_shader_builder.begin();
		if(!compute_shader_builder.add_file(glb::shader_compute, "../shaders/lod/temporal/bits_curr.cs"))
		{
			return false;
		}
		if(!compute_shader_builder.end())
		{
			return false;
		}
		_bits_curr_program = compute_shader_builder.get_shader_program().get_id();

		glProgramUniform1ui(_bits_curr_program, 0, _instance_count);

		// ----------------------------------------------------------------------------------------------------------------------
		// bit-packing current and not last shader
		// ----------------------------------------------------------------------------------------------------------------------

		compute_shader_builder.begin();
		if(!compute_shader_builder.add_file(glb::shader_compute, "../shaders/lod/temporal/bits_curr_notlast.cs"))
		{
			return false;
		}
		if(!compute_shader_builder.end())
		{
			return false;
		}
		_bits_curr_notlast_program = compute_shader_builder.get_shader_program().get_id();

		glProgramUniform1ui(_bits_curr_notlast_program, 0, _instance_count);

		// ----------------------------------------------------------------------------------------------------------------------
		// collect visible instance shader
		// ----------------------------------------------------------------------------------------------------------------------

		compute_shader_builder.begin();
		if(!compute_shader_builder.add_file(glb::shader_compute, "../shaders/lod/temporal/collect_instances.cs"))
		{
			return false;
		}
		if(!compute_shader_builder.end())
		{
			return false;
		}
		_collect_instances_program = compute_shader_builder.get_shader_program().get_id();

		glProgramUniform1ui(_collect_instances_program, 0, _instance_count);

		// ----------------------------------------------------------------------------------------------------------------------
		// visible flags ssbo
		// ----------------------------------------------------------------------------------------------------------------------

		glGenBuffers(1, &_visible_flags_ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, _visible_flags_ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER, _instance_count*sizeof(GLuint), nullptr, GL_STATIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, _visible_flags_ssbo);

		// ----------------------------------------------------------------------------------------------------------------------
		// current visible bits ssbo
		// ----------------------------------------------------------------------------------------------------------------------

		vector<GLuint> bits(_instance_count, 0xFFFFFFFF);

		glGenBuffers(1, &_curr_visible_bits_ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, _curr_visible_bits_ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER, _instance_count*sizeof(GLuint), bits.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, _curr_visible_bits_ssbo);

		// ----------------------------------------------------------------------------------------------------------------------
		// last visible bits ssbo
		// ----------------------------------------------------------------------------------------------------------------------

		glGenBuffers(1, &_last_visible_bits_ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, _last_visible_bits_ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER, _instance_count*sizeof(GLuint), bits.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 12, _last_visible_bits_ssbo);




//		_fbuffer->set_use_same_depth_for_mipmaps(true);


		return true;
	}

	bool compute_renderer::finalize()
	{
		return true;
	}

	void compute_renderer::render()
	{
//		render_hiz_last_frame();
		render_hiz_temporal();
	}

	void compute_renderer::render_raster_temporal()
	{
		// TODO: use int or bool to store visible flags?
		// TODO: really need to compact visible flags into bit array?

		// 1. multi draw indirect

		// 2. raster all bboxes against current z-buffer
		// 2.1. draw points for entire scene, expand to 3 box faces using geometry shader, and store visible flag in fragment shader
		// 2.2. pack visible flags into current bit array: current &= ~last (OPTIONAL)
		// 2.3. collect instance ids based on current bit array

		// 3. multi draw indirect

		// 4. prepare data for next frame
		// 4.1. pack visible flags into current bit array: current (OPTIONAL)
		// 4.2. collect instance ids based on current bit array
		// 4.3. swap current with last visible bit arrays
	}

	void compute_renderer::render_hiz_temporal()
	{
		// TODO: use int or bool to store visible flags?
		// TODO: really need to compact visible flags into bit array?
		// TODO: why use flags instead of storing instance id directly?

//{
//	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _draw_indirect_buffer);

//	vector<DrawElementsIndirectCommand> cmds(NUM_LODS);
//	glGetBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, NUM_LODS*sizeof(DrawElementsIndirectCommand), cmds.data());
//	io::print();

//	for(int i = 0; i < NUM_LODS; ++i)
//	{
//		std::vector<GLuint> ids(_instance_count);
//		glBindBuffer(GL_SHADER_STORAGE_BUFFER, _visible_instance_id_ssbo[i]);
//		glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, _instance_count*sizeof(GLuint), ids.data());
//		io::print();
//	}
//}

//print_per_lod_instance_count();


		// 1. draw previously visible
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _draw_indirect_buffer);
		glBindVertexArray(_draw_vao);
		glUseProgram(_draw_program);
		glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, GLB_BYTE_OFFSET(0), NUM_LODS, 0);

		// 2. test all bboxes for visibily
		// 2.1. build depth mipmaps
		_fbuffer->update_depth_mipmaps();
		// 2.2. perform frustum culling, occlusion culling using hi-z, and store visible flag
		glUseProgram(_hiz_cull_program);
		glDispatchCompute(_compute_count, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);


//{
//	vector<GLuint> flags(_instance_count);
//	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _visible_flags_ssbo);
//	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, _instance_count*sizeof(GLuint), flags.data());
//	io::print();
//}


//{
//	vector<GLuint> bits(get_num_blocks(_instance_count, 32));
//	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _curr_visible_bits_ssbo);
//	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, bits.size()*sizeof(GLuint), bits.data());
//	io::print();
//}

//{
//	vector<GLuint> bits(get_num_blocks(_instance_count, 32));
//	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _last_visible_bits_ssbo);
//	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, bits.size()*sizeof(GLuint), bits.data());
//	io::print();
//}


		// 2.3. pack visible flags into current bit array: current &= ~last (OPTIONAL)
		glUseProgram(_bits_curr_notlast_program);
		glDispatchCompute(_bit_compute_count, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

//{
//	vector<GLuint> bits(get_num_blocks(_instance_count, 32));
//	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _curr_visible_bits_ssbo);
//	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, bits.size()*sizeof(GLuint), bits.data());
//	io::print();
//}




		// 2.4. collect instance ids based on current bit array
		glBufferData(GL_DRAW_INDIRECT_BUFFER, NUM_LODS*sizeof(DrawElementsIndirectCommand), _draw_commands, GL_STATIC_DRAW);
		glUseProgram(_collect_instances_program);
		glDispatchCompute(_compute_count, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);


//{
//	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _draw_indirect_buffer);

//	vector<DrawElementsIndirectCommand> cmds(NUM_LODS);
//	glGetBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, NUM_LODS*sizeof(DrawElementsIndirectCommand), cmds.data());
//	io::print();

//	for(int i = 0; i < NUM_LODS; ++i)
//	{
//		std::vector<GLuint> ids(_instance_count);
//		glBindBuffer(GL_SHADER_STORAGE_BUFFER, _visible_instance_id_ssbo[i]);
//		glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, _instance_count*sizeof(GLuint), ids.data());
//		io::print();
//	}
//}



		// 3. draw newly visible in current frame
		glUseProgram(_draw_program);
		glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, GLB_BYTE_OFFSET(0), NUM_LODS, 0);

		// 4. prepare data for next frame
		// 4.1. pack visible flags into current bit array: current (OPTIONAL)
		glUseProgram(_bits_curr_program);
		glDispatchCompute(_bit_compute_count, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

//{
//	vector<GLuint> bits(get_num_blocks(_instance_count, 32));
//	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _curr_visible_bits_ssbo);
//	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, bits.size()*sizeof(GLuint), bits.data());
//	io::print();
//}


		// 4.2. collect instance ids based on current bit array
		glUseProgram(_collect_instances_program);
		glDispatchCompute(_compute_count, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

//{
//	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _draw_indirect_buffer);

//	vector<DrawElementsIndirectCommand> cmds(NUM_LODS);
//	glGetBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, NUM_LODS*sizeof(DrawElementsIndirectCommand), cmds.data());
//	io::print();

//	for(int i = 0; i < NUM_LODS; ++i)
//	{
//		std::vector<GLuint> ids(_instance_count);
//		glBindBuffer(GL_SHADER_STORAGE_BUFFER, _visible_instance_id_ssbo[i]);
//		glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, _instance_count*sizeof(GLuint), ids.data());
//		io::print();
//	}
//}

		// 4.3. swap current with last visible bit arrays
		static bool invert = true;
		if(invert)
		{
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 12, _curr_visible_bits_ssbo);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, _last_visible_bits_ssbo);
		}
		else
		{
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, _curr_visible_bits_ssbo);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 12, _last_visible_bits_ssbo);
		}
		invert ^= 1;
	}

	void compute_renderer::render_hiz_last_frame()
	{
		// frustum cull + occlusion cull + select lod
		glUseProgram(_hiz_cull_lastframe_program);
		_fbuffer->bind_depth_texture();
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _draw_indirect_buffer);
		glBufferData(GL_DRAW_INDIRECT_BUFFER, NUM_LODS*sizeof(DrawElementsIndirectCommand), _draw_commands, GL_STATIC_DRAW);
		glDispatchCompute(_compute_count, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

		// render
		glUseProgram(_draw_program);
		glBindVertexArray(_draw_vao);
		glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, GLB_BYTE_OFFSET(0), NUM_LODS, 0);

		// update hi-z map for next frame
		_fbuffer->update_depth_mipmaps();

		// DEBUG
//		print_per_lod_instance_count();
//		_fbuffer->draw_depth_mipmap(0);
	}

	void compute_renderer::print_per_lod_instance_count()
	{
		int total = 0;
		std::vector<DrawElementsIndirectCommand> cmds(NUM_LODS);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _draw_indirect_buffer);
		glGetBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, NUM_LODS*sizeof(DrawElementsIndirectCommand), cmds.data());
		for(const auto& cmd : cmds)
		{
			io::print(cmd.instanceCount);
			total += cmd.instanceCount;
		}
		io::print("total:", total, "--------------------------");
	}
}
