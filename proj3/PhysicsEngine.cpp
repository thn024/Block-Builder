#include "PhysicsEngine.h"

//--------------------------------------------------------------------------
PhysicsEngine::PhysicsEngine()
{
	
}
//--------------------------------------------------------------------------
void PhysicsEngine::Update()
{
	//update position for objects	
	for(unsigned i = 0; i < shapes.size(); ++i)
	{
		osg::Vec3d position = shapes[i]->getPosition() - osg::Vec3d(0,0,GRAVITY);
		if(position.z() < -5)
		{
			shapes.erase(shapes.begin()+i);
			continue;
		}
		else
			shapes[i]->setPosition(position);
	}
}
//--------------------------------------------------------------------------
void PhysicsEngine::AddObject(osg::PositionAttitudeTransform * pos)
{
	shapes.push_back(pos);
}
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------