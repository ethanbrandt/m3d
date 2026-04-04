#pragma once

#include <crossguid/guid.hpp>
#include <iostream>
using ComponentID = xg::Guid;
using EntityID = xg::Guid;

class ECS;

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
	virtual void on_initialize() = 0;

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

	virtual void start(EntityID entityID) = 0;
	virtual void update(EntityID entityID, float deltaTime) = 0;
	virtual void on_destroy(EntityID entityID) = 0;
};
