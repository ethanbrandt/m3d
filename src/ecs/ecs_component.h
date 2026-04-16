#pragma once

#include <crossguid/guid.hpp>
#include <iostream>
using ComponentID = xg::Guid;
using EntityID = xg::Guid;

class ECS;

enum ComponentType
{
	COLLIDER,
	RIGID_BODY,
	MESH_RENDERER,
	SCRIPT,
	AUDIO_PLAYER	
};

class Component
{
private:
	ComponentID id;
	EntityID entityID;
	bool initialized = false;

	bool initialize(EntityID _entityID)
	{
		if (initialized)
			return false;
		initialized = true;
		id = xg::newGuid();
		entityID = _entityID;
		on_initialize();
		return true;
	}

	// ECS is friend to allow only ECS to initialize Component
	friend class ECS;

protected:
	ComponentType type;

	virtual void on_initialize() = 0;

	bool get_initialized()
	{
		return initialized;
	}

public:
	virtual ~Component() = default;

	ComponentID get_id() const noexcept
	{
		return id;
	}

	EntityID get_entity_id() const noexcept
	{
		return entityID;
	}

	ComponentType get_type() const noexcept
	{
		return type;
	}

	virtual void start() = 0;
	virtual void update(float deltaTime) = 0;
	virtual void on_destroy() = 0;
};
