
#pragma once

#include "OgreMath/OgreVector3.h"
#include <vector>
#include "SolidGeometry.h"
#include "AABB.h"

class OpIntersectionSDF : public SolidGeometry
{
protected:
    std::vector<SolidGeometry*> m_SDFs;
	AABB m_AABB;

public:
    OpIntersectionSDF(std::vector<SolidGeometry*> sdfs) : m_SDFs(sdfs)
	{
		if (!m_SDFs.empty())
		{
			m_AABB = (*m_SDFs.begin())->getAABB();
			for (auto i = m_SDFs.begin() + 1; i != m_SDFs.end(); ++i)
			{
				m_AABB.merge((*i)->getAABB());
			}
		}
	}
	virtual void getSample(const Ogre::Vector3& point, Sample& minSample) const override
	{
        minSample.signedDistance = std::numeric_limits<float>::max();
        Sample sample;
		for (auto i = m_SDFs.begin(); i != m_SDFs.end(); ++i)
		{
            (*i)->getSample(point, sample);
			if (sample.signedDistance < minSample.signedDistance)
				minSample = sample;
		}
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
