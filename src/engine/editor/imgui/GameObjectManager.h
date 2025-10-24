/**
 * @file    GameObjectManager.h
 * @author  k.danielneozuofeng
 * @date    23/10/2025
 * @brief
 *
 *
 *
 *
 *
 */

#ifndef GAMEOBJECTMANAGER_H
#define GAMEOBJECTMANAGER_H

//
//	Controls and coordinates game object creation
//
class GameObjectManager {
public:
	GameObjectManager(World& world);									// constructor
	void CreateGameObject(const std::string& name = "GameObject");		// creates game object
	void DestroyGameObject(Entity& entity);								// deletes game object
	std::vector<std::pair <Entity, std::string>> GetAllGameObjects();	// obtain active objects

private:
	World& world_;														// reference to ecs world

};

#endif