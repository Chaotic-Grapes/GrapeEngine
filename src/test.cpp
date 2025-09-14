#include "test.h"
#include "../include/graphics/renderer.hpp"
#include <glm/vec2.hpp>

static inline glm::vec2 ToGLM(const Vector2D& v) { return { v.x, v.y }; }

void TestScene::init(Engine::PhysicsSystem* physics, float winW, float winH) {
	physics_ = physics;

	// Start in the middle of the window
	body_.position = { winW * 0.5f, winH * 0.5f, 0.0f };

	// Kick it left; PhysicsSystem will slow it with damping each fixed step
	body_.velocity = { -200.0f, 0.0f, 0.0f };

	// How much "slowdown": 1.0 = no slowdown, 0.0 = stop instantly
	body_.damping = 0.95f; // try 0.98f for a gentler slowdown

	// Register this body with the physics system
	physics_->AddEntity(&body_);
}

void TestScene::update(float /*dt*/) {
	// No per-frame integration here — PhysicsSystem updates body_.position/velocity
}

void TestScene::render(Renderer& r) {
	// Read the simulated position (x,y) and draw the circle there
	Vector2D center(body_.position.x, body_.position.y);
	r.submitCircle(ToGLM(center), circle_.radius, circle_.color, circle_.segments);
}