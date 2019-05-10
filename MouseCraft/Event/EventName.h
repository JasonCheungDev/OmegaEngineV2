#pragma once

// Enumerated type used for indexing and referencing events
enum EventName {
	// ENUM					| DATA TYPE			| INCLUDE FILE			| NOTES
	PLAY_SONG,			//	| TrackParams		| Sound/TrackParams.h	| Used to trigger a BGM track to play
	PLAY_SOUND,			//	| SoundParams		| Sound/SoundParams.h	| Used to trigger a Sound Effect
	COMPONENT_UPDATE,	//	| float				|						| Delta time 
	COMPONENT_F_UPDATE,	//	| <float,int>		| 						| Pair of overall delta time in seconds and number of steps (if steps is greater than 1 it means the engine is lagging).
	COMPONENT_REMOVED,	//	| Component*		| Core/Component.h		| Component that is about to be destroyed
	COMPONENT_ADDED,	//	| Component*		| Core/Component.h		| Component that was just created.
	ENTITY_CREATED,		//	| Entity*			| Core/Entity.h			| 
	ENTITY_DESTROYED,	//	| Entity*			| Core/Entity.h			| 
	ENTITY_ENABLE,		//	| Entity*			| Core/Entity.h			| Use entity->GetEnabled() to retrieve the enable status. 
	ENTITY_MOVE,		//  | <Entity*,...>		| Core/Entity.h			| First argument is child, second is parent. Parent maybe nullptr. Child is being moved to parent.
	INPUT_RAW,			//	|					|						| DO NOT USE
	INPUT_AXIS,			//	| AxisEvent			| Input/InputSystem.h	| Controller stick (1D)
	INPUT_AXIS_2D,		//	| Axis2DEvent		| Input/InputSystem.h	| Controller stick (2D)
	INPUT_BUTTON,		//	| ButtonEvent		| Input/InputSystem.h	| Controller button
	INPUT_MOUSE_CLICK,	//	| MouseButtonEvent	| Input/InputSystem.h	| Left and right click only
	INPUT_MOUSE_MOVE,	//	| glm::ivec2		| <glm/glm.hpp>			| 
	GAMEOVER,			//	| GameOverParams	| GameManager.h			| 
};
