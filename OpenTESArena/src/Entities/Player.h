#ifndef PLAYER_H
#define PLAYER_H

#include "Jolt/Jolt.h"
#include "Jolt/Physics/Character/Character.h"
#include "Jolt/Physics/Character/CharacterVirtual.h"
#include "Jolt/Physics/PhysicsSystem.h"

#include "Camera3D.h"
#include "PrimaryAttributeSet.h"
#include "WeaponAnimation.h"
#include "../Assets/MIFUtils.h"
#include "../World/Coord.h"

class CharacterClassLibrary;
class CollisionChunkManager;
class ExeData;
class Game;
class LevelDefinition;
class LevelInfoDefinition;
class Random;
class VoxelChunkManager;

struct Player
{
	// Distance from player's feet to head.
	static constexpr double HEIGHT = 60.0 / MIFUtils::ARENA_UNITS;

	// Arbitrary values for movement speed.
	static constexpr double DEFAULT_WALK_SPEED = 2.0;

	std::string displayName;
	bool male;
	int raceID;
	int charClassDefID;
	int portraitID;
	Camera3D camera;
	VoxelDouble3 velocity; // @todo: maybe this should come from Jolt
	double maxWalkSpeed; // Eventually a function of 'Speed' attribute
	double friction;
	WeaponAnimation weaponAnimation;
	PrimaryAttributeSet attributes;
	// Other stats...
	JPH::Character *physicsCharacter;
	JPH::CharacterVirtual *physicsCharacterVirtual;
	JPH::CharacterVsCharacterCollisionSimple physicsCharVsCharCollision;

	Player();
	~Player();

	// Gets the Y position of the player's feet.
	double getFeetY() const;

	// Changes the player's velocity based on collision with objects in the world.
	void handleCollision(double dt, const VoxelChunkManager &voxelChunkManager, const CollisionChunkManager &collisionChunkManager, double ceilingScale); // @todo: maybe delete this, or repurpose as a contact listener maybe?

	// Updates the player's position and velocity based on interactions with the world.
	void updatePhysics(double dt, const VoxelChunkManager &voxelChunkManager, const CollisionChunkManager &collisionChunkManager, double ceilingScale); // @todo: this probably would handle climbing mode?

	// Make player with rolled attributes based on race & gender.
	void init(const std::string &displayName, bool male, int raceID, int charClassDefID,
		int portraitID, const CoordDouble3 &position, const Double3 &direction, const Double3 &velocity,
		double maxWalkSpeed, int weaponID, const ExeData &exeData, JPH::PhysicsSystem &physicsSystem, Random &random);

	// Make player with given attributes.
	void init(const std::string &displayName, bool male, int raceID, int charClassDefID, PrimaryAttributeSet &&attributes,
		int portraitID, const CoordDouble3 &position, const Double3 &direction, const Double3 &velocity,
		double maxWalkSpeed, int weaponID, const ExeData &exeData, JPH::PhysicsSystem &physicsSystem);

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

	// Rotates the player's camera based on some change in X (left/right) and Y (up/down).
	void rotate(Degrees dx, Degrees dy, double pitchLimit);

	// Recalculates the player's view so they look at a point.
	void lookAt(const CoordDouble3 &point);

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
