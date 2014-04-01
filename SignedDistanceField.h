
#pragma once

#include "Vector3i.h"
#include "OgreMath/OgreVector3.h"
#include "AABB.h"
#include "Area.h"
#include "OgreMath/OgreVector2.h"

/**
Interface for three-dimensional signed distance fields. Such a signed distance field can be sampled on a grid, but it could also be encoded implicitly.
An example is a sphere with center and radius, but also a triangle mesh.
*/

typedef unsigned int MaterialID;

class SignedDistanceField3D
{
public:
	// If you want to store additional data in the signed distance grid, this is the right place to add it.
	struct Sample
	{
		Sample() {}
		Sample(float signedDistance) : signedDistance(signedDistance) {}
		Sample(float signedDistance, const Ogre::Vector3& normal, const Ogre::Vector2& uv, const MaterialID& materialID)
			: signedDistance(signedDistance), normal(normal), uv(uv), materialID(materialID) {}

		float signedDistance;

		MaterialID materialID;

		Ogre::Vector3 normal;
		Ogre::Vector3 correctionVector;
		Ogre::Vector2 uv;

		// Operators required for trilinear interpolation, obviously this can not handle the materialID correctly.
		inline Sample operator * (float rhs) const
		{
			return Sample(signedDistance * rhs, normal * rhs, uv * rhs, materialID);
		}
		inline Sample operator + (const Sample& rhs) const
		{
			return Sample(signedDistance + rhs.signedDistance, normal + rhs.normal, uv + rhs.uv, materialID);
		}
	};

	// Convenient helper methods for subclasses.
	static bool allSignsAreEqual(const float* signedDistances)
	{
		bool positive = (signedDistances[0] >= 0.0f);
		for (int i = 1; i < 8; i++)
		{
			if ((signedDistances[i] >= 0.0f) != positive)
				return false;
		}
		return true;
	}

	static bool allSignsAreEqual(const Sample** samples)
	{
		bool positive = (samples[0]->signedDistance >= 0.0f);
		for (int i = 1; i < 8; i++)
		{
			if ((samples[i]->signedDistance >= 0.0f) != positive)
				return false;
		}
		return true;
	}

	static bool signsAreEqual(float val1, float val2)
	{
		return (val1 >= 0.0f && val2 >= 0.0f) || (val1 < 0.0f && val2 < 0.0f);
	}

	/// Retrieves the sample at the given point (exact for implicit SDFs, interpolated for sampled SDFs).
	virtual Sample getSample(const Ogre::Vector3& point) const = 0;

	virtual void getSample(const Ogre::Vector3& point, Sample& sample) const { sample = getSample(point); }

	/// Retrieves whether the given AABB intersects the surface (zero contour of the sdf).
	virtual bool intersectsSurface(const AABB& aabb) const = 0;

	/// Implementations may override this to provide high speed implementations for cubic aabbs.
	virtual bool cubeNeedsSubdivision(const Area& area) const { return intersectsSurface(area.toAABB()); }

	virtual void getLowerAndUpperBound(const Area& area, bool containsSurface, const float* signedCornerDistances, float& lowerBound, float& upperBound) const
	{
		area.getLowerAndUpperBound(signedCornerDistances, lowerBound, upperBound);
		/*if (containsSurface)
			area.getLowerAndUpperBound(signedCornerDistances, lowerBound, upperBound);
		else
			area.getLowerAndUpperBoundOptimistic(signedCornerDistances, lowerBound, upperBound);*/
	}

	/// Retrieves the axis aligned bounding box of the sdf.
	virtual AABB getAABB() const = 0;

	/// Called before the first call to getSample. Usually not required, only used by TriangleMeshSDF_Robust so far which builds a grid.
	virtual void prepareSampling(const AABB& aabb, float cellSize) {}
};

// Interface for signed distance fields that can be processed using the Marching Cubes algorithm.
class SampledSignedDistanceField3D : public SignedDistanceField3D
{
public:
	struct Cube
	{
		Vector3i posMin;
		Sample* cornerSamples[8];
	};
	virtual std::vector<Cube> getCubesToMarch() = 0;

	virtual float getInverseCellSize() = 0;
};