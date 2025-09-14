#pragma once

#include "Vector2D.h"
#include <glm/vec4.hpp>
#include "Physics.h"
class Renderer;

struct TestCircle {
	float     radius = 40.0f;
	glm::vec4 color = { 0.2f, 0.7f, 1.0f, 1.0f };
	int       segments = 48;
};

class TestScene {
public:
	// Give us the physics system to register our body with, and the window size
	void init(Engine::PhysicsSystem* physics, float winW, float winH);

	// We don't integrate here; PhysicsSystem does that on FixedUpdate.
	void update(float /*dt*/);

	// Submit draw commands
	void render(Renderer& r);

	// Optional tweak at runtime
	void setDamping(float d) { body_.damping = d; }

private:
	Engine::PhysicsSystem* physics_ = nullptr;
	Engine::PhysicsComponent body_; // owned here; physics keeps a pointer
	TestCircle circle_;
};