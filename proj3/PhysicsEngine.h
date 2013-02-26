#ifndef _PHYSICS_ENGINE_H
#define _PHYSICS_ENGINE_H

#include <vector>
#include <stack>
#include <osg/PositionAttitudeTransform>

using namespace std;

#define GRAVITY 0.098
#define SIZE 128
#define BOUNCES 3

class PhysicsEngine
{
	public:
		PhysicsEngine();
		void Update();
		void AddObject(osg::PositionAttitudeTransform * pos);
	private:
		stack<int> availableSlots;
		vector<osg::PositionAttitudeTransform*> shapes;
};

#endif