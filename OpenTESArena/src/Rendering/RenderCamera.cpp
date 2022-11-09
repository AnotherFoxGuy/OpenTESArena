#include "RenderCamera.h"

void RenderCamera::init(const ChunkInt2 &chunk, const Double3 &point, const Double3 &direction, Degrees fovX,
	Degrees fovY, double aspectRatio)
{
	this->chunk = chunk;
	this->point = point;

	this->forward = direction.normalized();
	this->zoom = MathUtils::verticalFovToZoom(fovY);
	this->forwardScaled = this->forward * this->zoom;

	this->right = this->forward.cross(Double3::UnitY).normalized();
	this->aspectRatio = aspectRatio;
	this->rightScaled = this->right * this->aspectRatio;

	this->up = this->right.cross(this->forward).normalized();

	this->leftFrustumDir = (this->forwardScaled - this->rightScaled).normalized();
	this->rightFrustumDir = (this->forwardScaled + this->rightScaled).normalized();
	this->bottomFrustumDir = (this->forwardScaled - this->up).normalized();
	this->topFrustumDir = (this->forwardScaled + this->up).normalized();

	this->leftFrustumNormal = this->leftFrustumDir.cross(this->up).normalized();
	this->rightFrustumNormal = this->up.cross(this->rightFrustumDir).normalized();
	this->bottomFrustumNormal = this->right.cross(this->bottomFrustumDir).normalized();
	this->topFrustumNormal = this->topFrustumDir.cross(this->right).normalized();

	this->fovX = fovX;
	this->fovY = fovY;
}
