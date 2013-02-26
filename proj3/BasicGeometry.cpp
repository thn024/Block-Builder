#include <osg/Node>
#include <osg/Group>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Texture2D>
#include <osgDB/ReadFile>
#include <osg/Material>
#include <osg/ShapeDrawable>
#include <osgViewer/Viewer>
#include <osg/PositionAttitudeTransform>
#include <osgGA/TrackballManipulator>
#include <osg/PolygonMode>
#include <osgText/Text>
#include <osg/LineWidth>
#include <osg/math>
#include <osg/Image>
#include <osg/Texture>
#include <osg/StateSet>
#include <math.h>
#include <string>
#include "PhysicsEngine.h"
#include "CcolorVisitor.hpp"
#include <osg/Program>
#include <osg/Shader>
#include <osg/Uniform>
#include <osg/Light>
#include <osg/LightSource>

#include <osg/MatrixTransform>
#include <sixense.h>
#include <sixense_math.hpp>

#include <math.h>

//macros
#define RIGHT 1
#define LEFT 0
#define MAX_KEYS 7

#define SQUARE 1
#define RECTANGLE 2
#define SPHERE 3
#define CONE 4
#define CYLINDER 5

//globals
sixenseControllerData controllerLeft;
sixenseControllerData controllerRight;

osg::Group* root = new osg::Group();
osg::Matrixd * boxMatrix = new osg::Matrixd() ;
osg::MatrixTransform* boxTransform = new osg::MatrixTransform();
osg::MatrixTransform* currentTransform;
osg::MatrixTransform* currentScaleTransform;
osg::PositionAttitudeTransform * colorPicker = new osg::PositionAttitudeTransform();
osg::PositionAttitudeTransform * pos;
osg::Vec4 colorVector = osg::Vec4(0,0,0,1);
osg::ShapeDrawable* currentColor;
osg::Geometry *cornersGeo=new osg::Geometry;

osg::Texture2D * wheel_texture;
osg::Texture2D * shape_texture;

static bool rightKeys[MAX_KEYS];
static bool leftKeys[MAX_KEYS];
bool isShapeSet = true;
bool colorNodeMask = false;
bool shapeNodeMask = false;

bool shapeCreated = false;
bool rightTrigger = false;
bool leftTrigger = false;

float scaleFactor = 1.0;
float oldScaleFactor = 1.0;
bool scaleButtonsPressed = false;
int scaleDirection = 0;
float initialDistance = 0;

int numRootChildren = 0;

PhysicsEngine en;


//functions
void selectShape(float aX, float aY);
void selectColor(bool isChooseColor, float aX, float aY);
void scaleObject();
void handleButtonPress(int which);
bool onButtonRelease(int controller, int button);
osg::Geode* MakeSphere(float radius);
osg::Geode* MakeRectangle(int size);
osg::Geode* MakePlane(int size);
osg::Geode* MakeBox(float size);
osg::Geode* MakeCone(float radius, float height);
osg::Geode* MakeCylinder(float radius, float height);
void forcedWireFrameModeOn( osg::Node *srcNode );
void forcedWireFrameModeOff( osg::Node *srcNode );
osg::Node* createColorsHUD(osgText::Text* updateText);
osg::Vec3Array* circleVerts( int plane, int approx, float radius );
osg::Geode* circles( int plane, int approx, float radius );
void initTexture();
osg::Program* createShader();
osg::Vec4Array* colors = new osg::Vec4Array;

bool wasScaling = false;
osg::Material * material = new osg::Material;

double m[4][4];
double scale[4][4];

class SineAnimation: public osg::Uniform::Callback
{
public:
    SineAnimation( float rate = 1.0f, float scale = 1.0f, float offset = 0.0f ) :
            _rate(rate), _scale(scale), _offset(offset)
    {}

    void operator()( osg::Uniform* uniform, osg::NodeVisitor* nv )
    {
        float angle = _rate * nv->getFrameStamp()->getSimulationTime();
        float value = sinf( angle ) * _scale + _offset;
        uniform->set( value );
    }

private:
    const float _rate;
    const float _scale;
    const float _offset;
};
//--------------------------------------------------------------------------
void initTexture()
{
	osg::Image * color_wheel = osgDB::readImageFile("./data/color_wheel2.png");
        if(!color_wheel){
            printf("Error! Could not load up image file ./data/color_wheel2.png");
        }
	osg::Image * shapesImg = osgDB::readImageFile("./data/shapes.png");
		if(!shapesImg){
            printf("Error! Could not load up image file ./data/color_wheel.png");
        }

	wheel_texture = new osg::Texture2D;
	wheel_texture->setDataVariance(osg::Object::DYNAMIC);
	wheel_texture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
	wheel_texture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
	wheel_texture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP);
	wheel_texture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP);
	wheel_texture->setImage(color_wheel);

	shape_texture = new osg::Texture2D;
	shape_texture->setDataVariance(osg::Object::DYNAMIC);
	shape_texture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
	shape_texture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
	shape_texture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP);
	shape_texture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP);
	shape_texture->setImage(shapesImg);
}
//--------------------------------------------------------------------------
void InitHydra()
{ 
	if(sixenseInit() == SIXENSE_SUCCESS)
		printf("SIXENSE SDK Initialized . . . \n");
	if(sixenseSetActiveBase(0) == SIXENSE_SUCCESS)
		printf("Base Initialized . . .\n");
	for(int i = 0; i < MAX_KEYS; i++)
	{
		rightKeys[i] = false;
		leftKeys[i] = false;
	}

}
//--------------------------------------------------------------------------
void UpdateHydra()
{
	//get new info on left controller
	if(sixenseGetNewestData(0,&controllerLeft) == SIXENSE_SUCCESS)
	{
		bool leftJoyStickMoving = false;
		double vX = controllerLeft.joystick_x - 0;
		double vY = controllerLeft.joystick_y - 0;
		double magV = sqrt(vX*vX + vY*vY);
		double aX = vX / magV * 2;
		double aY = vY / magV * 2;
		colorPicker->setPosition(osg::Vec3(aX*110,aY*110-10 , 0));
		printf("X:%f Y:%f\n", aX/2, aY/2);
		(controllerLeft.joystick_x == 0.0f && controllerLeft.joystick_y == 0.0f) ? leftJoyStickMoving = false : leftJoyStickMoving = true;

		//if the only the lift stick is moving and no trigger was held
		((leftJoyStickMoving) && (controllerLeft.trigger == 0)) ? colorNodeMask = true : colorNodeMask = false;
		((leftJoyStickMoving) && (controllerLeft.trigger == 1)) ? shapeNodeMask = true : shapeNodeMask = false;
		if(colorNodeMask)
		{
			selectColor(controllerRight.trigger,aX/2,aY/2);
		}
		if(shapeNodeMask)
		{
			selectShape(aX/2, aY/2);
		}
	}
	//get new info on right controller
	if(sixenseGetNewestData(1,&controllerRight) == SIXENSE_SUCCESS)
	{
		//printf("%f %f %f\n",controllerRight.pos[0],controllerRight.pos[1],controllerRight.pos[2]);
		//set rotational matrix
		for(int i = 0; i < 3; i++)
		{
			m[0][i] = 0;//controllerRight.rot_mat[i][0];
			m[1][i] = 0;//controllerRight.rot_mat[i][2];
			m[2][i] = 0;//controllerRight.rot_mat[i][1];
			
		}		
		m[0][0] = 1;
		m[1][1] = 1;
		m[2][2] = 1;
		//set position vector

		m[3][0] = controllerRight.pos[0] / 100 + 8;
		m[3][1] = (-controllerRight.pos[2] / 100 + 5)*2;
		m[3][2] = controllerRight.pos[1] / 200 - 1;

		m[0][3] = 0;
		m[1][3] = 0;
		m[2][3] = 0;
		m[3][3] = 1;

		boxMatrix->set(	m[0][0],m[0][1],m[0][2],m[0][3],
						m[1][0],m[1][1],m[1][2],m[1][3],
						m[2][0],m[2][1],m[2][2],m[2][3],
						m[3][0],m[3][1],m[3][2],m[3][3]);

		boxTransform->setMatrix(*boxMatrix);
		if(!isShapeSet)
			currentTransform->setMatrix(*boxMatrix);
		//handleButtonPress(1);
		
	}
	if(!shapeNodeMask)
	{
		scaleObject();
	}
	if(controllerRight.trigger && !shapeNodeMask && !colorNodeMask && !controllerLeft.trigger && !wasScaling)
		{
			printf("Shape Set\n");
			isShapeSet = true;
			scaleFactor = 1.0;
		}
		else if(wasScaling)
		{
			if(!controllerRight.trigger)
			{
				wasScaling = false;
			}
		}
}
//--------------------------------------------------------------------------
osg::MatrixTransform* createShape(int shape)
{
	osg::Matrixd * shapeMatrix = new osg::Matrixd() ;
	osg::Matrixd* scaleMatrix = new osg::Matrixd();
	scaleMatrix->set(scale[0][0],scale[0][1],scale[0][2],scale[0][3],
						scale[1][0],scale[1][1],scale[1][2],scale[1][3],
						scale[2][0],scale[2][1],scale[2][2],scale[2][3],
						scale[3][0],scale[3][1],scale[3][2],scale[3][3]);
	shapeMatrix->set(	m[0][0],m[0][1],m[0][2],m[0][3],
						m[1][0],m[1][1],m[1][2],m[1][3],
					m[2][0],m[2][1],m[2][2],m[2][3],
						m[3][0],m[3][1],m[3][2],m[3][3]);
	osg::ref_ptr<osg::MatrixTransform> shapeTransform = new osg::MatrixTransform();
	osg::ref_ptr<osg::MatrixTransform> scaleTransform = new osg::MatrixTransform();
	osg::ref_ptr<osg::PositionAttitudeTransform> position = new osg::PositionAttitudeTransform();
	position->setPosition(osg::Vec3d(0,0,-0.098));
	scaleTransform->setMatrix(*scaleMatrix);
	shapeTransform->setMatrix(*shapeMatrix);
	shapeTransform->addChild(position);
	position->addChild(scaleTransform);
	//create new shapes
	switch(shape)
	{
	case 1:
		{
			scaleTransform->addChild(MakeBox(0.4));
			break;
		}
	case 2:
		{
			scaleTransform->addChild(MakeRectangle(1));
			break;
		}
	case 3:
		{
			scaleTransform->addChild(MakeSphere(0.5));
			break;
		}
	case 4:
		{
			scaleTransform->addChild(MakeCone(0.3, 0.5));
			break;
		}
	case 5:
		{
			scaleTransform->addChild(MakeCylinder(0.3,0.5));
			break;
		}
	}
	
	//scaleTransform->addChild(shapeTransform);
	//shapeTransform->addChild(MakeBox(0.4));
	root->addChild(shapeTransform);
	currentScaleTransform = scaleTransform;
	pos = position;
	return shapeTransform;
}
//--------------------------------------------------------------------------
osg::Geode* MakeRectangle(int size)
{
	int rectangleFaces = 6;
	osg::Geode* geode = new osg::Geode();
	osg::Geometry* rectangleGeometry = new osg::Geometry();
	geode->addDrawable(rectangleGeometry);
	osg::Vec3Array* rectangleVerts = new osg::Vec3Array;

	//plane for each z coordinate
	for(int i = 0; i < 2; i++)
	{
		rectangleVerts->push_back(osg::Vec3(-1,.5,-.5 + i));
		rectangleVerts->push_back(osg::Vec3(-1,-.5,-.5 + i));
		rectangleVerts->push_back(osg::Vec3(1,-.5,-.5 + i));
		rectangleVerts->push_back(osg::Vec3(1,.5,-.5 + i));
	}

	rectangleGeometry->setVertexArray(rectangleVerts);
	
	//front
	osg::DrawElementsUInt* front = new osg::DrawElementsUInt(osg::PrimitiveSet::QUADS,0);
	front->push_back(0);
	front->push_back(1);
	front->push_back(2);
	front->push_back(3);
	rectangleGeometry->addPrimitiveSet(front);
	//top
	osg::DrawElementsUInt* top = new osg::DrawElementsUInt(osg::PrimitiveSet::QUADS,0);
	top->push_back(4);
	top->push_back(0);
	top->push_back(3);
	top->push_back(7);
	rectangleGeometry->addPrimitiveSet(top);
	//back
	osg::DrawElementsUInt* back = new osg::DrawElementsUInt(osg::PrimitiveSet::QUADS,0);
	back->push_back(4);
	back->push_back(5);
	back->push_back(6);
	back->push_back(7);
	rectangleGeometry->addPrimitiveSet(back);
	//bottom
	osg::DrawElementsUInt* bottom = new osg::DrawElementsUInt(osg::PrimitiveSet::QUADS,0);
	bottom->push_back(5);
	bottom->push_back(1);
	bottom->push_back(2);
	bottom->push_back(6);
	rectangleGeometry->addPrimitiveSet(bottom);
	//right
	osg::DrawElementsUInt* right = new osg::DrawElementsUInt(osg::PrimitiveSet::QUADS,0);
	right->push_back(3);
	right->push_back(2);
	right->push_back(6);
	right->push_back(7);
	rectangleGeometry->addPrimitiveSet(right);
	//left
	osg::DrawElementsUInt* left = new osg::DrawElementsUInt(osg::PrimitiveSet::QUADS,0);
	left->push_back(0);
	left->push_back(1);
	left->push_back(5);
	left->push_back(4);
	rectangleGeometry->addPrimitiveSet(left);
	osg::StateSet* sset = geode->getOrCreateStateSet();
   sset->setMode( GL_LIGHTING, osg::StateAttribute::ON );

  sset->setAttribute(material);
  geode->setStateSet(sset);
	return geode;
}
//--------------------------------------------------------------------------
osg::Geode* MakeSphere(float radius)
{
   osg::Geode* geode = new osg::Geode();
   osg::Sphere* sphere = new osg::Sphere(osg::Vec3d(0,0,0), radius);
   osg::ShapeDrawable* shapeDrawable = new osg::ShapeDrawable(sphere);
   geode->addDrawable(shapeDrawable);
   currentColor = shapeDrawable;

   osg::StateSet* sset = geode->getOrCreateStateSet();
   sset->setMode( GL_LIGHTING, osg::StateAttribute::ON );

  sset->setAttribute(material);
  geode->setStateSet(sset);
   return geode;
}
//--------------------------------------------------------------------------
osg::Geode* MakeCone(float radius, float height)
{
   osg::Geode* geode = new osg::Geode();
   osg::Cone* cone = new osg::Cone(osg::Vec3d(0,0,0), radius, height);
   osg::ShapeDrawable* shapeDrawable = new osg::ShapeDrawable(cone);
   geode->addDrawable(shapeDrawable);
   currentColor = shapeDrawable;
   osg::StateSet* sset = geode->getOrCreateStateSet();
   sset->setMode( GL_LIGHTING, osg::StateAttribute::ON );

  sset->setAttribute(material);
  geode->setStateSet(sset);
   return geode;
}
//--------------------------------------------------------------------------
osg::Geode* MakeCylinder(float radius, float height)
{
	osg::Geode* geode = new osg::Geode();
	osg::Cylinder* cylinder = new osg::Cylinder(osg::Vec3d(0,0,0), radius, height);
	osg::ShapeDrawable* shapeDrawable = new osg::ShapeDrawable(cylinder);
	geode->addDrawable(shapeDrawable);
	currentColor = shapeDrawable;
	osg::StateSet* sset = geode->getOrCreateStateSet();
   sset->setMode( GL_LIGHTING, osg::StateAttribute::ON );

  sset->setAttribute(material);
  geode->setStateSet(sset);
	return geode;
}
//--------------------------------------------------------------------------
osg::Geode* MakePlane(int size)
{
  osg::Geode* geode = new osg::Geode();
   osg::Geometry* planeGeometry = new osg::Geometry();
   geode->addDrawable(planeGeometry);
   osg::Vec3Array* planeVerts = new osg::Vec3Array;

   for(int x = 0; x < size ; x++)
   {
	   for(int y = 0; y < size; y++)
	   {
		    planeVerts->push_back( osg::Vec3(x, y, 0));
	   }
   }

   planeGeometry->setVertexArray(planeVerts);

   for(int x = 0; x < size - 1; x++)
   {
	   for(int y = 0; y < size - 1; y++)
	   {
		   int newIndex = size*y + x;
		   osg::DrawElementsUInt* square = 
			  new osg::DrawElementsUInt(osg::PrimitiveSet::QUADS, 0);
		   square->push_back(newIndex);
		   square->push_back(newIndex+size);
		   square->push_back(newIndex+size+1);
		   square->push_back(newIndex+1);
		   planeGeometry->addPrimitiveSet(square);
	   }
   }

   osg::Vec4Array* colors = new osg::Vec4Array;
   for(int x = 0; x < size ; x++)
   {
	   for(int y = 0; y < size; y++)
	   {
		    if(y % 2 == 0)
			{
				colors->push_back(osg::Vec4(0.0f, 0.0f, 0.0f, 0.0f) );
			}
			else
			{
				colors->push_back(osg::Vec4(1.0f, 1.0f, 1.0f, 0.5f) );
			}
	   }
   }

   osg::StateSet* sset = geode->getOrCreateStateSet();
   sset->setMode( GL_LIGHTING, osg::StateAttribute::ON );

  sset->setAttribute(material);
  geode->setStateSet(sset);

   planeGeometry->setColorArray(colors);
   planeGeometry->setColorBinding(osg::Geometry::BIND_PER_PRIMITIVE);


   return geode;

}
//--------------------------------------------------------------------------
osg::Geode* MakeBox(float size)
{
	osg::Node* node = new osg::Node();
   osg::Geode* geode = new osg::Geode();
   osg::Box* box = new osg::Box(osg::Vec3d(0,0,0), size);
   osg::ShapeDrawable* shapeDrawable = new osg::ShapeDrawable(box);
   geode->addDrawable(shapeDrawable);
   shapeDrawable->setColor(osg::Vec4(1,0,0,1));
   currentColor = shapeDrawable;
   osg::StateSet* sset = geode->getOrCreateStateSet();
   sset->setMode( GL_LIGHTING, osg::StateAttribute::ON );

  sset->setAttribute(material);
  geode->setStateSet(sset);
   return geode;
}
//--------------------------------------------------------------------------
void    
forcedWireFrameModeOn( osg::Node *srcNode ){

	if( srcNode == NULL )
	return;

	osg::StateSet *state = srcNode->getOrCreateStateSet();

	osg::PolygonMode *polyModeObj;


	polyModeObj = dynamic_cast< osg::PolygonMode* >( state->getAttribute( osg::StateAttribute::POLYGONMODE ));

	if ( !polyModeObj ) {
		polyModeObj = new osg::PolygonMode;
		state->setAttribute( polyModeObj );    
		}

	polyModeObj->setMode(  osg::PolygonMode::FRONT_AND_BACK, osg::PolygonMode::LINE );
}
//--------------------------------------------------------------------------
void    
forcedWireFrameModeOff( osg::Node *srcNode ){

if( srcNode == NULL )
    return;
osg::StateSet *state = srcNode->getOrCreateStateSet();

osg::PolygonMode *polyModeObj;

polyModeObj = dynamic_cast< osg::PolygonMode* >( state->getAttribute( osg::StateAttribute::POLYGONMODE ));

if ( !polyModeObj ) {
    polyModeObj = new osg::PolygonMode;
    state->setAttribute( polyModeObj ); 
   }

polyModeObj->setMode(  osg::PolygonMode::FRONT_AND_BACK, osg::PolygonMode::FILL );

 

 

}  // forceWireFrameModeOff
//--------------------------------------------------------------------------
osg::Node* createColorsHUD(osgText::Text* updateText)
{

    // create the hud. derived from osgHud.cpp
    // adds a set of quads, each in a separate Geode - which can be picked individually
    // eg to be used as a menuing/help system!
    // Can pick texts too!

    osg::Camera* hudCamera = new osg::Camera;
    hudCamera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    hudCamera->setProjectionMatrixAsOrtho2D(0,1280,0,1024);
    hudCamera->setViewMatrix(osg::Matrix::identity());
    hudCamera->setRenderOrder(osg::Camera::POST_RENDER);
    hudCamera->setClearMask(GL_DEPTH_BUFFER_BIT);
    
    std::string timesFont("fonts/times.ttf");
    
    // turn lighting off for the text and disable depth test to ensure its always ontop.
	
		//this is the colorwheel geode
	{
		 //osg::Vec3 position(400.0f,800.0f,0.0f);
		osg::Vec3 position(1280/2 -250 , 1024/2 + 250.0f,0.0f);
        osg::Vec3 dy(0.0f,-500.0f,0.0f);
        osg::Vec3 dx(500.0f,0.0f,0.0f);
        osg::Geode* geode = new osg::Geode();
        osg::StateSet* stateset = geode->getOrCreateStateSet();
		osg::Material *material = new osg::Material();
		material->setEmission(osg::Material::FRONT, osg::Vec4(1, 1, 1, 1.0));
		material->setTransparency(osg::Material::FRONT, .5);
		stateset->ref();
		stateset->setAttribute(material);
		stateset->setTextureAttributeAndModes(0, wheel_texture, osg::StateAttribute::ON);
		stateset->setMode(GL_BLEND, osg::StateAttribute::ON);
		stateset->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
        osg::Geometry *quad=new osg::Geometry;
        stateset->setMode(GL_LIGHTING,osg::StateAttribute::OFF);
        stateset->setMode(GL_DEPTH_TEST,osg::StateAttribute::OFF);
        osg::Vec3Array* vertices = new osg::Vec3Array(4); // 1 quad
        osg::Vec4Array* colors = new osg::Vec4Array;
        (*vertices)[0]=position;
        (*vertices)[1]=position+dx;
        (*vertices)[2]=position+dx+dy;
        (*vertices)[3]=position+dy;
        quad->setVertexArray(vertices);
		osg::Vec2Array* texcoords = new osg::Vec2Array(4);
	   (*texcoords)[0].set(0.00f,0.0f); // tex coord for vertex 0 
	   (*texcoords)[1].set(1.0f,0.0f); // tex coord for vertex 1 
	   (*texcoords)[2].set(1.0f,1.0f); // ""
	   (*texcoords)[3].set(0.0f,1.0f); // "" 
	   quad->setTexCoordArray(0,texcoords);
        quad->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::QUADS,0,4));
		geode->addDrawable(quad);
        hudCamera->addChild(geode);
	}
	

	
	{
		//this corners geode
		osg::Vec3 bottemright(0.0f,0.0f,0.0f);
		osg::Vec3 topright(1280.0f,0.0f,0.0f);
		osg::Vec3 topleft(1280.0f,1024.0f,0.0f);
		osg::Vec3 bottemleft(0.0f,1024.0f,0.0f);

        osg::Vec3 yoffset(0.0f, 100.0f,0.0f);
		osg::Vec3 xoffset(100.0f, 0.0f,0.0f);

        osg::Geode* geode = new osg::Geode();
        
		geode->addDrawable(cornersGeo);
        osg::Vec3Array* vertices = new osg::Vec3Array(12); 

        (*vertices)[0]=topleft;
        (*vertices)[1]=topleft-yoffset;
        (*vertices)[2]=topleft-xoffset;

		(*vertices)[3]=topright;
		(*vertices)[5]=topright+yoffset;
		(*vertices)[4]=topright-xoffset;
        
        

		(*vertices)[6]=bottemleft;
		(*vertices)[8]=bottemleft-yoffset;
		(*vertices)[7]=bottemleft+xoffset;
        
        

		(*vertices)[9]=bottemright;
        (*vertices)[10]=bottemright+yoffset;
        (*vertices)[11]=bottemright+xoffset;

		cornersGeo->setVertexArray(vertices);

		for(int y = 0; y < 4; y++)
		{
			osg::DrawElementsUInt* tri = 
				new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES, 0);
			
			tri->push_back(y*3);
			tri->push_back(y*3+1);
			tri->push_back(y*3+2);
			
			cornersGeo->addPrimitiveSet(tri);
		}

		osg::Vec4Array* colors = new osg::Vec4Array;
		for(int y = 0; y < 12; y++)
		{
			colors->push_back(osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f) );
		}

		cornersGeo->setColorArray(colors);
		cornersGeo->setColorBinding(osg::Geometry::BIND_PER_PRIMITIVE);

        
		

        hudCamera->addChild(geode);
	}
	
    osg::PositionAttitudeTransform *  center = new osg::PositionAttitudeTransform();
	center->setPosition(osg::Vec3(1280/2, 1040/2, 0));
    
    hudCamera->addChild(center);
	center->addChild(colorPicker);
	colorPicker->addChild(MakeBox(20));
    
    return hudCamera;

}

osg::Node* createShapesHUD()
{

    osg::Camera* hudCamera = new osg::Camera;
    hudCamera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    hudCamera->setProjectionMatrixAsOrtho2D(0,1280,0,1024);
    hudCamera->setViewMatrix(osg::Matrix::identity());
    hudCamera->setRenderOrder(osg::Camera::POST_RENDER);
    hudCamera->setClearMask(GL_DEPTH_BUFFER_BIT);
    
    // turn lighting off for the text and disable depth test to ensure its always ontop.
	
		//this is the colorwheel geode
	{
		 //osg::Vec3 position(400.0f,800.0f,0.0f);
		osg::Vec3 position(1280/2 -250 , 1024/2 + 250.0f,0.0f);
        osg::Vec3 dy(0.0f,-500.0f,0.0f);
        osg::Vec3 dx(500.0f,0.0f,0.0f);
        osg::Geode* geode = new osg::Geode();
        osg::StateSet* stateset = geode->getOrCreateStateSet();
		osg::Material *material = new osg::Material();
		material->setEmission(osg::Material::FRONT, osg::Vec4(1, 1, 1, 1.0));
		material->setTransparency(osg::Material::FRONT, 0);
		stateset->ref();
		stateset->setAttribute(material);
		stateset->setTextureAttributeAndModes(0, shape_texture, osg::StateAttribute::ON);
		stateset->setMode(GL_BLEND, osg::StateAttribute::ON);
		stateset->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
        osg::Geometry *quad=new osg::Geometry;
        stateset->setMode(GL_LIGHTING,osg::StateAttribute::OFF);
        stateset->setMode(GL_DEPTH_TEST,osg::StateAttribute::OFF);
        osg::Vec3Array* vertices = new osg::Vec3Array(4); // 1 quad
        osg::Vec4Array* colors = new osg::Vec4Array;
        (*vertices)[0]=position;
        (*vertices)[1]=position+dx;
        (*vertices)[2]=position+dx+dy;
        (*vertices)[3]=position+dy;
        quad->setVertexArray(vertices);
		osg::Vec2Array* texcoords = new osg::Vec2Array(4);
	   (*texcoords)[0].set(0.00f,0.0f); // tex coord for vertex 0 
	   (*texcoords)[1].set(1.0f,0.0f); // tex coord for vertex 1 
	   (*texcoords)[2].set(1.0f,1.0f); // ""
	   (*texcoords)[3].set(0.0f,1.0f); // "" 
	   quad->setTexCoordArray(0,texcoords);
        quad->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::QUADS,0,4));
		geode->addDrawable(quad);
        hudCamera->addChild(geode);
	}
	

    osg::PositionAttitudeTransform *  center = new osg::PositionAttitudeTransform();
	center->setPosition(osg::Vec3(1280/2, 1040/2, 0));
    
    hudCamera->addChild(center);
	center->addChild(colorPicker);
	colorPicker->addChild(MakeBox(20));
    
    return hudCamera;

}


//--------------------------------------------------------------------------
//make circles
//--------------------------------------------------------------------------
osg::Vec3Array*
circleVerts( int plane, int approx, float radius )
{
    const double angle( osg::PI * 2. / (double) approx );
    osg::Vec3Array* v = new osg::Vec3Array;
    int idx, count;
    double x(0.), y(0.), z(0.);
    double height;
    double original_radius = radius;

    for(count = 0; count <= approx/4; count++)
    {
        height = original_radius*sin(count*angle);
        radius = cos(count*angle)*radius;


    switch(plane)
    {
        case 0: // X
            x = height;
            break;
        case 1: //Y
            y = height;
            break;
        case 2: //Z
            z = height;
            break;
    }



    for( idx=0; idx<approx; idx++)
    {
        double cosAngle = cos(idx*angle);
        double sinAngle = sin(idx*angle);
        switch (plane) {
            case 0: // X
                y = radius*cosAngle;
                z = radius*sinAngle;
                break;
            case 1: // Y
                x = radius*cosAngle;
                z = radius*sinAngle;
                break;
            case 2: // Z
                x = radius*cosAngle;
                y = radius*sinAngle;
                break;
        }
        v->push_back( osg::Vec3( x, y, z ) );
    }
    }
    return v;
}
//--------------------------------------------------------------------------
osg::Geode*
circles( int plane, int approx, float radius )
{
    osg::Geode* geode = new osg::Geode;
    osg::LineWidth* lw = new osg::LineWidth( 3. );
    geode->getOrCreateStateSet()->setAttributeAndModes( lw,
osg::StateAttribute::ON );


    osg::Geometry* geom = new osg::Geometry;
    osg::Vec3Array* v = circleVerts( plane, approx, radius );
    geom->setVertexArray( v );

    osg::Vec4Array* c = new osg::Vec4Array;
    c->push_back( osg::Vec4( 1., 1., 1., 1. ) );
    geom->setColorArray( c );
    geom->setColorBinding( osg::Geometry::BIND_OVERALL );
    geom->addPrimitiveSet( new osg::DrawArrays( GL_LINE_LOOP, 0, approx ) );

    geode->addDrawable( geom );

    return geode;
}
//--------------------------------------------------------------------------
bool onButtonRelease(int controller, int button)
{
	int index = 0;
	switch(button)
	{
		case SIXENSE_BUTTON_START: index = 0; break;
		case SIXENSE_BUTTON_1: index = 1; break;
		case SIXENSE_BUTTON_2: index = 2; break;
		case SIXENSE_BUTTON_3: index = 3; break;
		case SIXENSE_BUTTON_4: index = 4; break;
		case SIXENSE_BUTTON_BUMPER: index = 5;break;
		default:
			index = 7;
	}
	switch(controller)
	{
	case 1:
		{
		if((controllerRight.buttons & button) && !rightKeys[index])
		{
			rightKeys[index] = true;
			return false;
		}
		else if(!(controllerRight.buttons & button) && rightKeys[index])
		{
			rightKeys[index] = false;
			return true;
		}
	}
	default:
		return false;
	}
}
//--------------------------------------------------------------------------
bool triggerRelease(int controller)
{
	if((controllerRight.trigger == 1) && !rightTrigger)
	{
		rightTrigger = true;
		return false;
	}
	else if((controllerRight.trigger == 0) && rightTrigger)
	{
		rightTrigger = false;
		return true;
	}
	return false;
}
//--------------------------------------------------------------------------
void scaleObject()
{
	if(controllerRight.trigger == 1 && controllerLeft.trigger == 1 && !isShapeSet)
	{
		wasScaling = true;
		if(scaleButtonsPressed)
		{
			float right = controllerRight.pos[0];
			float left = controllerLeft.pos[0];
			if(left >= 0.0)
				initialDistance = abs(right) - abs(left);
			else
				initialDistance = abs(right) + abs(left);
			scaleButtonsPressed = false;
		}
		float distance = 0.0;
		float right = controllerRight.pos[0];
		float left = controllerLeft.pos[0];
		if(left >= 0.0)
			distance = abs(right) - abs(left);
		else
			distance = abs(right) + abs(left);
		//the change in the first distance to the dragging distances
		float deltaDistance = initialDistance - distance;
		//float distance = abs(controllerRight.pos[0]) - abs(controllerLeft.pos[0]);
		//printf("Distance %f\n", abs(distance/100));
		//printf("Y:%f Y:%f\n",controllerRight.pos[1], controllerLeft.pos[1]);
		
		//scaleFactor = oldScaleFactor + (deltaDistance/77);
		scaleFactor = oldScaleFactor - (deltaDistance/77);
		//printf("%f Scale Factor : %f %f \n", deltaDistance, distance, initialDistance);
		//printf("ScaleFactor : %f\n", scaleFactor);
		osg::Matrixd *scaleMatrix = new osg::Matrixd();

		scaleMatrix->set(scaleFactor,scale[0][1],scale[0][2],scale[0][3],
						scale[1][0],scaleFactor,scale[1][2],scale[1][3],
						scale[2][0],scale[2][1],1,scale[2][3],
						scale[3][0],scale[3][1],scale[3][2],scale[3][3]);
		currentScaleTransform->setMatrix(*scaleMatrix);
		
	}
	else
	{
		scaleButtonsPressed = true;
		//save the value of the current scale factor.
		oldScaleFactor = scaleFactor;
	}
}
//--------------------------------------------------------------------------
void selectColor(bool isChooseColor, float aX, float aY)
{
	if(isChooseColor)
	{
		//choose quadrant
		//printf("%f %f\n", aX, aY);//quad2 = true;
		float x = aX;
		float y = aY;
		float redpart = 0;
		float bluepart = 0;
		float greenpart = 0;
		if(aY > .5)
		{
			redpart = 0;
		}
		else
		{
			redpart = (.5 - aY) * 2.0/3;
		}
		
		if(x > 0 && y > 0)
		{
			if(y < .5)
			{
				greenpart = (aY + 1) * 2.0/3;
			}
			else
			{
				greenpart = -aY + 1.5;
			}
			if(y > .5)
			{
				bluepart = (aY-.5);// * 2/3;
			}
			else
			{
				bluepart = 0;
			}
			//currentColor->setColor(osg::Vec4(redpart,greenpart,bluepart,1));
			//printf("%f %f %f \n", redpart,(abs(x)+1)/2,y/2,1);// aY, aX);
			//printf("%f %f\n", aX, aY);//quad2 = true;
			//printf("Quad1\n");//quad1 = true;
		}
		else if(x < 0 && y > 0)
		{
			/*
			if(y < 0.25)
				currentColor->setColor(osg::Vec4((abs(y)+0.5)/2, y/2, (abs(x)+1)/1.5,1));
			else
			*/
			if(y < .5)
			{
				bluepart = (aY + 1) * 2.0/3;
			}
			else
			{
				bluepart = -aY + 1.5;
			}
			if(y > .5)
			{
				greenpart = (aY - .5);//* 2/3;
			}
			else
			{
				greenpart = 0;
			}
			//currentColor->setColor(osg::Vec4(redpart,greenpart,bluepart,1));
			//printf("Quad2\n");//quad2 = true;
			//printf("%f %f %f\n", (abs(x)+0.5)/3,y/2,(abs(x)+1)/2,1);//quad2 = true;
		}
		else if(x < 0 && y < 0)
		{
			bluepart = (aY + 1) * 2.0/3;
			//currentColor->setColor(osg::Vec4(redpart,greenpart,bluepart,1));
			//printf("Quad3\n");//quad3 = true;
		}
		else if(x > 0 && y < 0)
		{
			greenpart = (aY + 1) * 2.0/3;
			//currentColor->setColor(osg::Vec4(redpart,greenpart,bluepart,1));
			//printf("Quad4\n");//quad4 = true;
		}
		else if(x == 0 && y == 1)
		{
			redpart = 0;
			bluepart = .5;
			greenpart = .5;
		}
		else if(x == 0 && y == -1)
		{
			redpart = 1;
			bluepart = 0;
			greenpart = 0;
		}
		else if(x == 1 && y == 0)
		{
			redpart = 1.0/3.0;
			bluepart = 0;
			greenpart = 2.0/3.0;
		}
		else if(x == -1 && y == 0)
		{
			redpart = 1.0/3;
			bluepart = 2.0/3;
			greenpart = 0;
		}
		
		//osg::Vec4Array* colors = new osg::Vec4Array;
		colors = new osg::Vec4Array;
		for(int y = 0; y < 12; y++)
		{
			colors->push_back(osg::Vec4(redpart,greenpart,bluepart, 1.0f) );
		}

		cornersGeo->setColorArray(colors);
		currentColor->setColor(osg::Vec4(redpart,greenpart,bluepart,1));
		//printf("%f %f r = %f, b = %f, g = %f \n", aX, aY, redpart, bluepart, greenpart);//quad2 = true;
	}
}

//--------------------------------------------------------------------------
void removeCurrentShape()
{
	root->removeChild(currentTransform);
	currentTransform = nullptr;
	currentScaleTransform = nullptr;
}
//--------------------------------------------------------------------------
void selectShape(float aX, float aY)
{
	float x = aX;
	float y = aY;
	//select Square
	if((x >= -0.3 && x <= 0.3) && y >= 0.95)
	{
		if(triggerRelease(1))
		{
			if(!isShapeSet)
				removeCurrentShape();
			currentTransform = createShape(SQUARE);
			isShapeSet = false;
			scaleFactor = 1.0;
		}
	}
	//select Cone
	else if((x <= 1.0 && x >= 0.8) && (y >= 0.0 && y <= 0.5))
	{
		if(triggerRelease(1))
		{
			if(!isShapeSet)
				removeCurrentShape();
			currentTransform = createShape(CONE);
			isShapeSet = false;
			scaleFactor = 1.0;
		}
	}
	//select Sphere
	else if((x <= 0.85 && x >= 0.37) && (y <= -0.5 && y >= -0.92))
	{
		if(triggerRelease(1))
		{
			if(!isShapeSet)
				removeCurrentShape();
			currentTransform = createShape(SPHERE);
			isShapeSet = false;
			scaleFactor = 1.0;
		}
	}
	//select Cylinder
	else if((x >= -0.68 && x <= -0.19) && (y <= -0.75 && y >= -0.98))
	{
		if(triggerRelease(1))
		{
			if(!isShapeSet)
				removeCurrentShape();
			currentTransform = createShape(CYLINDER);
			isShapeSet = false;
			scaleFactor = 1.0;
		}
	}
	else if((x >= -1.0 && x <= -0.89) && (y >= 0 && y <= 0.45))
	{
		if(triggerRelease(1))
		{
			if(!isShapeSet)
				removeCurrentShape();
			currentTransform = createShape(RECTANGLE);
			isShapeSet = false;
			scaleFactor = 1.0;
		}
	}
}
//--------------------------------------------------------------------------
void handleButtonPress(int which)
{
	if(which == RIGHT)
	{
		if(onButtonRelease(1,SIXENSE_BUTTON_1))
		{
			/*
			if(shapeCreated && !isShapeSet)
				en.AddObject(pos);
				*/
			printf("Shape Created\n");
			currentTransform = createShape(1);
			isShapeSet = false;
			shapeCreated = true;
		}
		else if(onButtonRelease(1,SIXENSE_BUTTON_2))
		{
			printf("Button 2 Released\n");
			currentTransform = createShape(2);
			isShapeSet = false;
		}
		else if(onButtonRelease(1,SIXENSE_BUTTON_3))
		{
			printf("Button 2 Released\n");
			currentTransform = createShape(3);
			isShapeSet = false;
		}
		else if(onButtonRelease(1,SIXENSE_BUTTON_BUMPER))
		{
			printf("Shape Set\n");
			isShapeSet = true;
			scaleFactor = 1.0;
		}
		
	}
}
//--------------------------------------------------------------------------
int main()
{
	InitHydra();
	initTexture();
	//initialize scale matrix
	for(int i = 0; i < 4; i++)
	{
		for(int j = 0; j < 4; j++)
		{
			scale[i][j] = 0;
		}
	}
	for(int i = 0; i < 4; i++)
	{
		scale[i][i] = 1;
	}

	   
	  material->setColorMode(osg::Material::SPECULAR);
	  material->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4f(.1,.1,.1,1));
	  material->setColorMode(osg::Material::AMBIENT);
	  material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4f(1,1,1, 1));

	osg::Light* light = new osg::Light();
	osg::LightSource * lightsource = new osg::LightSource();
	lightsource->setLight(light);

    osgViewer::Viewer viewer;
	osgText::Text * t = new osgText::Text();
	t->setText("asdf");
	osg::Node* hud = createColorsHUD(t);
	osg::Node* shapes_hud = createShapesHUD();
    osg::Geode* plane = MakePlane(20);
	//forcedWireFrameModeOn(plane);

	boxTransform->setMatrix(*boxMatrix);
    //root->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
	root->addChild(boxTransform);
	boxTransform->addChild(MakeBox(0.1));
	root->addChild(plane);
	root->addChild(hud);
	root->addChild(shapes_hud);
	root->addChild(lightsource);
	osg::StateSet * stateset = root->getOrCreateStateSet();
	lightsource->setStateSetModes(*stateset, osg::StateAttribute::ON);
	light->setAmbient(osg::Vec4d(1.0, 1.0, 1.0, 1.0));
	light->setDiffuse(osg::Vec4d(.5, .5, .5, 1.0));
	light->setSpecular(osg::Vec4d(1.0, 1.0, 1.0, 1.0));
	light->setPosition(osg::Vec4d(0.0, 0.0, 10.0, 1.0));

	numRootChildren = 4;
    viewer.setSceneData( root );

    viewer.setCameraManipulator(new osgGA::TrackballManipulator());
	//viewer.setUpViewInWindow(0, 0, 1280, 720); 
    viewer.realize();



    while( !viewer.done() )
    {
		UpdateHydra();
		//en.Update();
        viewer.frame();
		hud->setNodeMask(colorNodeMask ? ~0 : 0);
		shapes_hud->setNodeMask(shapeNodeMask ? ~0 : 0);
    }

	sixenseExit();
    return 0;
}

///////////////////////////////////////////////////////////////////////////

static const char* vertSource = {
"#version 120\n"
"#extension GL_EXT_geometry_shader4 : enable\n"
"uniform float u_anim1;\n"
"varying vec4 v_color;\n"
"void main(void)\n"
"{\n"
"    v_color = gl_Vertex;\n"
"    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
"}\n"
};

static const char* geomSource = {
"#version 120\n"
"#extension GL_EXT_geometry_shader4 : enable\n"
"uniform float u_anim1;\n"
"varying in vec4 v_color[];\n"
"varying out vec4 v_color_out;\n"
"void main(void)\n"
"{\n"
"    vec4 v = gl_PositionIn[0];\n"
"    v_color_out = v + v_color[0];\n"
"\n"
"    gl_Position = v + vec4(u_anim1,0.,0.,0.);  EmitVertex();\n"
"    gl_Position = v - vec4(u_anim1,0.,0.,0.);  EmitVertex();\n"
"    EndPrimitive();\n"
"\n"
"    gl_Position = v + vec4(0.,1.0-u_anim1,0.,0.);  EmitVertex();\n"
"    gl_Position = v - vec4(0.,1.0-u_anim1,0.,0.);  EmitVertex();\n"
"    EndPrimitive();\n"
"}\n"
};


static const char* fragSource = {
"#version 120\n"
"#extension GL_EXT_geometry_shader4 : enable\n"
"uniform float u_anim1;\n"
"varying vec4 v_color_out;\n"
"void main(void)\n"
"{\n"
"    gl_FragColor = v_color_out;\n"
"}\n"
};

osg::Program* createShader()
{
    osg::Program* pgm = new osg::Program;
    pgm->setName( "osgshader2 demo" );

    pgm->addShader( new osg::Shader( osg::Shader::VERTEX,   vertSource ) );
    pgm->addShader( new osg::Shader( osg::Shader::FRAGMENT, fragSource ) );

    pgm->addShader( new osg::Shader( osg::Shader::GEOMETRY, geomSource ) );
    pgm->setParameter( GL_GEOMETRY_VERTICES_OUT_EXT, 4 );
    pgm->setParameter( GL_GEOMETRY_INPUT_TYPE_EXT, GL_POINTS );
    pgm->setParameter( GL_GEOMETRY_OUTPUT_TYPE_EXT, GL_LINE_STRIP );


    return pgm;
}



