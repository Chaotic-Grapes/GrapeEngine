/**
 * @file    GameObjectManager.cpp
 * @author  k.danielneozuofeng
 * @date    23/10/2025
 * @brief   
 *
 * 
 * 
 * 
 * 
 */

#include "GameObjectManager.h"
#include "World.h"
#include "EntityManager.h"

GameObjectManager::GameObjectManager(World & world) : world_(world) {};	// reference to ecs world

void GameObjectManager::CreateGameObject(const std::string& name){
	world_.CreateEntity(name);											// reference to world > create game object
}

void GameObjectManager::DestroyGameObject(Entity& entity) {
	world_.GetEntityManager().DestroyEntity(entity);					// reference to world > destroy game object
}

std::vector<std::pair<Entity, std::string>> GameObjectManager::GetAllGameObjects() {
	auto entityIds = world_.GetEntityManager().GetAllEntities();		// returns all existing entity IDs

	std::vector <std::pair<Entity, std::string>> entityP;				
	for (auto id : entityIds) {
		auto entity = world_.GetEntityManager().GetEntity(id);			// returns the Entity object corresponding to the ID
		auto name = world_.GetEntityManager().GetName(id);				// returns the name of the Entity

		entityP.push_back({ entity, name });							// stores Entity ID and name
	}

	return entityP;
}