
#pragma once

#include <unordered_map>
#include "SignedDistanceField.h"
#include "Vector3i.h"
#include "AABB.h"
#include "Prerequisites.h"
#include "OpInvertSDF.h"

/*
Samples a signed distance field in an adaptive way.
For each node (includes inner nodes and leaves) signed distances are stores for the 8 corners. This allows to interpolate signed distances in the node cell using trilinear interpolation.
The actual signed distances are stored in a spatial hashmap because octree nodes share corners with other nodes.
*/
class OctreeSDF : public SampledSignedDistanceField3D
{
protected:
	struct Area
	{
		Area() {}
		Area(const Vector3i& minPos, int sizeExpo, const Ogre::Vector3& minRealPos, float realSize)
			: m_MinPos(minPos), m_SizeExpo(sizeExpo), m_MinRealPos(minRealPos), m_RealSize(realSize) {}
		Vector3i m_MinPos;
		int m_SizeExpo;

		Ogre::Vector3 m_MinRealPos;
		float m_RealSize;

		__forceinline bool containsPoint(const Ogre::Vector3& point)
		{
			return (point.x >= m_MinRealPos.x && point.x < (m_MinRealPos.x + m_RealSize)
				&& point.y >= m_MinRealPos.y && point.y < (m_MinRealPos.y + m_RealSize)
				&& point.z >= m_MinRealPos.z && point.z < (m_MinRealPos.z + m_RealSize));
		}

		// Computes a lower and upper bound inside the area given the 8 corner signed distances.
		void getLowerAndUpperBound(float* signedDistances, float& lowerBound, float& upperBound) const
		{
			float minDist = std::numeric_limits<float>::max();
			float maxDist = std::numeric_limits<float>::min();
			for (int i = 0; i < 8; i++)
			{
				minDist = std::min(minDist, signedDistances[i]);
				maxDist = std::max(maxDist, signedDistances[i]);
			}
			lowerBound = minDist - m_RealSize * 0.5f;
			upperBound = maxDist + m_RealSize * 0.5f;
		}

		Vector3i getCorner(int corner) const
		{
			return m_MinPos + Vector3i((corner & 4) != 0, (corner & 2) != 0, corner & 1) * (1 << m_SizeExpo);
		}

		/// Retrieves the i-th corner of the cube with 0 = min and 7 = max
		std::pair<Vector3i, Ogre::Vector3> getCornerVecs(int corner) const
		{
			Vector3i offset((corner & 4) != 0, (corner & 2) != 0, corner & 1);
			Ogre::Vector3 realPos = m_MinRealPos;
			for (int i = 0; i < 3; i++)
			{
				realPos[i] += m_RealSize * (float)offset[i];
			}
			return std::make_pair(m_MinPos + offset * (1 << m_SizeExpo), realPos);
		}
		AABB toAABB() const
		{
			return AABB(m_MinRealPos, m_MinRealPos + Ogre::Vector3(m_RealSize, m_RealSize, m_RealSize));
		}

		void getSubAreas(Area* areas) const
		{
			float halfSize = m_RealSize * 0.5f;
			for (int i = 0; i < 8; i++)
			{
				Vector3i offset((i & 4) != 0, (i & 2) != 0, i & 1);
				areas[i] = Area(m_MinPos + offset * (1 << (m_SizeExpo - 1)),
					m_SizeExpo - 1,
					m_MinRealPos + Ogre::Vector3(offset.x * halfSize, offset.y * halfSize, offset.z * halfSize), halfSize);
			}
		}
	};
	struct Node
	{
		Node* m_Children[8];
		Node()
		{
			for (int i = 0; i < 8; i++)
				m_Children[i] = nullptr;
		}
	};

	typedef std::unordered_map<Vector3i, Sample> SignedDistanceGrid;

	SignedDistanceGrid m_SDFValues;
	Node* m_RootNode;

	float m_CellSize;

	/// The octree covers an axis aligned cube.
	 Area m_RootArea;

	Sample lookupOrComputeSample(int corner, const Area& area, const SignedDistanceField3D* implicitSDF, SignedDistanceGrid& sdfValues)
	{
		auto vecs = area.getCornerVecs(corner);
		auto tryInsert = sdfValues.insert(std::make_pair(vecs.first, Sample(0.0f)));
		if (tryInsert.second)
		{
			tryInsert.first->second = implicitSDF->getSample(vecs.second);
		}
		return tryInsert.first->second;
	}

	Sample lookupSample(int corner, const Area& area) const
	{
		auto find = m_SDFValues.find(area.getCorner(corner));
		vAssert(find != m_SDFValues.end())
		return find->second;
	}

	/// Top down octree constructor given a SDF.
	Node* createNode(const Area& area, const SignedDistanceField3D* implicitSDF, SignedDistanceGrid& sdfValues)
	{
		if (area.m_SizeExpo <= 0 ||
			!implicitSDF->intersectsSurface(area.toAABB()))
		{
			float signedDistances[8];
			for (int i = 0; i < 8; i++)
			{
				signedDistances[i] = lookupOrComputeSample(i, area, implicitSDF, sdfValues).signedDistance;
			}
			if (area.m_SizeExpo <= 0 || allSignsAreEqual(signedDistances))
				return nullptr;	// leaf
		}

		// create inner node
		Area subAreas[8];
		area.getSubAreas(subAreas);
		Node* node = new Node();
		for (int i = 0; i < 8; i++)
			node->m_Children[i] = createNode(subAreas[i], implicitSDF, sdfValues);
		return node;
	}

	// Computes a lower and upper bound inside the area given the 8 corner signed distances.
	void getLowerAndUpperBound(Node* node, const Area& area, float* signedDistances, float& lowerBound, float& upperBound) const
	{
		if (node) area.getLowerAndUpperBound(signedDistances, lowerBound, upperBound);
		else
		{	// if it's a leaf we can do even better and just return min and max of the corner values.
			lowerBound = std::numeric_limits<float>::max();
			upperBound = std::numeric_limits<float>::min();
			for (int i = 0; i < 8; i++)
			{
				lowerBound = std::min(lowerBound, signedDistances[i]);
				upperBound = std::max(upperBound, signedDistances[i]);
			}
		}
	}

	void getCubesToMarch(Node* node, const Area& area, vector<Cube>& cubes)
	{
		if (node)
		{
			vAssert(area.m_SizeExpo > 0)
			Area subAreas[8];
			area.getSubAreas(subAreas);
			for (int i = 0; i < 8; i++)
				getCubesToMarch(node->m_Children[i], subAreas[i], cubes);
		}
		else
		{	// leaf
			Cube cube;
			cube.posMin = area.m_MinPos;
			for (int i = 0; i < 8; i++)
			{
				cube.cornerSamples[i] = lookupSample(i, area);
			}
			if (allSignsAreEqual(cube.cornerSamples)) return;
			if (area.m_SizeExpo <= 0)
				cubes.push_back(cube);
			else 
			{
				interpolateLeaf(area);
				Area subAreas[8];
				area.getSubAreas(subAreas);
				for (int i = 0; i < 8; i++)
					getCubesToMarch(nullptr, subAreas[i], cubes);
			}
		}
	}
	Sample getSample(Node* node, const Area& area, const Ogre::Vector3& point) const
	{
		if (!node)
		{
			float invNodeSize = 1.0f / area.m_RealSize;
			float weights[3];
			weights[0] = (point.x - area.m_MinRealPos.x) * invNodeSize;
			weights[1] = (point.y - area.m_MinRealPos.y) * invNodeSize;
			weights[2] = (point.z - area.m_MinRealPos.z) * invNodeSize;
			Sample cornerSamples[8];
			for (int i = 0; i < 8; i++)
				cornerSamples[i] = lookupSample(i, area);
			return MathMisc::trilinearInterpolation(cornerSamples, weights);
		}
		else
		{
			Area subAreas[8];
			area.getSubAreas(subAreas);
			for (int i = 0; i < 8; i++)
			{
				if (subAreas[i].containsPoint(point))
				{
					return getSample(node->m_Children[i], subAreas[i], point);
					break;
				}
			}
		}
		return 0.0f;		// should never occur
	}

	/// Intersects an sdf with the node and returns the new node. The new sdf values are written to newSDF.
	Node* intersect(Node* node, const Area& area, SignedDistanceField3D* otherSDF, SignedDistanceGrid& newSDF, SignedDistanceGrid& otherSDFCache)
	{
		// if otherSDF does not overlap with the node AABB we can stop here
		if (!area.toAABB().intersectsAABB(otherSDF->getAABB())) return node;

		// compute signed distances for this node and the area of the other sdf
		float otherSignedDistances[8];
		float thisSignedDistances[8];
		for (int i = 0; i < 8; i++)
		{
			auto vecs = area.getCornerVecs(i);
			Sample otherSample = lookupOrComputeSample(i, area, otherSDF, otherSDFCache);
			Vector3i globalPos = vecs.first;
			auto find = m_SDFValues.find(globalPos);
			vAssert(find != m_SDFValues.end())
			otherSignedDistances[i] = otherSample.signedDistance;
			thisSignedDistances[i] = find->second.signedDistance;

			if (otherSignedDistances[i] < thisSignedDistances[i])
				newSDF[globalPos] = otherSample;
			else newSDF[globalPos] = find->second;
		}

		// compute a lower and upper bound for this node and the other sdf
		float otherLowerBound, otherUpperBound;
		area.getLowerAndUpperBound(otherSignedDistances, otherLowerBound, otherUpperBound);

		float thisLowerBound, thisUpperBound;
		getLowerAndUpperBound(node, area, thisSignedDistances, thisLowerBound, thisUpperBound);

		if (otherUpperBound < thisLowerBound)
		{	// this node is replaced with the other sdf
			if (node) delete node;
			return createNode(area, otherSDF, newSDF);
		}
		else if (otherLowerBound > thisUpperBound)
		{	// no change for this node
			return node;
		}

		if (node)
		{	// need to recurse to node children
			vAssert(area.m_SizeExpo > 0)
			Area subAreas[8];
			area.getSubAreas(subAreas);
			for (int i = 0; i < 8; i++)
				node->m_Children[i] = intersect(node->m_Children[i], subAreas[i], otherSDF, newSDF, otherSDFCache);
		}
		else
		{	// it's a leaf in the octree
			if (area.m_SizeExpo > 0 && otherSDF->intersectsSurface(area.toAABB()))
			{
				// need to subdivide this node
				node = new Node();
				interpolateLeaf(area);
				Area subAreas[8];
				area.getSubAreas(subAreas);
				for (int i = 0; i < 8; i++)
					node->m_Children[i] = intersect(nullptr, subAreas[i], otherSDF, newSDF, otherSDFCache);
			}
		}
		return node;
	}

	/// Intersects an sdf with the node and returns the new node. The new sdf values are written to newSDF.
	Node* merge(Node* node, const Area& area, SignedDistanceField3D* otherSDF, SignedDistanceGrid& newSDF, SignedDistanceGrid& otherSDFCache)
	{
		// if otherSDF does not overlap with the node AABB we can stop here
		if (!area.toAABB().intersectsAABB(otherSDF->getAABB()))
			return node;

		// compute signed distances for this node and the area of the other sdf
		float otherSignedDistances[8];
		float thisSignedDistances[8];
		for (int i = 0; i < 8; i++)
		{
			auto vecs = area.getCornerVecs(i);
			Sample otherSample = lookupOrComputeSample(i, area, otherSDF, otherSDFCache);
			Vector3i globalPos = vecs.first;
			auto find = m_SDFValues.find(globalPos);
			vAssert(find != m_SDFValues.end())
			otherSignedDistances[i] = otherSample.signedDistance;
			thisSignedDistances[i] = find->second.signedDistance;

			if (otherSignedDistances[i] > thisSignedDistances[i])
				newSDF[globalPos] = otherSample;
			else newSDF[globalPos] = find->second;
		}

		// compute a lower and upper bound for this node and the other sdf
		float otherLowerBound, otherUpperBound;
		area.getLowerAndUpperBound(otherSignedDistances, otherLowerBound, otherUpperBound);

		float thisLowerBound, thisUpperBound;
		getLowerAndUpperBound(node, area, thisSignedDistances, thisLowerBound, thisUpperBound);

		if (!node && thisLowerBound > 0)
		{	// no change - already completely solid
			return node;
		}

		if (!node || otherLowerBound > thisUpperBound)
		{	// this node is replaced with the other sdf
			// this could lead to inaccurate distance outside the volume, however these are usually not required
			if (node) delete node;
			return createNode(area, otherSDF, newSDF);
		}
		if (node)
		{	// need to recurse to node children
			vAssert(area.m_SizeExpo > 0)
			Area subAreas[8];
			area.getSubAreas(subAreas);
			for (int i = 0; i < 8; i++)
				node->m_Children[i] = merge(node->m_Children[i], subAreas[i], otherSDF, newSDF, otherSDFCache);
		}
		return node;
	}

	/// Interpolates signed distance for the 3x3x3 subgrid of a leaf.
	void interpolateLeaf(const Area& area)
	{
		Area subAreas[8];
		area.getSubAreas(subAreas);

		Sample cornerSamples[8];
		for (int i = 0; i < 8; i++)
		{
			cornerSamples[i] = lookupSample(i, area);
		}

		int expoMultiplier = (1 << (subAreas[0].m_SizeExpo));
		int expoMultiplier2 = (expoMultiplier << 1);

		Vector3i minPos = subAreas[0].m_MinPos;

		// first do the xy plane at z = 0
		Sample edgeMid15 = (cornerSamples[0] + cornerSamples[4]) * 0.5f;
		Sample edgeMid13 = (cornerSamples[0] + cornerSamples[2]) * 0.5f;
		Sample edgeMid57 = (cornerSamples[4] + cornerSamples[6]) * 0.5f;
		Sample edgeMid37 = (cornerSamples[2] + cornerSamples[6]) * 0.5f;
		Sample faceMid1 = (edgeMid15 + edgeMid37) * 0.5f;
		m_SDFValues.insert(std::make_pair(minPos + Vector3i(expoMultiplier, 0, 0), edgeMid15));
		m_SDFValues.insert(std::make_pair(minPos + Vector3i(0, expoMultiplier, 0), edgeMid13));
		m_SDFValues.insert(std::make_pair(minPos + Vector3i(expoMultiplier, expoMultiplier, 0), faceMid1));
		m_SDFValues.insert(std::make_pair(minPos + Vector3i(expoMultiplier, expoMultiplier2, 0), edgeMid57));
		m_SDFValues.insert(std::make_pair(minPos + Vector3i(expoMultiplier2, expoMultiplier, 0), edgeMid37));

		// then the xy plane at z = 2
		minPos = minPos + Vector3i(0, 0, 2 * expoMultiplier);
		Sample edgeMid26 = (cornerSamples[1] + cornerSamples[5]) * 0.5f;
		Sample edgeMid24 = (cornerSamples[1] + cornerSamples[3]) * 0.5f;
		Sample edgeMid68 = (cornerSamples[5] + cornerSamples[7]) * 0.5f;
		Sample edgeMid48 = (cornerSamples[3] + cornerSamples[7]) * 0.5f;
		Sample faceMid2 = (edgeMid26 + edgeMid48) * 0.5f;
		m_SDFValues.insert(std::make_pair(minPos + Vector3i(expoMultiplier, 0, 0), edgeMid26));
		m_SDFValues.insert(std::make_pair(minPos + Vector3i(0, expoMultiplier, 0), edgeMid24));
		m_SDFValues.insert(std::make_pair(minPos + Vector3i(expoMultiplier, expoMultiplier, 0), faceMid2));
		m_SDFValues.insert(std::make_pair(minPos + Vector3i(expoMultiplier, expoMultiplier2, 0), edgeMid68));
		m_SDFValues.insert(std::make_pair(minPos + Vector3i(expoMultiplier2, expoMultiplier, 0), edgeMid48));

		// 4 edges at z = 1
		minPos = subAreas[0].m_MinPos + Vector3i(0, 0, expoMultiplier);
		m_SDFValues.insert(std::make_pair(minPos, (cornerSamples[0] + cornerSamples[1]) * 0.5f));
		m_SDFValues.insert(std::make_pair(minPos + Vector3i(0, expoMultiplier2, 0), (cornerSamples[2] + cornerSamples[3]) * 0.5f));
		m_SDFValues.insert(std::make_pair(minPos + Vector3i(expoMultiplier2, 0, 0), (cornerSamples[4] + cornerSamples[5]) * 0.5f));
		m_SDFValues.insert(std::make_pair(minPos + Vector3i(expoMultiplier2, expoMultiplier2, 0), (cornerSamples[6] + cornerSamples[7]) * 0.5f));

		// 4 faces at z = 1
		m_SDFValues.insert(std::make_pair(minPos + Vector3i(0, expoMultiplier, 0), (edgeMid13 + edgeMid24) * 0.5f));
		m_SDFValues.insert(std::make_pair(minPos + Vector3i(expoMultiplier, 0, 0), (edgeMid15 + edgeMid26) * 0.5f));
		m_SDFValues.insert(std::make_pair(minPos + Vector3i(expoMultiplier, expoMultiplier2, 0), (edgeMid37 + edgeMid48) * 0.5f));
		m_SDFValues.insert(std::make_pair(minPos + Vector3i(expoMultiplier2, expoMultiplier, 0), (edgeMid57 + edgeMid68) * 0.5f));

		// and finally, the mid point
		m_SDFValues.insert(std::make_pair(minPos + Vector3i(expoMultiplier, expoMultiplier, 0), (faceMid1 + faceMid2) * 0.5f));

		/*Area subArea = subAreas[0];
		Sample currentSample = cornerSamples[0];
		for (int x = 0; x < 3; x++)
		{
			for (int y = 0; y < 3; y++)
			{
				subArea.m_MinPos.y += expoMultiplier, 0;
				currentSample += 
			}
		}
		subArea.m_MinPos = subArea.m_MinPos + Vector3i(1, 0, 0) * expoMultiplier;
		for (int x = 0; x < 3; x++)

		// interpolate 3x3x3 signed distance subgrid
		Vector3i subGridVecs[27];
		Vector3i::grid3(subGridVecs);
		for (int i = 0; i < 27; i++)
		{
			float weights[3];
			for (int d = 0; d < 3; d++)
				weights[d] = subGridVecs[i][d] * 0.5f;
			Area subArea = subAreas[0];
			subArea.m_MinPos = subArea.m_MinPos + subGridVecs[i] * (1 << (subArea.m_SizeExpo));
			auto tryInsert = m_SDFValues.insert(std::make_pair(subArea.getCorner(0), 0.0f));
			if (tryInsert.second)
				tryInsert.first->second = MathMisc::trilinearInterpolation(cornerSamples, weights);
		}*/
	}
public:
	static std::shared_ptr<OctreeSDF> sampleSDF(std::shared_ptr<SignedDistanceField3D> otherSDF, int maxDepth)
	{
		std::shared_ptr<OctreeSDF> octreeSDF = std::make_shared<OctreeSDF>();
		AABB aabb = otherSDF->getAABB();
		Ogre::Vector3 aabbSize = aabb.getMax() - aabb.getMin();
		float cubeSize = std::max(std::max(aabbSize.x, aabbSize.y), aabbSize.z);
		octreeSDF->m_CellSize = cubeSize / (1 << maxDepth);
		otherSDF->prepareSampling(aabb, octreeSDF->m_CellSize);
		octreeSDF->m_RootArea = Area(Vector3i(0, 0, 0), maxDepth, aabb.getMin(), cubeSize);
		octreeSDF->m_RootNode = octreeSDF->createNode(octreeSDF->m_RootArea, otherSDF.get(), octreeSDF->m_SDFValues);
		return octreeSDF;
	}

	float getInverseCellSize() override
	{
		return (float)(1 << m_RootArea.m_SizeExpo) / m_RootArea.m_RealSize;
	}

	AABB getAABB() const override { return m_RootArea.toAABB(); }

	vector<Cube> getCubesToMarch()
	{
		vector<Cube> cubes;
		std::stack<Node*> nodes;
		getCubesToMarch(m_RootNode, m_RootArea, cubes);
		return cubes;
	}

	Sample getSample(const Ogre::Vector3& point) const override
	{
		return getSample(m_RootNode, m_RootArea, point);
	}

	// TODO!
	bool intersectsSurface(const AABB &) const override
	{
		return true;
	}

	void subtract(std::shared_ptr<SignedDistanceField3D> otherSDF)
	{
		otherSDF->prepareSampling(m_RootArea.toAABB(), m_CellSize);
		SignedDistanceGrid newSDF;
		OpInvertSDF invertedSDF(otherSDF.get());
		m_RootNode = intersect(m_RootNode, m_RootArea, &invertedSDF, newSDF, SignedDistanceGrid());
		for (auto i = newSDF.begin(); i != newSDF.end(); i++)
			m_SDFValues[i->first] = i->second;
	}

	void intersect(std::shared_ptr<SignedDistanceField3D> otherSDF)
	{
		otherSDF->prepareSampling(m_RootArea.toAABB(), m_CellSize);
		SignedDistanceGrid newSDF;
		m_RootNode = intersect(m_RootNode, m_RootArea, otherSDF.get(), newSDF, SignedDistanceGrid());
		for (auto i = newSDF.begin(); i != newSDF.end(); i++)
			m_SDFValues[i->first] = i->second;
	}

	/// Resizes the octree so that it covers the given aabb.
	void resize(const AABB& aabb)
	{
		while (!m_RootArea.toAABB().containsPoint(aabb.min))
		{	// need to resize octree
			m_RootArea.m_MinPos = m_RootArea.m_MinPos - Vector3i(1 << m_RootArea.m_SizeExpo, 1 << m_RootArea.m_SizeExpo, 1 << m_RootArea.m_SizeExpo);
			m_RootArea.m_MinRealPos -= Ogre::Vector3(m_RootArea.m_RealSize, m_RootArea.m_RealSize, m_RootArea.m_RealSize);
			m_RootArea.m_RealSize *= 2.0f;
			m_RootArea.m_SizeExpo++;
			Node* oldRoot = m_RootNode;
			m_RootNode = new Node();
			m_RootNode->m_Children[7] = oldRoot;

			// need to fill in some signed distance values for the new area
			for (int i = 0; i < 7; i++)
			{
				Vector3i offset = Vector3i((i & 4) != 0, (i & 2) != 0, (i & 1) != 0) * (1 << m_RootArea.m_SizeExpo);
				m_SDFValues[m_RootArea.m_MinPos + offset] = -m_RootArea.m_RealSize;
			}
			interpolateLeaf(m_RootArea);
		}
		while (!m_RootArea.toAABB().containsPoint(aabb.max))
		{	// need to resize octree
			m_RootArea.m_RealSize *= 2.0f;
			m_RootArea.m_SizeExpo++;
			Node* oldRoot = m_RootNode;
			m_RootNode = new Node();
			m_RootNode->m_Children[0] = oldRoot;

			// need to fill in some signed distance values for the new area
			for (int i = 1; i < 8; i++)
			{
				Vector3i offset = Vector3i((i & 4) != 0, (i & 2) != 0, (i & 1) != 0) * (1 << m_RootArea.m_SizeExpo);
				m_SDFValues[m_RootArea.m_MinPos + offset] = -m_RootArea.m_RealSize;
			}
			interpolateLeaf(m_RootArea);
		}
	}

	void merge(std::shared_ptr<SignedDistanceField3D> otherSDF)
	{
		// this is not an optimal resize policy but it should work
		// it is recommended to avoid resizes anyway
		resize(otherSDF->getAABB());

		otherSDF->prepareSampling(m_RootArea.toAABB(), m_CellSize);
		SignedDistanceGrid newSDF;
		m_RootNode = merge(m_RootNode, m_RootArea, otherSDF.get(), newSDF, SignedDistanceGrid());
		for (auto i = newSDF.begin(); i != newSDF.end(); i++)
			m_SDFValues[i->first] = i->second;
	}
};