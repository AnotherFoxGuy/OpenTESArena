#include <algorithm>
#include <cmath>
#include <vector>

#include "Jolt/Jolt.h"
#include "Jolt/Physics/Body/Body.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"

#include "CharacterClassDefinition.h"
#include "CharacterClassLibrary.h"
#include "Player.h"
#include "PrimaryAttributeName.h"
#include "../Collision/CollisionChunk.h"
#include "../Collision/CollisionChunkManager.h"
#include "../Collision/Physics.h"
#include "../Collision/PhysicsLayer.h"
#include "../Game/CardinalDirection.h"
#include "../Game/Game.h"
#include "../Game/GameState.h"
#include "../Game/Options.h"
#include "../Math/Constants.h"
#include "../Math/Quaternion.h"
#include "../Math/Random.h"
#include "../Voxels/VoxelChunkManager.h"

#include "components/debug/Debug.h"
#include "components/utilities/Buffer.h"
#include "components/utilities/String.h"

namespace // @todo: could be in a PlayerUtils instead
{
	constexpr double COLLIDER_RADIUS = 0.15; // Radius around the player they will collide at.
	constexpr double COLLIDER_CYLINDER_HALF_HEIGHT = (Player::HEIGHT / 2.0) - COLLIDER_RADIUS;
	constexpr double STEPPING_HEIGHT = 0.25; // Allowed change in height for stepping on stairs. @todo: Jolt has CharacterVirtual::WalkStairs()
	constexpr double JUMP_VELOCITY = 3.0; // Instantaneous change in Y velocity when jumping.

	// Friction for slowing the player down on ground.
	constexpr double FRICTION = 3.0;

	bool TryCreatePhysicsCharacters(JPH::PhysicsSystem &physicsSystem, JPH::Character **outCharacter, JPH::CharacterVirtual **outCharacterVirtual,
		JPH::CharacterVsCharacterCollisionSimple *outCharVsCharCollision)
	{
		if (*outCharacter != nullptr)
		{
			const JPH::BodyID &existingBodyID = (*outCharacter)->GetBodyID();
			if (!existingBodyID.IsInvalid())
			{
				JPH::BodyInterface &bodyInterface = physicsSystem.GetBodyInterface();
				bodyInterface.RemoveBody(existingBodyID);
				bodyInterface.DestroyBody(existingBodyID);
			}
		}

		// Create same capsule for physical and virtual collider.
		constexpr double capsuleRadius = COLLIDER_RADIUS;
		constexpr double cylinderHalfHeight = (Player::HEIGHT / 2.0) - capsuleRadius;
		static_assert(cylinderHalfHeight >= 0.0);
		static_assert(MathUtils::almostEqual((capsuleRadius * 2.0) + (cylinderHalfHeight * 2.0), Player::HEIGHT));

		JPH::CapsuleShapeSettings capsuleShapeSettings(cylinderHalfHeight, capsuleRadius);
		capsuleShapeSettings.SetEmbedded(); // Marked embedded to prevent it from being freed when its ref count reaches 0.
		// @todo: make sure this ^ isn't leaking when we remove/destroy the body

		JPH::ShapeSettings::ShapeResult capsuleShapeResult = capsuleShapeSettings.Create();
		if (capsuleShapeResult.HasError())
		{
			DebugLogError("Couldn't create Jolt capsule collider settings: " + std::string(capsuleShapeResult.GetError().c_str()));
			return false;
		}

		constexpr float collisionTolerance = 0.05f; // from Jolt example
		constexpr float characterRadius = 0.5f; // Not sure what this is yet
		constexpr float characterRadiusStanding = characterRadius; // Not sure what this is yet
		constexpr float maxSlopeAngle = ConstantsF::DegToRad(5.0f); // Game world doesn't have slopes, so this can be very small.
		constexpr float maxStrength = 100.0f; // from Jolt example
		constexpr float characterPadding = 0.02f; // from Jolt example
		constexpr float penetrationRecoverySpeed = 1.0f; // from Jolt example
		constexpr float predictiveContactDistance = 0.1f; // from Jolt example

		// Jolt says "pair a CharacterVirtual with a Character that has no gravity and moves with the CharacterVirtual so other objects collide with it".
		// I just need a capsule that runs into things, jumps, and steps on stairs.
		JPH::CharacterVirtualSettings characterVirtualSettings;
		characterVirtualSettings.SetEmbedded();
		characterVirtualSettings.mMass = 1.0f;
		characterVirtualSettings.mMaxSlopeAngle = maxSlopeAngle;
		characterVirtualSettings.mMaxStrength = maxStrength;
		characterVirtualSettings.mShape = capsuleShapeResult.Get();
		characterVirtualSettings.mBackFaceMode = JPH::EBackFaceMode::CollideWithBackFaces;
		characterVirtualSettings.mCharacterPadding = characterPadding;
		characterVirtualSettings.mPenetrationRecoverySpeed = penetrationRecoverySpeed;
		characterVirtualSettings.mPredictiveContactDistance = predictiveContactDistance;
		characterVirtualSettings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -characterRadiusStanding);
		characterVirtualSettings.mEnhancedInternalEdgeRemoval = false;
		characterVirtualSettings.mInnerBodyShape = nullptr;
		characterVirtualSettings.mInnerBodyLayer = PhysicsLayers::MOVING;
		
		JPH::CharacterSettings characterSettings;
		characterSettings.SetEmbedded();
		characterSettings.mFriction = 0.3f;
		characterSettings.mGravityFactor = 0.0f; // Uses zero gravity when paired w/ CharacterVirtual
		characterSettings.mShape = capsuleShapeResult.Get();
		characterSettings.mLayer = PhysicsLayers::MOVING;
		characterSettings.mMaxSlopeAngle = maxSlopeAngle;
		characterSettings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), characterRadius);
		
		constexpr uint64_t characterVirtualUserData = 0;
		*outCharacterVirtual = new JPH::CharacterVirtual(&characterVirtualSettings, JPH::Vec3Arg::sZero(), JPH::QuatArg::sIdentity(), characterVirtualUserData, &physicsSystem);
		(*outCharacterVirtual)->SetCharacterVsCharacterCollision(outCharVsCharCollision);
		outCharVsCharCollision->Add(*outCharacterVirtual);
		(*outCharacterVirtual)->SetListener(nullptr); // @todo
		DebugLogError("\nNeed characterVirtual's contact listeners\n");
		
		constexpr uint64_t characterUserData = 0;
		*outCharacter = new JPH::Character(&characterSettings, JPH::Vec3Arg::sZero(), JPH::QuatArg::sIdentity(), characterUserData, &physicsSystem);
		(*outCharacter)->AddToPhysicsSystem(JPH::EActivation::Activate);

		return true;
	}

	void SetPhysicsPosition(JPH::Character *character, JPH::CharacterVirtual *charVirtual, const WorldDouble3 &position)
	{
		const JPH::RVec3Arg physicsPosition(
			static_cast<float>(position.x),
			static_cast<float>(position.y),
			static_cast<float>(position.z));
		character->SetPosition(physicsPosition);
		charVirtual->SetPosition(physicsPosition);
	}

	Radians SafeDegreesToRadians(Degrees degrees)
	{
		const Radians radians = Constants::DegToRad(degrees);
		if (!std::isfinite(radians))
		{
			return 0.0;
		}

		return radians;
	}
}

Player::Player()
{
	this->male = false;
	this->raceID = -1;
	this->charClassDefID = -1;
	this->portraitID = -1;
	this->initCamera(CoordDouble3(), -Double3::UnitX); // Avoids audio listener issues w/ uninitialized player.
	this->maxWalkSpeed = 0.0;
	this->friction = 0.0;	
	this->physicsCharacter = nullptr;
	this->physicsCharacterVirtual = nullptr;
}

Player::~Player()
{
	DebugAssert(this->physicsCharacter == nullptr);
	DebugAssert(this->physicsCharacterVirtual == nullptr);
}

void Player::init(const std::string &displayName, bool male, int raceID, int charClassDefID,
	int portraitID, const CoordDouble3 &position, const Double3 &direction, const Double3 &velocity,
	double maxWalkSpeed, int weaponID, const ExeData &exeData, JPH::PhysicsSystem &physicsSystem, Random &random)
{
	this->displayName = displayName;
	this->male = male;
	this->raceID = raceID;
	this->charClassDefID = charClassDefID;
	this->portraitID = portraitID;
	this->initCamera(position, direction);
	this->velocity = velocity;
	this->maxWalkSpeed = maxWalkSpeed;
	this->friction = FRICTION;
	this->weaponAnimation.init(weaponID, exeData);
	this->attributes.init(raceID, male, random);
	
	if (!TryCreatePhysicsCharacters(physicsSystem, &this->physicsCharacter, &this->physicsCharacterVirtual, &this->physicsCharVsCharCollision))
	{
		DebugCrash("Couldn't create player physics collider.");
	}

	SetPhysicsPosition(this->physicsCharacter, this->physicsCharacterVirtual, VoxelUtils::coordToWorldPoint(this->position));
}

void Player::init(const std::string &displayName, bool male, int raceID, int charClassDefID,
	PrimaryAttributeSet &&attributes, int portraitID, const CoordDouble3 &position, const Double3 &direction,
	const Double3 &velocity, double maxWalkSpeed, int weaponID, const ExeData &exeData, JPH::PhysicsSystem &physicsSystem)
{
	this->displayName = displayName;
	this->male = male;
	this->raceID = raceID;
	this->charClassDefID = charClassDefID;
	this->portraitID = portraitID;
	this->initCamera(position, direction);
	this->velocity = velocity;
	this->maxWalkSpeed = maxWalkSpeed;
	this->friction = FRICTION;
	this->weaponAnimation.init(weaponID, exeData);
	this->attributes = std::move(attributes);
	
	if (!TryCreatePhysicsCharacters(physicsSystem, &this->physicsCharacter, &this->physicsCharacterVirtual, &this->physicsCharVsCharCollision))
	{
		DebugCrash("Couldn't create player physics collider.");
	}

	SetPhysicsPosition(this->physicsCharacter, this->physicsCharacterVirtual, VoxelUtils::coordToWorldPoint(this->position));
}

void Player::initCamera(const CoordDouble3 &coord, const Double3 &forward)
{
	this->position = coord;
	this->forward = forward;
	this->right = this->forward.cross(Double3::UnitY).normalized();
	this->up = this->right.cross(this->forward).normalized();
}

void Player::initRandom(const CharacterClassLibrary &charClassLibrary, const ExeData &exeData, JPH::PhysicsSystem &physicsSystem, Random &random)
{
	this->displayName = "Player";
	this->male = random.next(2) == 0;
	this->raceID = random.next(8);
	this->charClassDefID = random.next(charClassLibrary.getDefinitionCount());
	this->portraitID = random.next(10);

	const CoordDouble3 position(ChunkInt2::Zero, VoxelDouble3::Zero);
	const Double3 direction(CardinalDirection::North.x, 0.0, CardinalDirection::North.y);
	this->initCamera(position, direction);
	this->velocity = Double3::Zero;
	this->maxWalkSpeed = Player::DEFAULT_WALK_SPEED;
	this->friction = FRICTION;

	const CharacterClassDefinition &charClassDef = charClassLibrary.getDefinition(this->charClassDefID);
	const int weaponID = [&random, &charClassDef]()
	{
		// Generate weapons available for this class and pick a random one.
		const int allowedWeaponCount = charClassDef.getAllowedWeaponCount();
		Buffer<int> weapons(allowedWeaponCount + 1);
		for (int i = 0; i < allowedWeaponCount; i++)
		{
			weapons.set(i, charClassDef.getAllowedWeapon(i));
		}

		// Add fists.
		weapons.set(allowedWeaponCount, -1);

		const int randIndex = random.next(weapons.getCount());
		return weapons.get(randIndex);
	}();

	this->weaponAnimation.init(weaponID, exeData);
	this->attributes.init(this->raceID, this->male, random);
	
	if (!TryCreatePhysicsCharacters(physicsSystem, &this->physicsCharacter, &this->physicsCharacterVirtual, &this->physicsCharVsCharCollision))
	{
		DebugCrash("Couldn't create player physics collider.");
	}

	SetPhysicsPosition(this->physicsCharacter, this->physicsCharacterVirtual, VoxelUtils::coordToWorldPoint(this->position));
}

void Player::freePhysicsBody(JPH::PhysicsSystem &physicsSystem)
{
	if (this->physicsCharacter != nullptr)
	{
		this->physicsCharacter->Release();
		this->physicsCharacter = nullptr;
	}

	if (this->physicsCharacterVirtual != nullptr)
	{
		this->physicsCharacterVirtual->Release();
		this->physicsCharacterVirtual = nullptr;
	}
}

std::string Player::getFirstName() const
{
	Buffer<std::string> nameTokens = String::split(this->displayName);
	return nameTokens[0];
}

Double2 Player::getGroundDirection() const
{
	return Double2(this->forward.x, this->forward.z).normalized();
}

double Player::getJumpMagnitude() const
{
	return JUMP_VELOCITY;
}

double Player::getFeetY() const
{
	const double cameraY = this->position.point.y;
	return cameraY - Player::HEIGHT;
}

bool Player::onGround() const
{
	//return this->physicsCharacter->IsSupported(); // @todo: not sure which is better, maybe virtual is for steps?
	return this->physicsCharacterVirtual->IsSupported();
}

void Player::teleport(const CoordDouble3 &position)
{
	this->position = position;
}

void Player::rotateX(Degrees deltaX)
{
	DebugAssert(std::isfinite(this->forward.length()));
	const Radians deltaAsRadians = SafeDegreesToRadians(deltaX);
	const Quaternion quat = Quaternion::fromAxisAngle(-Double3::UnitY, deltaAsRadians) * Quaternion(this->forward, 0.0);
	this->forward = Double3(quat.x, quat.y, quat.z).normalized();
	this->right = this->forward.cross(Double3::UnitY).normalized();
	this->up = this->right.cross(this->forward).normalized();
}

void Player::rotateY(Degrees deltaY, Degrees pitchLimit)
{
	DebugAssert(std::isfinite(this->forward.length()));
	DebugAssert(pitchLimit >= 0.0);
	DebugAssert(pitchLimit < 90.0);

	const Radians deltaAsRadians = SafeDegreesToRadians(deltaY);
	const Radians currentAngle = std::acos(this->forward.normalized().y);
	const Radians requestedAngle = currentAngle - deltaAsRadians;

	// Clamp to avoid breaking cross product.
	const Radians maxAngle = Constants::DegToRad(90.0 - pitchLimit);
	const Radians minAngle = Constants::DegToRad(90.0 + pitchLimit);
	const Radians actualDeltaAngle = (requestedAngle > minAngle) ? (currentAngle - minAngle) : ((requestedAngle < maxAngle) ? (currentAngle - maxAngle) : deltaAsRadians);

	const Quaternion quat = Quaternion::fromAxisAngle(this->right, actualDeltaAngle) * Quaternion(this->forward, 0.0);
	this->forward = Double3(quat.x, quat.y, quat.z).normalized();
	this->right = this->forward.cross(Double3::UnitY).normalized();
	this->up = this->right.cross(this->forward).normalized();
}

void Player::lookAt(const CoordDouble3 &targetCoord)
{
	const Double3 newForward = (targetCoord - this->position).normalized();
	const Double3 newRight = newForward.cross(Double3::UnitY).normalized();
	const Double3 newUp = newRight.cross(newForward).normalized();

	if (std::isfinite(newUp.length()))
	{
		this->forward = newForward;
		this->right = newRight;
		this->up = newUp;
	}
}

void Player::setVelocityToZero()
{
	this->velocity = Double3::Zero;
}

void Player::setDirectionToHorizon()
{
	const CoordDouble3 &coord = this->position;
	const WorldDouble2 groundDirection = this->getGroundDirection();
	const VoxelDouble3 lookAtPoint = coord.point + VoxelDouble3(groundDirection.x, 0.0, groundDirection.y);
	const CoordDouble3 lookAtCoord(coord.chunk, lookAtPoint);
	this->lookAt(lookAtCoord);
}

void Player::accelerate(const Double3 &direction, double magnitude, double dt)
{
	DebugAssert(dt >= 0.0);
	DebugAssert(magnitude >= 0.0);
	DebugAssert(std::isfinite(magnitude));
	DebugAssert(direction.isNormalized());

	// Simple Euler integration for updating velocity.
	Double3 newVelocity = this->velocity + (direction * (magnitude * dt));

	if (std::isfinite(newVelocity.length()))
	{
		this->velocity = newVelocity;
	}

	// Don't let the horizontal velocity be greater than the max speed for the
	// current movement state (i.e., walking/running).
	double maxSpeed = this->maxWalkSpeed;
	Double2 velocityXZ(this->velocity.x, this->velocity.z);
	if (velocityXZ.length() > maxSpeed)
	{
		velocityXZ = velocityXZ.normalized() * maxSpeed;
	}

	// If the velocity is near zero, set it to zero. This fixes a problem where
	// the velocity could remain at a tiny magnitude and never reach zero.
	if (this->velocity.length() < 0.001)
	{
		this->velocity = Double3::Zero;
	}
}

void Player::accelerateInstant(const Double3 &direction, double magnitude)
{
	DebugAssert(direction.isNormalized());

	const Double3 additiveVelocity = direction * magnitude;

	if (std::isfinite(additiveVelocity.length()))
	{
		this->velocity = this->velocity + additiveVelocity;
	}
}

void Player::prePhysicsStep(double dt, Game &game)
{
	if (game.options.getMisc_GhostMode())
	{
		this->setVelocityToZero(); // Prevent leftover momentum when switching cheat modes.
		// @todo: maybe turn physicsCharacter and virtual off and keep them glued to player arbitrarily? ghost mode is literally "disable jolt"
		return;
	}

	const SceneManager &sceneManager = game.sceneManager;
	const VoxelChunkManager &voxelChunkManager = sceneManager.voxelChunkManager;
	const CollisionChunkManager &collisionChunkManager = sceneManager.collisionChunkManager;

	const GameState &gameState = game.gameState;
	const double ceilingScale = gameState.getActiveCeilingScale();

	// @todo: BodyInterface::MoveKinematic()?

	// update velocity member, then:
	// Character::SetLinearVelocity or CharacterVirtual::SetLinearVelocity before their update()?

	// @todo: move the player collider(s) to where it ATTEMPTS to be for the physics system update, then see where it actually is in postPhysicsStep()

	if (!this->onGround())
	{
		this->accelerate(-Double3::UnitY, Physics::GRAVITY, dt);
	}

	//this->handleCollision(dt, voxelChunkManager, collisionChunkManager, ceilingScale);

	if (this->onGround())
	{
		Double2 velocityXZ(this->velocity.x, this->velocity.z);
		Double2 frictionDirection = Double2(-velocityXZ.x, -velocityXZ.y).normalized();
		double frictionMagnitude = velocityXZ.length() * this->friction;

		if (std::isfinite(frictionDirection.length()) && (frictionMagnitude > Constants::Epsilon))
		{
			this->accelerate(Double3(frictionDirection.x, 0.0, frictionDirection.y), frictionMagnitude, dt);
		}
	}

	const JPH::Vec3Arg physicsVelocity(
		static_cast<float>(this->velocity.x),
		static_cast<float>(this->velocity.y),
		static_cast<float>(this->velocity.z));
	this->physicsCharacterVirtual->SetLinearVelocity(physicsVelocity);
	this->physicsCharacter->SetLinearVelocity(physicsVelocity);

	JPH::PhysicsSystem &physicsSystem = game.physicsSystem;
	const JPH::Vec3Arg physicsGravity = -this->physicsCharacter->GetUp() * physicsSystem.GetGravity().Length();
	JPH::CharacterVirtual::ExtendedUpdateSettings extendedUpdateSettings; // @todo: for stepping up/down stairs
	const JPH::BroadPhaseLayerFilter &broadPhaseLayerFilter = physicsSystem.GetDefaultBroadPhaseLayerFilter(PhysicsLayers::MOVING);
	const JPH::ObjectLayerFilter &objectLayerFilter = physicsSystem.GetDefaultLayerFilter(PhysicsLayers::MOVING);
	const JPH::BodyFilter bodyFilter; // Nothing
	const JPH::ShapeFilter shapeFilter; // Nothing

	// Update + stick to floor + walk stairs
	this->physicsCharacterVirtual->ExtendedUpdate(
		static_cast<float>(dt),
		physicsGravity,
		extendedUpdateSettings,
		broadPhaseLayerFilter,
		objectLayerFilter,
		bodyFilter,
		shapeFilter,
		*game.physicsTempAllocator);
}

void Player::postPhysicsStep(Game &game)
{
	if (game.options.getMisc_GhostMode())
	{
		return;
	}

	// @todo: not completely understanding the character + charactervirtual synergy yet
	// - i think charactervirtual is for stairsteps and 'weird interactions' that character gets driven by?

	constexpr float maxSeparationDistance = static_cast<float>(Constants::Epsilon);
	this->physicsCharacter->PostSimulation(maxSeparationDistance);

	const JPH::RVec3 physicsCharPos = this->physicsCharacter->GetPosition();
	const WorldDouble3 worldPos(
		static_cast<SNDouble>(physicsCharPos.GetX()),
		static_cast<double>(physicsCharPos.GetY()),
		static_cast<WEDouble>(physicsCharPos.GetZ()));

	this->position = VoxelUtils::worldPointToCoord(worldPos);
}
