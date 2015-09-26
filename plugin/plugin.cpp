#include "plugin_foundation/plugin_api.h"
#include "plugin_foundation/id_string.h"
#include "plugin_foundation/array.h"
#include "plugin_foundation/allocator.h"
#include "plugin_foundation/matrix4x4.h"
#include "plugin_foundation/vector3.h"
#include "plugin_foundation/random.h"
#include "plugin_foundation/stream.h"

#include <string.h>

using namespace PLUGIN_NAMESPACE::stingray_plugin_foundation;

namespace {

	LuaApi *_lua;
	RenderBufferApi *_render_buffer;
	MeshObjectApi *_mesh_api;
	ApplicationApi *_application_api;
	ResourceManagerApi *_resource_api;
	AllocatorApi *_allocator_api;
	SceneGraphApi *_scene_graph;
	UnitApi *_unit;

	AllocatorObject *_allocator_object;
	ApiAllocator _allocator = ApiAllocator(nullptr, nullptr);

	struct Building
	{
		float xmin, xmax;
		float ymin, ymax;
		float height;
	};

	struct Block {
		float xmin, xmax;
		float ymin, ymax;
		float max_height;
		int num_buildings;
		Building *buildings;
	};

	struct Vertex
	{
		Vector3 position;
		Vector3 normal;
	};

	typedef uint16_t Index;

	struct Object {
		Unit *unit;
		uint32_t node_name_id;
		void *material;

		uint32_t vbuffer;
		uint32_t ibuffer;
		uint32_t vdecl;
		uint32_t mesh;
		Array<Vertex> *vertices;
		Array<uint16_t> *indices;
	};
	Object *_object = nullptr;

	Vertex vertex(float x, float y, float z)
	{
		Vertex v;
		v.position = vector3(x, y, z);
		v.normal = vector3(0, 0, 1);
		return v;
	}

	void draw_aabb(Array<Vertex> &vb, Array<Index> &ib, const Vector3 &min, const Vector3 &max)
	{
		auto v0 = vb.size();

		vb.push_back(vertex(min.x, min.y, min.z));
		vb.push_back(vertex(min.x, min.y, max.z));
		vb.push_back(vertex(min.x, max.y, min.z));
		vb.push_back(vertex(min.x, max.y, max.z));
		vb.push_back(vertex(max.x, min.y, min.z));
		vb.push_back(vertex(max.x, min.y, max.z));
		vb.push_back(vertex(max.x, max.y, min.z));
		vb.push_back(vertex(max.x, max.y, max.z));

		Index ids[36] = {0,1,2, 1,3,2, 1,5,3, 5,7,3, 5,4,7, 4,6,7, 4,0,6, 0,2,6, 2,3,6, 3,7,6, 4,5,0, 5,1,0};

		for (auto id : ids)
			ib.push_back(v0 + id);
	}

	Block *make_block(unsigned seed, float xmin, float xmax, float ymin, float ymax)
	{
		Random r(seed);

		int n = 1500;
		Block &b = *(Block *)_allocator.allocate(sizeof(Block) + n * sizeof(Building));
		b.xmin = xmin;
		b.xmax = xmax;
		b.ymin = ymin;
		b.ymax = ymax;
		b.max_height = 100;
		b.num_buildings = n;
		b.buildings = (Building *)(&b+1);

		for (int i=0; i<n; ++i) {
			Building &bu = b.buildings[i];
			bu.xmin = 1.0f+i*2.0f + r(-0.1f, 0.1f);
			bu.ymin = 0.0f + r(-0.1f, 0.1f);
			while (bu.xmin+2 > b.xmax)
			{
				bu.xmin -= b.xmax - 3;
				bu.ymin += 3;
			}
			bu.xmax = bu.xmin + 1;
			bu.ymax = bu.ymin + 2;
			bu.height = r(1.0f, 5.0f);
		}
		return &b;
	}

	void render_block(Block &b, Array<Vertex> &vb, Array<Index> &ib)
	{
		// Draw ground
		draw_aabb(vb, ib, vector3(b.xmin, b.ymin, -1), vector3(b.xmax, b.ymax, 0.05f));

		for (int i=0; i<b.num_buildings; ++i) {
			const Building &bu = b.buildings[i];
			draw_aabb(vb, ib, vector3(bu.xmin, bu.ymin, 0), vector3(bu.xmax, bu.ymax, bu.height));
		}
	}

	void make_city(Unit *unit, uint32_t node_name_id, void *material)
	{
		_object = MAKE_NEW(_allocator, Object);
		Object &o = *_object;

		o.unit = unit;
		o.node_name_id = node_name_id;
		o.material = material;

		const RB_VertexChannel vchannels[] = {
			{ _render_buffer->format(RB_ComponentType::RB_FLOAT_COMPONENT, true, false, 32, 32, 32, 0), RB_VertexSemantic::RB_POSITION_SEMANTIC, 0, 0, false },
			{ _render_buffer->format(RB_ComponentType::RB_FLOAT_COMPONENT, true, false, 32, 32, 32, 0), RB_VertexSemantic::RB_NORMAL_SEMANTIC, 0, 0, false },
		};
		const uint32_t n_channels = sizeof(vchannels) / sizeof(RB_VertexChannel);

		RB_VertexDescription vdesc;
		vdesc.n_channels = n_channels;
		uint32_t stride = 0;
		for (uint32_t i = 0; i != n_channels; ++i) {
			stride += _render_buffer->num_bits(vchannels[i].format) / 8;
			uint32_t n_components = _render_buffer->num_components(vchannels[i].format);
			vdesc.channels[i] = vchannels[i];
		}

		o.vdecl = _render_buffer->create_description(RB_Description::RB_VERTEX_DESCRIPTION, &vdesc);

		RB_VertexBufferView vb_view;
		vb_view.stride = stride;

		RB_IndexBufferView ib_view;
		ib_view.stride = 2;

		o.indices = MAKE_NEW(_allocator, Array<uint16_t>, _allocator);
		o.vertices = MAKE_NEW(_allocator, Array<Vertex>, _allocator);

		Block *block = make_block(0, 0.0f, 100.0f, 0.0f, 100.0f);
		render_block(*block, *o.vertices, *o.indices);

		o.vbuffer = _render_buffer->create_buffer(o.vertices->size() * vb_view.stride, RB_Validity::RB_VALIDITY_STATIC, RB_View::RB_VERTEX_BUFFER_VIEW, &vb_view, o.vertices->begin());
		o.ibuffer = _render_buffer->create_buffer(o.indices->size() * ib_view.stride, RB_Validity::RB_VALIDITY_STATIC, RB_View::RB_INDEX_BUFFER_VIEW, &ib_view, o.indices->begin());

		o.mesh = _mesh_api->create(unit, node_name_id, MO_Flags::MO_VIEWPORT_VISIBLE_FLAG | MO_Flags::MO_SHADOW_CASTER_FLAG);

		float bv_min[] = { block->xmin, block->ymin, -1.0f };
		float bv_max[] = { block->xmax, block->ymax, block->max_height };
		_mesh_api->set_bounding_box(o.mesh, bv_min, bv_max);

		MO_BatchInfo batch_info = { MO_PrimitiveType::MO_TRIANGLE_LIST, 0, 0, o.indices->size() / 3, 0, 0, 1 };
		_mesh_api->set_batch_info(o.mesh, 1, &batch_info);

		_mesh_api->add_resource(o.mesh, _render_buffer->lookup_resource(o.vbuffer));
		_mesh_api->add_resource(o.mesh, _render_buffer->lookup_resource(o.vdecl));
		_mesh_api->add_resource(o.mesh, _render_buffer->lookup_resource(o.ibuffer));

		void *materials[] = { material };
		_mesh_api->set_materials(o.mesh, 1, materials);
	}

	void destroy_city()
	{
		if (!_object) return;

		Object &o = *_object;

		_render_buffer->destroy_buffer(o.ibuffer);
		_render_buffer->destroy_buffer(o.vbuffer);
		_render_buffer->destroy_description(o.vdecl);
		_mesh_api->destroy(o.mesh);

		MAKE_DELETE(_allocator, o.vertices);
		MAKE_DELETE(_allocator, o.indices);
		MAKE_DELETE(_allocator, _object);
		_object = nullptr;
	}

	void init_api(GetApiFunction get_engine_api)
	{
		_application_api = (ApplicationApi*)get_engine_api(APPLICATION_API_ID);
		_render_buffer = (RenderBufferApi*)get_engine_api(RENDER_BUFFER_API_ID);
		_mesh_api = (MeshObjectApi*)get_engine_api(MESH_API_ID);
		_resource_api = (ResourceManagerApi*)get_engine_api(RESOURCE_MANAGER_API_ID);
		_unit = (UnitApi*)get_engine_api(UNIT_API_ID);
		_scene_graph = (SceneGraphApi*)get_engine_api(SCENE_GRAPH_API_ID);
		_allocator_api = (AllocatorApi*)get_engine_api(ALLOCATOR_API_ID);

		_allocator_object = _allocator_api->make_plugin_allocator("City");
		_allocator = ApiAllocator(_allocator_api, _allocator_object);

		_lua = (LuaApi*)get_engine_api(LUA_API_ID);
	}

	void setup_game(GetApiFunction get_engine_api)
	{
		init_api(get_engine_api);

		_lua->add_module_function("City", "make_city", [](lua_State *L)
		{
			auto unit = _lua->getunit(L, 1);
			auto node_name = _lua->tolstring(L, 2, NULL);
			auto material_name = _lua->tolstring(L, 3, NULL);

			uint32_t node_name_id = IdString64(node_name).id() >> 32;
			auto material = _resource_api->get("material", material_name);

			make_city(unit, node_name_id, material);

			_lua->pushinteger(L, 0);
			return 1;
		});

		// _lua->add_module_function("RenderPlugin", "destroy_logo", destroy_logo);

		//_objects = MAKE_NEW(_allocator, Objects, _allocator);
		//_free_objects = MAKE_NEW(_allocator, FreeObjects, _allocator);
	}

	void shutdown_game()
	{
		/*
		for (auto &o : *_objects) {
			if (!o.used)
				continue;

			_render_buffer->destroy_buffer(o.ibuffer);
			_render_buffer->destroy_buffer(o.vbuffer);
			_render_buffer->destroy_description(o.vdecl);
			_mesh_api->destroy(o.mesh);
			MAKE_DELETE(_allocator, o.cloth);
			MAKE_DELETE(_allocator, o.vertices);
			MAKE_DELETE(_allocator, o.indices);
		}
		MAKE_DELETE(_allocator, _objects);
		MAKE_DELETE(_allocator, _free_objects);
		*/
		_allocator_api->destroy_plugin_allocator(_allocator_object);
	}

	void* start_reload(GetApiFunction get_engine_api)
	{
		void *state = _allocator.allocate(256);
		char *s = (char *)state;
		if (_object)
		{
			stream::pack<int>(s, 1);
			stream::pack(s, _object->unit);
			stream::pack(s, _object->node_name_id);
			stream::pack(s, _object->material);
		} else
			stream::pack<int>(s, 0);

		destroy_city();

		return state;
	}

	void finish_reload(GetApiFunction get_engine_api, void *state)
	{
		init_api(get_engine_api);

		char *s = (char *)state;
		if (stream::unpack<int>(s))
		{
			auto unit = stream::unpack<Unit *>(s);
			auto node_name_id = stream::unpack<uint32_t>(s);
			auto material = stream::unpack<void *>(s);

			make_city(unit, node_name_id, material);
		}
	}
}

extern "C" {

#ifdef STATIC_LINKING
	void *get_city_plugin_api(unsigned api)
#else
	__declspec(dllexport) void *get_plugin_api(unsigned api)
#endif
{
	if (api == PLUGIN_API_ID) {
		static struct PluginApi api = {0};
		api.setup_game = setup_game;
		api.shutdown_game = shutdown_game;
		api.start_reload = start_reload;
		api.finish_reload = finish_reload;
		return &api;
	}
	return 0;
}
}
