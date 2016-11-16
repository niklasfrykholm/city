#include "engine_plugin_api/plugin_api.h"
#include "plugin_foundation/id_string.h"
#include "plugin_foundation/array.h"
#include "plugin_foundation/allocator.h"
#include "plugin_foundation/matrix4x4.h"
#include "plugin_foundation/vector3.h"
#include "plugin_foundation/random.h"
#include "plugin_foundation/stream.h"

#include <string.h>

using namespace stingray_plugin_foundation;

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

	typedef uint32_t Index;

	struct City {
		CApiUnit *unit;
		uint32_t node_name_id;
		void *material;

		uint32_t vbuffer;
		uint32_t ibuffer;
		uint32_t vdecl;
		uint32_t mesh;
		Array<Vertex> *vertices;
		Array<uint32_t> *indices;
	};
	City *_city = nullptr;

	Vertex vertex(const Vector3 &p, const Vector3 &n)
	{
		Vertex v = { p, n };
		return v;
	}

	void draw_aabb(Array<Vertex> &vb, Array<Index> &ib, const Vector3 &min, const Vector3 &max)
	{
		auto add_face = [&](const Vector3 &n) {
			auto c = (min + max) / 2.0f + n * (max - min) / 2.0f;
			auto e1 = n.x ? vector3(0, 1, 0) : vector3(1, 0, 0);
			auto e2 = cross(n, e1);
			auto o1 = e1 * (max - min) / 2.0f;
			auto o2 = e2 * (max - min) / 2.0f;

			auto v0 = vb.size();
			vb.push_back(vertex(c - o1 - o2, n));
			vb.push_back(vertex(c - o1 + o2, n));
			vb.push_back(vertex(c + o1 + o2, n));
			vb.push_back(vertex(c + o1 - o2, n));

			ib.push_back(v0 + 2);
			ib.push_back(v0 + 1);
			ib.push_back(v0 + 0);
			ib.push_back(v0 + 0);
			ib.push_back(v0 + 3);
			ib.push_back(v0 + 2);
		};

		add_face(vector3(-1, 0, 0));
		add_face(vector3(+1, 0, 0));
		add_face(vector3(0, -1, 0));
		add_face(vector3(0, +1, 0));
		add_face(vector3(0, 0, -1));
		add_face(vector3(0, 0, +1));
	}

	struct AABB { float xmin, xmax, ymin, ymax; };
	AABB aabb(float xmin, float xmax, float ymin, float ymax)
	{
		AABB x = { xmin, xmax, ymin, ymax };
		return x;
	}

	Block *make_block(unsigned seed, float xmin, float xmax, float ymin, float ymax)
	{
		Random r(seed);

		Array<AABB> lots(_allocator);
		Array<AABB> queue(_allocator);

		queue.push_back(aabb(xmin, xmax, ymin, ymax));
		while (queue.size() > 0) {
			AABB item = queue.back();
			queue.pop_back();

			if ((item.xmax - item.xmin) < 20 || (item.ymax - item.ymin) < 20) {
				lots.push_back(item);
			} else {
				if ((item.xmax - item.xmin) > (item.ymax - item.ymin)) {
					// Split x
					float rw = (item.xmax - item.xmin) / 10.0f / 2.0f;
					if (rw > 25.0f) rw = 25.0f;
					float x = r(item.xmin + rw * 2, item.xmax - rw * 2);
					queue.push_back(aabb(item.xmin, x - rw, item.ymin, item.ymax));
					queue.push_back(aabb(x + rw, item.xmax, item.ymin, item.ymax));
				} else {
					float rw = (item.ymax - item.ymin) / 10.0f / 2.0f;
					if (rw > 25.0f) rw = 25.0f;
					float y = r(item.ymin + rw * 2, item.ymax - rw * 2);
					queue.push_back(aabb(item.xmin, item.xmax, item.ymin, y - rw));
					queue.push_back(aabb(item.xmin, item.xmax, y + rw, item.ymax));
				}
			}
		}


		int n = lots.size();;
		Block &b = *(Block *)_allocator.allocate(sizeof(Block) + n * sizeof(Building));
		b.xmin = xmin;
		b.xmax = xmax;
		b.ymin = ymin;
		b.ymax = ymax;
		b.max_height = 100;
		b.num_buildings = n;
		b.buildings = (Building *)(&b+1);

		for (int i=0; i<n; ++i) {
			AABB lot = lots[i];
			Building &bu = b.buildings[i];
			bu.xmin = lot.xmin;
			bu.xmax = lot.xmax;
			bu.ymin = lot.ymin;
			bu.ymax = lot.ymax;
			int floors = r(1,3) * r(1,3) * r(1,3);
			bu.height = floors * 3.50f;
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

	void make_city(CApiUnit *unit, uint32_t node_name_id, void *material)
	{
		XENSURE(!_city);
		_city = MAKE_NEW(_allocator, City);
		City &o = *_city;

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
		ib_view.stride = 4;

		o.indices = MAKE_NEW(_allocator, Array<uint32_t>, _allocator);
		o.vertices = MAKE_NEW(_allocator, Array<Vertex>, _allocator);

		Block *block = make_block(0, -2000.0f, 2000.0f, -2000.0f, 2000.0f);
		render_block(*block, *o.vertices, *o.indices);

		o.vbuffer = _render_buffer->create_buffer(o.vertices->size() * vb_view.stride, RB_Validity::RB_VALIDITY_STATIC, RB_View::RB_VERTEX_BUFFER_VIEW, &vb_view, o.vertices->begin());
		o.ibuffer = _render_buffer->create_buffer(o.indices->size() * ib_view.stride, RB_Validity::RB_VALIDITY_STATIC, RB_View::RB_INDEX_BUFFER_VIEW, &ib_view, o.indices->begin());

		o.mesh = _mesh_api->create(unit, node_name_id, node_name_id, MO_Flags::MO_VIEWPORT_VISIBLE_FLAG | MO_Flags::MO_SHADOW_CASTER_FLAG);

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

		_allocator.deallocate(block);
		MAKE_DELETE(_allocator, o.indices);
		MAKE_DELETE(_allocator, o.vertices);
		o.indices = nullptr;
		o.vertices = nullptr;
	}

	void destroy_city()
	{
		if (!_city) return;

		City &o = *_city;

		_render_buffer->destroy_buffer(o.ibuffer);
		_render_buffer->destroy_buffer(o.vbuffer);
		_render_buffer->destroy_description(o.vdecl);
		_mesh_api->destroy(o.mesh);

		MAKE_DELETE(_allocator, o.vertices);
		MAKE_DELETE(_allocator, o.indices);
		MAKE_DELETE(_allocator, _city);
		_city = nullptr;
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
		_lua->add_module_function("City", "destroy_city", [](lua_State *L)
		{
			destroy_city();
			return 0;
		});
	}

	void shutdown_game()
	{
		destroy_city();
		_allocator_api->destroy_plugin_allocator(_allocator_object);
	}

	void* start_reload(GetApiFunction get_engine_api)
	{
		void *state = _allocator.allocate(256);
		char *s = (char *)state;
		if (_city)
		{
			stream::pack<int>(s, 1);
			stream::pack(s, _city->unit);
			stream::pack(s, _city->node_name_id);
			stream::pack(s, _city->material);
		} else
			stream::pack<int>(s, 0);

		stream::pack(s, _allocator_object);

		destroy_city();

		return state;
	}

	void finish_reload(GetApiFunction get_engine_api, void *state)
	{
		init_api(get_engine_api);
		_allocator_api->destroy_plugin_allocator(_allocator_object);

		char *s = (char *)state;
		if (stream::unpack<int>(s))
		{
			auto unit = stream::unpack<CApiUnit *>(s);
			auto node_name_id = stream::unpack<uint32_t>(s);
			auto material = stream::unpack<void *>(s);
			_allocator_object = stream::unpack<AllocatorObject *>(s);
			_allocator = ApiAllocator(_allocator_api, _allocator_object);

			make_city(unit, node_name_id, material);
		}
		else {
			_allocator_object = stream::unpack<AllocatorObject *>(s);
			_allocator = ApiAllocator(_allocator_api, _allocator_object);
		}

		_allocator.deallocate(state);
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
