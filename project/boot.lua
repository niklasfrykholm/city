local Application = stingray.Application
local World = stingray.World
local Unit = stingray.Unit
local ShadingEnvironment = stingray.ShadingEnvironment
local City = stingray.City
local Camera = stingray.Camera
local Mouse = stingray.Mouse
local Keyboard = stingray.Keyboard
local Matrix4x4 = stingray.Matrix4x4
local Vector3 = stingray.Vector3
local Quaternion = stingray.Quaternion

self = {}

camera = {
	free_flight_speed = 20.0,
	rotation_speed = 1.0,

	init = function(self, world, camera_unit)
		self.world = world
		self.camera_unit = camera_unit
	end,
	update = function(self, dt)
		local camera = Unit.camera(self.camera_unit, "camera")
		local q = Camera.local_rotation(camera, self.camera_unit)
		local p = Camera.local_position(camera, self.camera_unit)

		local input = self:_get_input()
		q = self:_compute_rotation(q, input, dt)
		p = self:_compute_translation(p, q, input, dt)
		local pose = Matrix4x4.from_quaternion(q)
		Matrix4x4.set_translation(pose, p)
		Camera.set_local_pose(camera, self.camera_unit, pose)
	end,

	_get_input = function(self)
		local input = {}
		input.pan = Mouse.axis(Mouse.axis_id("mouse"))
		input.move = Vector3 (
			Keyboard.button(Keyboard.button_id("d")) - Keyboard.button(Keyboard.button_id("a")),
			Keyboard.button(Keyboard.button_id("w")) - Keyboard.button(Keyboard.button_id("s")),
			0
		)
		return input
	end,
	_compute_rotation = function(self, qo, input, dt)
		local cm = Matrix4x4.from_quaternion(qo)

		local q1 = Quaternion( Vector3(0,0,1), -Vector3.x(input.pan) * self.rotation_speed * dt )
		local q2 = Quaternion( Matrix4x4.x(cm), -Vector3.y(input.pan) * self.rotation_speed * dt )
		local q = Quaternion.multiply(q1, q2)
		local qres = Quaternion.multiply(q, qo)
		return qres
	end,
	_compute_translation = function(self, p, q, input, dt)
		local pose = Matrix4x4.from_quaternion(q)
		local local_move = input.move * dt * self.free_flight_speed
		local move = Matrix4x4.transform(pose, local_move)
		return p + move
	end
}

function init()
	Application.set_plugin_hot_reload_directory("C:\\Work\\city\\plugin\\build\\dlls")

	self.world = Application.new_world()
	self.viewport = Application.create_viewport(self.world, "default")
	self.shading_environment = World.create_shading_environment(self.world, "core/stingray_renderer/environments/midday/midday")
	self.camera_unit = World.spawn_unit(self.world, "core/units/camera")
	self.sky = World.spawn_unit(self.world, "core/editor_slave/units/skydome/skydome")
	self.city_unit = World.spawn_unit(self.world, "core/units/camera")
	City.make_city(self.city_unit, "rp_root", "city")

	camera:init(self.world, self.camera_unit)
end

function update(dt)
	camera:update(dt)
	self.world:update(dt)
	if Keyboard.button(Keyboard.button_id("esc")) > 0 then
		Application.quit()
	end
end

function render()
	ShadingEnvironment.blend(self.shading_environment, {"default", 1})
	ShadingEnvironment.apply(self.shading_environment)
	local camera = Unit.camera(self.camera_unit, "camera")
	Application.render_world(self.world, camera, self.viewport, self.shading_environment)
end

function shutdown()
	City.destroy_city()
	Application.destroy_viewport(self.world, self.viewport)
	World.destroy_shading_environment(self.world, self.shading_environment)
	Application.release_world(self.world)
end
