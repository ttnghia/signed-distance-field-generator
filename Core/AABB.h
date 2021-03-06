
#pragma once

#include <algorithm>
#include "OgreMath/OgreVector3.h"
#include "Ray.h"
#include "MathMisc.h"

class AABB
{
public:
	Ogre::Vector3 min, max;
	virtual ~AABB() {}
	AABB() {}
	AABB(const Ogre::Vector3 &_min, const Ogre::Vector3 &_max) : min(_min), max(_max) {}

	/// Merge constructor
    /*AABB(const AABB &aabb1, const AABB &aabb2)
	{
		min.x = std::min(aabb1.min.x, aabb2.min.x);
		min.y = std::min(aabb1.min.y, aabb2.min.y);
		min.z = std::min(aabb1.min.z, aabb2.min.z);
		max.x = std::max(aabb1.max.x, aabb2.max.x);
		max.y = std::max(aabb1.max.y, aabb2.max.y);
		max.z = std::max(aabb1.max.z, aabb2.max.z);
    }*/

	/// Sphere constructor
	AABB(const Ogre::Vector3 &sphereCenter, float sphereRadius)
	{
		min = sphereCenter - Ogre::Vector3(sphereRadius, sphereRadius, sphereRadius);
		max = sphereCenter + Ogre::Vector3(sphereRadius, sphereRadius, sphereRadius);
	}

	/// Points constructor.
	AABB(const std::vector<Ogre::Vector3> &points)
	{
		assert(points.size() > 1);
		min = *points.begin();
		max = min;
		for (auto i = points.begin() + 1; i != points.end(); ++i)
		{
			min.x = std::min(min.x, i->x);
			min.y = std::min(min.y, i->y);
			min.z = std::min(min.z, i->z);
			max.x = std::max(max.x, i->x);
			max.y = std::max(max.y, i->y);
			max.z = std::max(max.z, i->z);
		}
	}

    inline void merge(const AABB &aabb2)
    {
        min.x = std::min(min.x, aabb2.min.x);
        min.y = std::min(min.y, aabb2.min.y);
        min.z = std::min(min.z, aabb2.min.z);
        max.x = std::max(max.x, aabb2.max.x);
        max.y = std::max(max.y, aabb2.max.y);
        max.z = std::max(max.z, aabb2.max.z);
    }

	void addEpsilon(float epsilon)
	{
		Ogre::Vector3 epsilonVec(epsilon, epsilon, epsilon);
		min -= epsilonVec;
		max += epsilonVec;
	}

    inline Ogre::Vector3 getCorner(int corner) const
	{
		Ogre::Vector3 sizeVec(max - min);
		return min + Ogre::Vector3((float)((corner & 4) != 0), (float)((corner & 2) != 0), (float)(corner & 1)) * sizeVec;
	}

    inline bool intersectsAABB(const AABB& otherAABB) const
	{
		// perform separating axis test
		if (MathMisc::intervalDoesNotOverlap(min.x, max.x, otherAABB.min.x, otherAABB.max.x)) return false;
		if (MathMisc::intervalDoesNotOverlap(min.y, max.y, otherAABB.min.y, otherAABB.max.y)) return false;
		if (MathMisc::intervalDoesNotOverlap(min.z, max.z, otherAABB.min.z, otherAABB.max.z)) return false;
		return true;
	}

    inline bool containsPoint(const Ogre::Vector3& point) const
	{
		return (point.x >= min.x && point.x < max.x
			&& point.y >= min.y && point.y < max.y
			&& point.z >= min.z && point.z < max.z);
	}

    inline bool intersectsSphere(const Ogre::Vector3& center, float radius) const
	{
		// if (!intersectsAABB(AABB(center, radius))) return false;
		return squaredDistance(center) < radius*radius;
	}

	/// Returns the squared distance to the AABB for the given point, returns 0 if the point is inside the AABB.
    inline float squaredDistance(const Ogre::Vector3& point) const
	{
		return MathMisc::aabbPointSquaredDistance(min, max, point);
	}

    inline float getMaximumSquaredDistance(const Ogre::Vector3& point) const
    {
        float maxSquaredDist = 0.0f;
        for (int i = 0; i < 8; i++)
        {
            maxSquaredDist = std::max(maxSquaredDist, getCorner(i).squaredDistance(point));
        }
        return maxSquaredDist;
    }

    bool intersectsPlane(const Ogre::Vector3& p, const Ogre::Vector3& normal) const
    {
        unsigned char mask = 0;
        for (int i = 0; i < 8; i++)
        {
            float dist = (getCorner(i) - p).dotProduct(normal);
            mask |= (1 << (int)(dist < 0.0f));
            if (mask == 3) return true;
        }
        return false;
    }

    bool intersectsTriangle(const Ogre::Vector3& p1, const Ogre::Vector3& p2, const Ogre::Vector3& p3, const Ogre::Vector3& normal) const
	{
		// perform separating axis test
		// first check AABB face normals
		float tMin, tMax;
		MathMisc::projectTriangleOnAxis(Ogre::Vector3(1, 0, 0), p1, p2, p3, tMin, tMax);
		if (MathMisc::intervalDoesNotOverlap(min.x, max.x, tMin, tMax)) return false;
		MathMisc::projectTriangleOnAxis(Ogre::Vector3(0, 1, 0), p1, p2, p3, tMin, tMax);
		if (MathMisc::intervalDoesNotOverlap(min.y, max.y, tMin, tMax)) return false;
		MathMisc::projectTriangleOnAxis(Ogre::Vector3(0, 0, 1), p1, p2, p3, tMin, tMax);
		if (MathMisc::intervalDoesNotOverlap(min.z, max.z, tMin, tMax)) return false;

		float aabbMin, aabbMax;
		Ogre::Vector3 aabbPoints[8];
        const float epsilon = 0.00001f;
        Ogre::Vector3 minEpsilon = min - Ogre::Vector3(epsilon, epsilon, epsilon);
        Ogre::Vector3 maxEpsilon = max + Ogre::Vector3(epsilon, epsilon, epsilon);
		for (int i = 0; i < 8; i++)
		{
            aabbPoints[i] = minEpsilon;
            if ( i & 4) aabbPoints[i].x = maxEpsilon.x;
            if ( i & 2) aabbPoints[i].y = maxEpsilon.y;
            if ( i & 1) aabbPoints[i].z = maxEpsilon.z;
		}

        MathMisc::projectTriangleOnAxis(normal, p1, p2, p3, tMin, tMax);
		MathMisc::projectAABBOnAxis(normal, aabbPoints, aabbMin, aabbMax);
        if (MathMisc::intervalDoesNotOverlap(aabbMin, aabbMax, tMin, tMax))
            return false;

		Ogre::Vector3 edges[3] = {(p2-p1), (p3-p1), (p3-p2)};
        for (int i = 0; i < 3; i++)
		{
			Ogre::Vector3 axis = edges[i];
			MathMisc::projectTriangleOnAxis(axis, p1, p2, p3, tMin, tMax);
			MathMisc::projectAABBOnAxis(axis, aabbPoints, aabbMin, aabbMax);
			if (MathMisc::intervalDoesNotOverlap(aabbMin, aabbMax, tMin, tMax)) return false;

			axis = edges[i].crossProduct(Ogre::Vector3(1, 0, 0));
			MathMisc::projectTriangleOnAxis(axis, p1, p2, p3, tMin, tMax);
			MathMisc::projectAABBOnAxis(axis, aabbPoints, aabbMin, aabbMax);
			if (MathMisc::intervalDoesNotOverlap(aabbMin, aabbMax, tMin, tMax)) return false;

			axis = edges[i].crossProduct(Ogre::Vector3(0, 1, 0));
			MathMisc::projectTriangleOnAxis(axis, p1, p2, p3, tMin, tMax);
			MathMisc::projectAABBOnAxis(axis, aabbPoints, aabbMin, aabbMax);
			if (MathMisc::intervalDoesNotOverlap(aabbMin, aabbMax, tMin, tMax)) return false;

			axis = edges[i].crossProduct(Ogre::Vector3(0, 0, 1));
			MathMisc::projectTriangleOnAxis(axis, p1, p2, p3, tMin, tMax);
			MathMisc::projectAABBOnAxis(axis, aabbPoints, aabbMin, aabbMax);
			if (MathMisc::intervalDoesNotOverlap(aabbMin, aabbMax, tMin, tMax)) return false;
        }
		return true;
	}

    inline bool rayIntersect(const Ray& ray, float tNear, float tFar) const
	{
		return ray.intersectAABB(&min, tNear, tFar);
	}
    inline bool rayIntersect(const Ray& ray) const
	{
		return ray.intersectAABB(&min);
	}

    inline Ogre::Vector3 getCenter() const
	{
		return (min + max) * 0.5f;
	}

    inline const Ogre::Vector3& getMin() const
	{
		return min;
	}

    inline const Ogre::Vector3& getMax() const
	{
		return max;
	}
};
