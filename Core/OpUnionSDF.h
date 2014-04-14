
#pragma once

#include "OgreMath/OgreVector3.h"
#include <vector>
#include "SignedDistanceField.h"
#include "AABB.h"

class OpUnionSDF : public SignedDistanceField3D
{
protected:
	std::vector<SignedDistanceField3D*> m_SDFs;
	AABB m_AABB;

public:
	OpUnionSDF(std::vector<SignedDistanceField3D*> sdfs) : m_SDFs(sdfs)
	{
		if (!m_SDFs.empty())
		{
			m_AABB = (*m_SDFs.begin())->getAABB();
			for (auto i = m_SDFs.begin() + 1; i != m_SDFs.end(); ++i)
			{
				m_AABB = AABB(m_AABB, (*i)->getAABB());
			}
		}
	}
	Sample getSample(const Ogre::Vector3& point) const override
	{
		Sample maxSample(std::numeric_limits<float>::lowest());
		for (auto i = m_SDFs.begin(); i != m_SDFs.end(); ++i)
		{
			Sample sample = (*i)->getSample(point);
			if (sample.signedDistance > maxSample.signedDistance)
				maxSample = sample;
		}
		return maxSample;
	}

	bool intersectsSurface(const AABB& aabb) const override
	{
		for (auto i = m_SDFs.begin(); i != m_SDFs.end(); ++i)
		{
			if ((*i)->intersectsSurface(aabb)) return true;
		}
		return false;
	}

	AABB getAABB() const override
	{
		return m_AABB;
	}

	void prepareSampling(const AABB& aabb, float cellSize) override
	{
		for (auto i = m_SDFs.begin(); i != m_SDFs.end(); ++i)
			(*i)->prepareSampling(aabb, cellSize);
	}
};