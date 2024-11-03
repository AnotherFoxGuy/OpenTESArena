#ifndef PLAYER_H
#define PLAYER_H

#include "Jolt/Jolt.h"
#include "Jolt/Physics/Character/Character.h"
#include "Jolt/Physics/Character/CharacterVirtual.h"
#include "Jolt/Physics/PhysicsSystem.h"

#include "PrimaryAttributeSet.h"
#include "WeaponAnimation.h"
#include "../Assets/MIFUtils.h"
#include "../Math/MathUtils.h"
#include "../World/Coord.h"

class CharacterClassLibrary;
class CollisionChunkManager;
class ExeData;
class Game;
class LevelDefinition;
class LevelInfoDefinition;
class Random;
class VoxelChunkManager;

namespace PlayerConstants
{
	constexpr double HEIGHT = 60.0 / MIFUtils::ARENA_UNITS; // Distance from feet to head.
	constexpr double DEFAULT_WALK_SPEED = 2.0;
	constexpr double FRICTION = 3.0; // Friction for slowing the player down on ground.
	constexpr double COLLIDER_RADIUS = 0.15; // Radius around the player they will collide at.
	constexpr double COLLIDER_CYLINDER_HALF_HEIGHT = (HEIGHT / 2.0) - COLLIDER_RADIUS;
	constexpr double STEPPING_HEIGHT = 0.25; // Stairsteps delta (used by Jolt CharacterVirtual::ExtendedUpdate()).
	constexpr double JUMP_VELOCITY = 3.0; // Instantaneous change in Y velocity when jumping.
}

struct Player
{
	std::string displayName;
	bool male;
	int raceID;
	int charClassDefID;
	int portraitID;
	CoordDouble3 position; // Eye position @todo: what if it was just the Jolt collider position and a Vec3 forward? And other vectors were calculated
	Double3 forward; // Camera direction
	Double3 right;
	Double3 up;
	// @todo: polar coordinates (XYZ angles)
	VoxelDouble3 velocity; // @todo: maybe this should come from Jolt collider
	double maxWalkSpeed; // Eventually a function of 'Speed' attribute
	WeaponAnimation weaponAnimation;
	PrimaryAttributeSet attributes;
	JPH::Character *physicsCharacter;
	JPH::CharacterVirtual *physicsCharacterVirtual;
	JPH::CharacterVsCharacterCollisionSimple physicsCharVsCharCollision;

	Player();
	~Player();

	// Gets the Y position of the player's feet.
	double getFeetY() const;

	// Make player with rolled attributes based on race & gender.
	void init(const std::string &displayName, bool male, int raceID, int charClassDefID,
		int portraitID, const CoordDouble3 &position, const Double3 &direction, const Double3 &velocity,
		double maxWalkSpeed, int weaponID, const ExeData &exeData, JPH::PhysicsSystem &physicsSystem, Random &random);

	// Make player with given attributes.
	void init(const std::string &displayName, bool male, int raceID, int charClassDefID, PrimaryAttributeSet &&attributes,
		int portraitID, const CoordDouble3 &position, const Double3 &direction, const Double3 &velocity,
		double maxWalkSpeed, int weaponID, const ExeData &exeData, JPH::PhysicsSystem &physicsSystem);

	void initCamera(const CoordDouble3 &coord, const Double3 &forward);

	// Initializes a random player for testing.
	void initRandom(const CharacterClassLibrary &charClassLibrary, const ExeData &exeData, JPH::PhysicsSystem &physicsSystem, Random &random);

	void freePhysicsBody(JPH::PhysicsSystem &physicsSystem);

	std::string getFirstName() const; // @todo: maybe replace this with a PlayerName struct that has fullName and firstName char[]

	// Gets the bird's eye view of the player's direction (in the XZ plane).
	Double2 getGroundDirection() const;

	// Gets the strength of the player's jump (i.e., instantaneous change in Y velocity).
	double getJumpMagnitude() const;

	bool onGround() const;

	// Teleports the player to a point.
	void teleport(const CoordDouble3 &position); // @todo: probably takes PhysicsSystem&

	// Pitches and yaws relative to global up vector.
	void rotateX(Degrees deltaX);
	void rotateY(Degrees deltaY, Degrees pitchLimit);

	// Recalculates the player's view so they look at a point. The global up vector is used when generating the new 3D frame,
	// so don't give a point directly above or below the camera.
	void lookAt(const CoordDouble3 &targetCoord);

	// Intended for stopping after level transitions.
	void setVelocityToZero(); // @todo: probably takes PhysicsSystem&

	// Flattens direction vector to the horizon (used when switching classic/modern camera mode).
	void setDirectionToHorizon();

	// Changes the velocity (as a force) given a normalized direction, magnitude, and delta time.
	void accelerate(const Double3 &direction, double magnitude, double dt); // @todo: this will give CharacterVirtual a force probably?

	// Changes the velocity instantly. Intended for instantaneous acceleration like jumping.
	void accelerateInstant(const Double3 &direction, double magnitude); // @todo: CharacterVirtual should treat this like a jump

	void prePhysicsStep(double dt, Game &game);
	void postPhysicsStep(Game &game);
};

#endif
