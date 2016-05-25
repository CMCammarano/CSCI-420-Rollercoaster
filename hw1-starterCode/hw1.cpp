/*
	CSCI 420 Computer Graphics, USC
	Assignment 2: Splines
	C++ starter code

	Student username: cammaran
*/

#include <iostream>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>

#include "openGLHeader.h"
#include "glutHeader.h"
#include "openGLHeader.h"
#include "imageIO.h"
#include "openGLMatrix.h"
#include "basicPipelineProgram.h"
#include "splinePipelineProgram.h"

#ifdef WIN32
	#ifdef _DEBUG
		#pragma comment(lib, "glew32d.lib")
	#else
		#pragma comment(lib, "glew32.lib")
	#endif
#endif

#ifdef WIN32
	char shaderBasePath[1024] = SHADER_BASE_PATH;
#else
	char shaderBasePath[96] = "../openGLHelper";
#endif

using namespace std;

/************************************************************/
// STRUCTS
/************************************************************/
// Represents one control point along the spline 
struct Point {
	double x;
	double y;
	double z;
};

struct FPoint {
	float x;
	float y;
	float z;
};

// Spline struct -- contains how many control points the spline has, and an array of control points 
struct Spline {
	int numControlPoints;
	Point* points;
};

/************************************************************/
// GLOBAL VARIABLES
/************************************************************/
// Input
int mousePos[2];									// x,y coordinate of the mouse position
int leftMouseButton = 0;							// 1 if pressed, 0 if not 
int middleMouseButton = 0;							// 1 if pressed, 0 if not
int rightMouseButton = 0;							// 1 if pressed, 0 if not

// Control State
typedef enum { ROTATE, TRANSLATE, SCALE } CONTROL_STATE;
CONTROL_STATE controlState = ROTATE;

// Application state variables
float landRotate[3] = { 0.0f, 0.0f, 0.0f };
float landTranslate[3] = { 0.0f, 0.0f, 0.0f };
float landScale[3] = { 1.0f, 1.0f, 1.0f };
int windowWidth = 1280;
int windowHeight = 720;
char windowTitle[512] = "CSCI 420 Homework II: Attack of the Splines";

// Pipeline Program
BasicPipelineProgram g_pipeline;
GLint g_programID;

SplinePipelineProgram g_splinePipeline;
GLint g_splineProgramID;

// ModelView and Projection Matrices
OpenGLMatrix g_matrices;
float mv[16];
float proj[16];
const float FOV = 45;

// Terrain data
std::vector<GLfloat> g_terrainVertices;
std::vector<GLfloat> g_terrainUVs;

GLuint g_terrainVAO;
GLuint g_terrainVerticesVBO;
GLuint g_terrainUVsVBO;
GLuint g_terrainTexture;

// Skyboxe
std::vector<GLfloat> g_skyboxVertices;
std::vector<GLfloat> g_skyboxUVs;

GLuint g_skyboxVAO;
GLuint g_skyboxVerticesVBO;
GLuint g_skyboxUVsVBO;
GLuint g_skyboxTexture;

// Spline data 
Spline* g_splines;
int g_numSplines;
std::vector<FPoint> g_splinePoints;

// Track data
std::vector<GLfloat> g_trackVertices;
std::vector<GLfloat> g_trackColors;

GLuint g_trackVAO;
GLuint g_trackVerticesVBO;
GLuint g_trackColorsVBO;

// Spline helpers
static const float S_VALUE = 0.5f;

// Screenshot counter
const int MAXIMUM_SCREENSHOTS = 1001;
int g_screenshotCounter = 0;
bool g_takeScreenshots = false;

// Camera steps
int g_cameraSteps = 0;

// Misc Helpful Constants
const float TRANSLATION_MODIFIER = 1.0f;
const float ROTATION_ANGLE_THETA = 0.05f;
const float SCALE_MODIFIER = 0.01f;

/************************************************************/
// CAMERA COMPUTATION FUNCTIONS
/************************************************************/
FPoint computeNormal (const FPoint& unitTangent) {

	// Compute the normal between two vectors -- used the world y axis to get one axis
	//(a2b3−a3b2)i−(a1b3−a3b1)j+(a1b2−a2b1)k
	FPoint normal;
	normal.x = (unitTangent.y * 0 - unitTangent.z * 1);
	normal.y = (unitTangent.x * 0 - unitTangent.z * 0);
	normal.z = (unitTangent.x * 1 - unitTangent.y * 0);
	
	// Compute the binormal by crossing the normal with the tangent
	FPoint binormal;
	binormal.x = (normal.y * unitTangent.z - normal.z * unitTangent.y);
	binormal.y = -(normal.x * unitTangent.z - normal.z * unitTangent.x);
	binormal.z = (normal.x * unitTangent.y - normal.y * unitTangent.z);
	
	return binormal;
}

void computeCameraPosition (const FPoint& point0, const FPoint& point1, const FPoint& point2, const FPoint& point3) {

	// Compute the tangent
	const float u = 0.001f;
	FPoint tangent;
	tangent.x = S_VALUE * (
		(-point0.x + point2.x) + 
		(2 * point0.x - 5 * point1.x + 4 * point2.x - point3.x) * (2 * u) + 
		(-point0.x + 3 * point1.x - 3 * point2.x + point3.x) * (3 * u * u)
	);

	tangent.y = S_VALUE * (
		(-point0.y + point2.y) + 
		(2 * point0.y - 5 * point1.y + 4 * point2.y - point3.y) * (2 * u) + 
		(-point0.y + 3 * point1.y - 3 * point2.y + point3.y) * (3 * u * u)
	);

	tangent.z = S_VALUE * (
		(-point0.z + point2.z) + 
		(2 * point0.z - 5 * point1.z + 4 * point2.z - point3.z) * (2 * u) + 
		(-point0.z + 3 * point1.z - 3 * point2.z + point3.z) * (3 * u * u)
	);

	// Compute the unit tangent
	float magnitude = std::sqrt (std::pow (tangent.x, 2) + std::pow (tangent.y, 2) + std::pow (tangent.z, 2));
	FPoint unitTangent;
	unitTangent.x = tangent.x / magnitude;
	unitTangent.y = tangent.y / magnitude;
	unitTangent.z = tangent.z / magnitude;
	
	// Compute the normal
	FPoint normal = computeNormal (unitTangent);

	// Set up our matrices -- start with the ModelView matrix default value
	g_matrices.SetMatrixMode(OpenGLMatrix::ModelView);
	g_matrices.LoadIdentity();									// Set up the model matrix
	g_matrices.LookAt(
		point0.x, point0.y + 0.5f, point0.z,
		point0.x + unitTangent.x, point0.y + unitTangent.y, point0.z + unitTangent.z,
		normal.x, normal.y, normal.z
	);															// Set up the view matrix
	g_matrices.GetMatrix(mv);

	// Send the matrices to the pipeline
	g_pipeline.Bind();
	g_pipeline.SetModelViewMatrix(mv);

	g_splinePipeline.Bind();
	g_splinePipeline.SetModelViewMatrix(mv);
}

// Update the camera position based on the provided index
void computeCameraForRide (int index) {
	FPoint point1 = g_splinePoints[index];
	FPoint point2 = g_splinePoints[index + 1];
	FPoint point3 = g_splinePoints[index + 2];
	FPoint point4 = g_splinePoints[index + 3];
	
	computeCameraPosition (point1, point2, point3, point4);
}

// Reset the camera
void resetCameraForRide () {
	computeCameraForRide (0);
}

/************************************************************/
// HELPER FUNCTIONS
/************************************************************/
int loadSplines(char* argv) {
	char* cName = (char*)malloc(128 * sizeof(char));
	FILE* fileList;
	FILE* fileSpline;
	int iType, i = 0, j, iLength;

	// Load the track file 
	fileList = fopen(argv, "r");
	if (fileList == NULL) {
		printf("can't open file\n");
		exit(1);
	}

	// Stores the number of splines in a global variable 
	fscanf(fileList, "%d", &g_numSplines);
	g_splines = (Spline*)malloc(g_numSplines * sizeof(Spline));

	// Reads through the spline files 
	for (j = 0; j < g_numSplines; j++) {
		i = 0;
		fscanf(fileList, "%s", cName);
		fileSpline = fopen(cName, "r");

		if (fileSpline == NULL) {
			printf("can't open file\n");
			exit(1);
		}

		// Gets length for spline file
		fscanf(fileSpline, "%d %d", &iLength, &iType);

		// Allocate memory for all the points
		g_splines[j].points = (Point*)malloc(iLength * sizeof(Point));
		g_splines[j].numControlPoints = iLength;

		// Saves the data to the struct
		while (fscanf(fileSpline, "%lf %lf %lf", &g_splines[j].points[i].x, &g_splines[j].points[i].y, &g_splines[j].points[i].z) != EOF) {
			i++;
		}
	}

	free(cName);
	return 0;
}

int initTexture(const char* imageFilename, GLuint textureHandle) {
	// Read the texture image
	ImageIO img;
	ImageIO::fileFormatType imgFormat;
	ImageIO::errorType err = img.load(imageFilename, &imgFormat);

	if (err != ImageIO::OK) {
		printf("Loading texture from %s failed.\n", imageFilename);
		return -1;
	}

	// Check that the number of bytes is a multiple of 4
	if (img.getWidth() * img.getBytesPerPixel() % 4) {
		printf("Error (%s): The width*numChannels in the loaded image must be a multiple of 4.\n", imageFilename);
		return -1;
	}

	// Allocate space for an array of pixels
	int width = img.getWidth();
	int height = img.getHeight();
	unsigned char * pixelsRGBA = new unsigned char[4 * width * height]; // we will use 4 bytes per pixel, i.e., RGBA

	// Fill the pixelsRGBA array with the image pixels
	memset(pixelsRGBA, 0, 4 * width * height); // set all bytes to 0
	for (int h = 0; h < height; h++) {
		for (int w = 0; w < width; w++) {
			// Assign some default byte values (for the case where img.getBytesPerPixel() < 4)
			pixelsRGBA[4 * (h * width + w) + 0] = 0; // red
			pixelsRGBA[4 * (h * width + w) + 1] = 0; // green
			pixelsRGBA[4 * (h * width + w) + 2] = 0; // blue
			pixelsRGBA[4 * (h * width + w) + 3] = 255; // alpha channel; fully opaque

			// Set the RGBA channels, based on the loaded image
			int numChannels = img.getBytesPerPixel();

			// Only set as many channels as are available in the loaded image; the rest get the default value
			for (int c = 0; c < numChannels; c++) {
				pixelsRGBA[4 * (h * width + w) + c] = img.getPixel(w, h, c);
			}
		}
	}

	// Bind the texture
	glBindTexture(GL_TEXTURE_2D, textureHandle);

	// Initialize the texture
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelsRGBA);

	// Generate the mipmaps for this texture
	glGenerateMipmap(GL_TEXTURE_2D);

	// Set the texture parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// Query support for anisotropic texture filtering
	GLfloat fLargest;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fLargest);
	printf("Max available anisotropic samples: %f\n", fLargest);

	// Set anisotropic texture filtering
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 0.5f * fLargest);

	// Query for any errors
	GLenum errCode = glGetError();
	if (errCode != 0) {
		printf("Texture initialization error. Error code: %d.\n", errCode);
		return -1;
	}

	// De-allocate the pixel array -- it is no longer needed
	delete[] pixelsRGBA;

	return 0;
}

void generateTerrain () {

	// Generate corners -- Bottom left
	GLfloat bl[3] = { -128, -1, -128 };

	// Generate corners -- Top Left
	GLfloat tl[3] = { -128, -1, 128 };

	// Generate corners -- Top Right
	GLfloat tr[3] = { 128, -1, 128 };

	// Generate corners -- Bottom Right
	GLfloat br[3] = { 128, -1, -128 };

	// Push our coordinates into the vertex buffer as two triangles (clockwise)
	g_terrainVertices.insert(g_terrainVertices.end(), tl, tl + 3);
	g_terrainVertices.insert(g_terrainVertices.end(), tr, tr + 3);
	g_terrainVertices.insert(g_terrainVertices.end(), bl, bl + 3);
	g_terrainVertices.insert(g_terrainVertices.end(), bl, bl + 3);
	g_terrainVertices.insert(g_terrainVertices.end(), tr, tr + 3);
	g_terrainVertices.insert(g_terrainVertices.end(), br, br + 3);

	// Generate corners -- Bottom left
	GLfloat bl_uv[3] = { 0, 0 };

	// Generate corners -- Top Left
	GLfloat tl_uv[3] = { 0, 16 };

	// Generate corners -- Top Right
	GLfloat tr_uv[3] = { 16, 16 };

	// Generate corners -- Bottom Right
	GLfloat br_uv[3] = { 16, 0 };

	// Push our uv data to the buffer
	g_terrainUVs.insert(g_terrainUVs.end(), tl_uv, tl_uv + 2);
	g_terrainUVs.insert(g_terrainUVs.end(), tr_uv, tr_uv + 2);
	g_terrainUVs.insert(g_terrainUVs.end(), bl_uv, bl_uv + 2);
	g_terrainUVs.insert(g_terrainUVs.end(), bl_uv, bl_uv + 2);
	g_terrainUVs.insert(g_terrainUVs.end(), tr_uv, tr_uv + 2);
	g_terrainUVs.insert(g_terrainUVs.end(), br_uv, br_uv + 2);

	// Load our texture
	glGenTextures (1, &g_terrainTexture);
	initTexture ("textures/grnd.jpg", g_terrainTexture);

	// Handle triangle VAO and VBOs
	glGenVertexArrays(1, &g_terrainVAO);
	glBindVertexArray(g_terrainVAO);

	glGenBuffers(1, &g_terrainVerticesVBO);
	glBindBuffer(GL_ARRAY_BUFFER, g_terrainVerticesVBO);
	glBufferData(GL_ARRAY_BUFFER, g_terrainVertices.size() * sizeof(GLfloat), &g_terrainVertices[0], GL_STATIC_DRAW);

	glGenBuffers(1, &g_terrainUVsVBO);
	glBindBuffer(GL_ARRAY_BUFFER, g_terrainUVsVBO);
	glBufferData(GL_ARRAY_BUFFER, g_terrainUVs.size() * sizeof(GLfloat), &g_terrainUVs[0], GL_STATIC_DRAW);
}

void generateSplines () {
	// Now, generate the full list of spline points
	GLfloat color = 1.0f;
	for (int i = 0; i < g_numSplines; i++) {
		for (int j = 0; j < g_splines[i].numControlPoints - 3; j++) {

			// We want floats
			Point point0 = g_splines[i].points[j];
			Point point1 = g_splines[i].points[j + 1];
			Point point2 = g_splines[i].points[j + 2];
			Point point3 = g_splines[i].points[j + 3];

			// Brute force our spline points
			for (float u = 0; u < 1.0f; u += 0.004f) {
				FPoint fpoint;
				fpoint.x = S_VALUE * (
					(2 * point1.x) +
					(-point0.x + point2.x) * u + 
					(2 * point0.x - 5 * point1.x + 4 * point2.x - point3.x) * (u * u) + 
					(-point0.x + 3 * point1.x - 3 * point2.x + point3.x) * (u * u * u)
				);

				fpoint.y = S_VALUE * (
					(2 * point1.y) +
					(-point0.y + point2.y) * u + 
					(2 * point0.y - 5 * point1.y + 4 * point2.y - point3.y) * (u * u) + 
					(-point0.y + 3 * point1.y - 3 * point2.y + point3.y) * (u * u * u)
				);

				fpoint.z = S_VALUE * (
					(2 * point1.z) +
					(-point0.z + point2.z) * u + 
					(2 * point0.z - 5 * point1.z + 4 * point2.z - point3.z) * (u * u) + 
					(-point0.z + 3 * point1.z - 3 * point2.z + point3.z) * (u * u * u)
				);

				g_splinePoints.push_back(fpoint);
			}
		}
	}

	// For every four spline points, compute the triangles for the track
	for (int i = 0; i < g_splinePoints.size() - 3; i++) {
		// Get four points from the list
		FPoint point0 = g_splinePoints[i];
		FPoint point1 = g_splinePoints[i + 1];
		FPoint point2 = g_splinePoints[i + 2];
		FPoint point3 = g_splinePoints[i + 3];

		// Compute the tangent -- solved with C++ calculus!
		const float u = 0.004f;
		FPoint tangent;
		tangent.x = S_VALUE * (
			(-point0.x + point2.x) + 
			(2 * point0.x - 5 * point1.x + 4 * point2.x - point3.x) * (2 * u) + 
			(-point0.x + 3 * point1.x - 3 * point2.x + point3.x) * (3 * u * u)
		);

		tangent.y = S_VALUE * (
			(-point0.y + point2.y) + 
			(2 * point0.y - 5 * point1.y + 4 * point2.y - point3.y) * (2 * u) + 
			(-point0.y + 3 * point1.y - 3 * point2.y + point3.y) * (3 * u * u)
		);

		tangent.z = S_VALUE * (
			(-point0.z + point2.z) + 
			(2 * point0.z - 5 * point1.z + 4 * point2.z - point3.z) * (2 * u) + 
			(-point0.z + 3 * point1.z - 3 * point2.z + point3.z) * (3 * u * u)
		);

		// Compute the unit tangent!
		float magnitude = std::sqrt (std::pow (tangent.x, 2) + std::pow (tangent.y, 2) + std::pow (tangent.z, 2));
		FPoint unitTangent;
		unitTangent.x = tangent.x / magnitude;
		unitTangent.y = tangent.y / magnitude;
		unitTangent.z = tangent.z / magnitude;

		// Compute the normal between two vectors -- used the world y axis to get one axis
		//(a2b3−a3b2)i−(a1b3−a3b1)j+(a1b2−a2b1)k
		FPoint normal;
		normal.x = (unitTangent.y * 0 - unitTangent.z * 1);
		normal.y = (unitTangent.x * 0 - unitTangent.z * 0);
		normal.z = (unitTangent.x * 1 - unitTangent.y * 0);

		// Compute the binormal by crossing the normal with the tangent
		FPoint binormal;
		binormal.x = (normal.y * unitTangent.z - normal.z * unitTangent.y);
		binormal.y = (normal.x * unitTangent.z - normal.z * unitTangent.x);
		binormal.z = (normal.x * unitTangent.y - normal.y * unitTangent.z);

		// Get the array of points for the track rail mesh
		const float scalar = 0.015f;
		GLfloat p[] = {
			// Front face
			// V2
			point0.x + scalar * (normal.x - binormal.x), point0.y + scalar * (normal.y - binormal.y), point0.z + scalar * (normal.z - binormal.z),
			// V1
			point0.x + scalar * (normal.x + binormal.x), point0.y + scalar * (normal.y + binormal.y), point0.z + scalar * (normal.z + binormal.z),
			// V3
			point0.x + scalar * (-normal.x - binormal.x), point0.y + scalar * (-normal.y - binormal.y), point0.z + scalar * (-normal.z - binormal.z),
			// V3
			point0.x + scalar * (-normal.x - binormal.x), point0.y + scalar * (-normal.y - binormal.y), point0.z + scalar * (-normal.z - binormal.z),
			// V1
			point0.x + scalar * (normal.x + binormal.x), point0.y + scalar * (normal.y + binormal.y), point0.z + scalar * (normal.z + binormal.z),
			// V0
			point0.x + scalar * (-normal.x + binormal.x), point0.y + scalar * (-normal.y + binormal.y), point0.z + scalar * (-normal.z + binormal.z),

			// Left face
			// V6
			point3.x + scalar * (normal.x - binormal.x), point3.y + scalar * (normal.y - binormal.y), point3.z + scalar * (normal.z - binormal.z),
			// V2
			point0.x + scalar * (normal.x - binormal.x), point0.y + scalar * (normal.y - binormal.y), point0.z + scalar * (normal.z - binormal.z),
			// V7
			point3.x + scalar * (-normal.x - binormal.x), point3.y + scalar * (-normal.y - binormal.y), point3.z + scalar * (-normal.z - binormal.z),
			// V7
			point3.x + scalar * (-normal.x - binormal.x), point3.y + scalar * (-normal.y - binormal.y), point3.z + scalar * (-normal.z - binormal.z),
			// V2
			point0.x + scalar * (normal.x - binormal.x), point0.y + scalar * (normal.y - binormal.y), point0.z + scalar * (normal.z - binormal.z),
			// V3
			point0.x + scalar * (-normal.x - binormal.x), point0.y + scalar * (-normal.y - binormal.y), point0.z + scalar * (-normal.z - binormal.z),

			// Right face
			// V1
			point0.x + scalar * (normal.x + binormal.x), point0.y + scalar * (normal.y + binormal.y), point0.z + scalar * (normal.z + binormal.z),
			// V5
			point3.x + scalar * (normal.x + binormal.x), point3.y + scalar * (normal.y + binormal.y), point3.z + scalar * (normal.z + binormal.z),
			// V0
			point0.x + scalar * (-normal.x + binormal.x), point0.y + scalar * (-normal.y + binormal.y), point0.z + scalar * (-normal.z + binormal.z),
			// V0
			point0.x + scalar * (-normal.x + binormal.x), point0.y + scalar * (-normal.y + binormal.y), point0.z + scalar * (-normal.z + binormal.z),
			// V5
			point3.x + scalar * (normal.x + binormal.x), point3.y + scalar * (normal.y + binormal.y), point3.z + scalar * (normal.z + binormal.z),
			// V4
			point3.x + scalar * (-normal.x + binormal.x), point3.y + scalar * (-normal.y + binormal.y), point3.z + scalar * (-normal.z + binormal.z),
			
			// Top face
			// V6
			point3.x + scalar * (normal.x - binormal.x), point3.y + scalar * (normal.y - binormal.y), point3.z + scalar * (normal.z - binormal.z),
			// V5
			point3.x + scalar * (normal.x + binormal.x), point3.y + scalar * (normal.y + binormal.y), point3.z + scalar * (normal.z + binormal.z),
			// V2
			point0.x + scalar * (normal.x - binormal.x), point0.y + scalar * (normal.y - binormal.y), point0.z + scalar * (normal.z - binormal.z),
			// V2
			point0.x + scalar * (normal.x - binormal.x), point0.y + scalar * (normal.y - binormal.y), point0.z + scalar * (normal.z - binormal.z),
			// V5
			point3.x + scalar * (normal.x + binormal.x), point3.y + scalar * (normal.y + binormal.y), point3.z + scalar * (normal.z + binormal.z),
			// V1
			point0.x + scalar * (normal.x + binormal.x), point0.y + scalar * (normal.y + binormal.y), point0.z + scalar * (normal.z + binormal.z),
			
			// Bottom face
			// V7
			point3.x + scalar * (-normal.x - binormal.x), point3.y + scalar * (-normal.y - binormal.y), point3.z + scalar * (-normal.z - binormal.z),
			// V4
			point3.x + scalar * (-normal.x + binormal.x), point3.y + scalar * (-normal.y + binormal.y), point3.z + scalar * (-normal.z + binormal.z),
			// V3
			point0.x + scalar * (-normal.x - binormal.x), point0.y + scalar * (-normal.y - binormal.y), point0.z + scalar * (-normal.z - binormal.z),
			// V3
			point0.x + scalar * (-normal.x - binormal.x), point0.y + scalar * (-normal.y - binormal.y), point0.z + scalar * (-normal.z - binormal.z),
			// V4
			point3.x + scalar * (-normal.x + binormal.x), point3.y + scalar * (-normal.y + binormal.y), point3.z + scalar * (-normal.z + binormal.z),
			// V0
			point0.x + scalar * (-normal.x + binormal.x), point0.y + scalar * (-normal.y + binormal.y), point0.z + scalar * (-normal.z + binormal.z),

			// Back face
			// V6
			point3.x + scalar * (normal.x - binormal.x), point3.y + scalar * (normal.y - binormal.y), point3.z + scalar * (normal.z - binormal.z),
			// V5
			point3.x + scalar * (normal.x + binormal.x), point3.y + scalar * (normal.y + binormal.y), point3.z + scalar * (normal.z + binormal.z),
			// V7
			point3.x + scalar * (-normal.x - binormal.x), point3.y + scalar * (-normal.y - binormal.y), point3.z + scalar * (-normal.z - binormal.z),
			// V7
			point3.x + scalar * (-normal.x - binormal.x), point3.y + scalar * (-normal.y - binormal.y), point3.z + scalar * (-normal.z - binormal.z),
			// V5
			point3.x + scalar * (normal.x + binormal.x), point3.y + scalar * (normal.y + binormal.y), point3.z + scalar * (normal.z + binormal.z),
			// V4
			point3.x + scalar * (-normal.x + binormal.x), point3.y + scalar * (-normal.y + binormal.y), point3.z + scalar * (-normal.z + binormal.z),
		};

		g_trackVertices.insert (g_trackVertices.end(), p, p + 108);

		// Add the colors -- the mesh is one solid color
		for (int j = 0; j < 36; j++) {
			float color = 0.5f;
			GLfloat colors[] = { color, color, color, color };
			g_trackColors.insert (g_trackColors.end(), colors, colors + 4);
		}
	}

	// Handle the track VAO and VBOs
	glGenVertexArrays(1, &g_trackVAO);
	glBindVertexArray(g_trackVAO);

	glGenBuffers(1, &g_trackVerticesVBO);
	glBindBuffer(GL_ARRAY_BUFFER, g_trackVerticesVBO);
	glBufferData(GL_ARRAY_BUFFER, g_trackVertices.size() * sizeof(FPoint), &g_trackVertices[0], GL_STATIC_DRAW);

	glGenBuffers(1, &g_trackColorsVBO);
	glBindBuffer(GL_ARRAY_BUFFER, g_trackColorsVBO);
	glBufferData(GL_ARRAY_BUFFER, g_trackColors.size() * sizeof(GLfloat), &g_trackColors[0], GL_STATIC_DRAW);
}

void generateSkybox () {

	{
		// Generate corners -- Bottom left
		GLfloat bl[3] = { -128, -128, -128 };

		// Generate corners -- Top Left
		GLfloat tl[3] = { -128, 128, -128 };

		// Generate corners -- Top Right
		GLfloat tr[3] = { -128, 128, 128 };

		// Generate corners -- Bottom Right
		GLfloat br[3] = { -128, -128, 128 };

		// Push our coordinates into the vertex buffer as two triangles (clockwise)
		g_skyboxVertices.insert(g_skyboxVertices.end(), tl, tl + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), tr, tr + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), bl, bl + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), bl, bl + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), tr, tr + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), br, br + 3);
	}

	{
		// Generate corners -- Bottom left
		GLfloat bl[3] = { -128, -128, 128 };

		// Generate corners -- Top Left
		GLfloat tl[3] = { -128, 128, 128 };

		// Generate corners -- Top Right
		GLfloat tr[3] = { 128, 128, 128 };

		// Generate corners -- Bottom Right
		GLfloat br[3] = { 128, -128, 128 };

		// Push our coordinates into the vertex buffer as two triangles (clockwise)
		g_skyboxVertices.insert(g_skyboxVertices.end(), tl, tl + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), tr, tr + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), bl, bl + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), bl, bl + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), tr, tr + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), br, br + 3);
	}

	{
		// Generate corners -- Bottom left
		GLfloat bl[3] = { 128, -128, 128 };

		// Generate corners -- Top Left
		GLfloat tl[3] = { 128, 128, 128 };

		// Generate corners -- Top Right
		GLfloat tr[3] = { 128, 128, -128 };

		// Generate corners -- Bottom Right
		GLfloat br[3] = { 128, -128, -128 };

		// Push our coordinates into the vertex buffer as two triangles (clockwise)
		g_skyboxVertices.insert(g_skyboxVertices.end(), tl, tl + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), tr, tr + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), bl, bl + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), bl, bl + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), tr, tr + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), br, br + 3);
	}

	{
		// Generate corners -- Bottom left
		GLfloat bl[3] = { -128, -128, -128 };

		// Generate corners -- Top Left
		GLfloat tl[3] = { -128, 128, -128 };

		// Generate corners -- Top Right
		GLfloat tr[3] = { 128, 128, -128 };

		// Generate corners -- Bottom Right
		GLfloat br[3] = { 128, -128, -128 };

		// Push our coordinates into the vertex buffer as two triangles (clockwise)
		g_skyboxVertices.insert(g_skyboxVertices.end(), tl, tl + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), tr, tr + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), bl, bl + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), bl, bl + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), tr, tr + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), br, br + 3);
	}

	{
		// Generate corners -- Bottom left
		GLfloat bl[3] = { -128, 128, -128 };

		// Generate corners -- Top Left
		GLfloat tl[3] = { 128, 128, -128 };

		// Generate corners -- Top Right
		GLfloat tr[3] = { 128, 128, 128 };

		// Generate corners -- Bottom Right
		GLfloat br[3] = { -128, 128, 128 };

		// Push our coordinates into the vertex buffer as two triangles (clockwise)
		g_skyboxVertices.insert(g_skyboxVertices.end(), tl, tl + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), tr, tr + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), bl, bl + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), bl, bl + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), tr, tr + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), br, br + 3);
	}

	{
		// Generate corners -- Bottom left
		GLfloat bl[3] = { -128, -128, -128 };

		// Generate corners -- Top Left
		GLfloat tl[3] = { 128, -128, -128 };

		// Generate corners -- Top Right
		GLfloat tr[3] = { 128, -128, 128 };

		// Generate corners -- Bottom Right
		GLfloat br[3] = { -128, -128, 128 };

		// Push our coordinates into the vertex buffer as two triangles (clockwise)
		g_skyboxVertices.insert(g_skyboxVertices.end(), tl, tl + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), tr, tr + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), bl, bl + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), bl, bl + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), tr, tr + 3);
		g_skyboxVertices.insert(g_skyboxVertices.end(), br, br + 3);
	}

	for (int i = 0; i < 6; i++) {
		// Calculate colors
		GLfloat color = 1.0f;
		GLfloat colorBL[4] = { 0, 0 };
		GLfloat colorTL[4] = { 0, 1 };
		GLfloat colorTR[4] = { 1, 1 };
		GLfloat colorBR[4] = { 1, 0 };

		// Push our color data to the buffer
		g_skyboxUVs.insert(g_skyboxUVs.end(), colorTL, colorTL + 2);
		g_skyboxUVs.insert(g_skyboxUVs.end(), colorTR, colorTR + 2);
		g_skyboxUVs.insert(g_skyboxUVs.end(), colorBL, colorBL + 2);
		g_skyboxUVs.insert(g_skyboxUVs.end(), colorBL, colorBL + 2);
		g_skyboxUVs.insert(g_skyboxUVs.end(), colorTR, colorTR + 2);
		g_skyboxUVs.insert(g_skyboxUVs.end(), colorBR, colorBR + 2);
	}

	// Load our texture
	glGenTextures (1, &g_skyboxTexture);
	initTexture ("textures/sky.jpg", g_skyboxTexture);

	// Handle the skybox VAO and VBOs
	glGenVertexArrays(1, &g_skyboxVAO);
	glBindVertexArray(g_skyboxVAO);

	glGenBuffers(1, &g_skyboxVerticesVBO);
	glBindBuffer(GL_ARRAY_BUFFER, g_skyboxVerticesVBO);
	glBufferData(GL_ARRAY_BUFFER, g_skyboxVertices.size() * sizeof(GLfloat), &g_skyboxVertices[0], GL_STATIC_DRAW);

	glGenBuffers(1, &g_skyboxUVsVBO);
	glBindBuffer(GL_ARRAY_BUFFER, g_skyboxUVsVBO);
	glBufferData(GL_ARRAY_BUFFER, g_skyboxUVs.size() * sizeof(GLfloat), &g_skyboxUVs[0], GL_STATIC_DRAW);
}

void generateMeshes () {

	 // Bind the shaders
	g_pipeline.Bind();

	// First, generate our terrain mesh.
	generateTerrain();
	generateSkybox();

	// Next, generate the track
	g_splinePipeline.Bind();
	generateSplines();
}

/************************************************************/
// SCREENSHOTS
/************************************************************/
void saveScreenshot(const char* filename) {
	unsigned char* screenshotData = new unsigned char[windowWidth * windowHeight * 3];
	glReadPixels(0, 0, windowWidth, windowHeight, GL_RGB, GL_UNSIGNED_BYTE, screenshotData);

	ImageIO screenshotImg(windowWidth, windowHeight, 3, screenshotData);

	if (screenshotImg.save(filename, ImageIO::FORMAT_JPEG) == ImageIO::OK) {
		cout << "File " << filename << " saved successfully." << endl;
	}

	else {
		cout << "Failed to save file " << filename << '.' << endl;
	}

	delete [] screenshotData;
}

/************************************************************/
// GLOBAL CALLBACKS
/************************************************************/
// Render function -- draw in objects, then swap buffers
void displayFunc() {

	// Clear the frame buffer
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Bind the shaders
	g_pipeline.Bind();

	// TODO: Draw the skybox!
	glBindTexture(GL_TEXTURE_2D, g_skyboxTexture);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, g_skyboxVerticesVBO);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

	// Enable our color data
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, g_skyboxUVsVBO);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

	glDrawArrays(GL_TRIANGLES, 0, g_skyboxVertices.size() / 3);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);

	// Draw the terrain! Enable our vertex buffer data
	glBindTexture(GL_TEXTURE_2D, g_terrainTexture);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, g_terrainVerticesVBO);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

	// Enable our texture data
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, g_terrainUVsVBO);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

	glDrawArrays(GL_TRIANGLES, 0, g_terrainVertices.size() / 3);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);

	// Draw the splines! Enable our vertex buffer data
	g_splinePipeline.Bind();
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, g_trackVerticesVBO);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

	// Enable our color data
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, g_trackColorsVBO);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glDrawArrays(GL_TRIANGLES, 0, g_trackVertices.size());

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	// Lastly, swap buffers
	glutSwapBuffers();
}

// Idle function -- used to take screenshots and rotate the object in display mode
void idleFunc() {
	// Animation requirement -- use stringstreams to build our filename
	if (g_screenshotCounter < MAXIMUM_SCREENSHOTS && g_takeScreenshots) {
		std::stringstream ss;
		ss << "../anim/" << g_screenshotCounter << ".jpg";
		std::string name = ss.str();
		saveScreenshot (name.c_str());
		g_screenshotCounter++;
	}

	// Move the camera along the spline track
	if (g_cameraSteps < g_splinePoints.size () - 3) {
		computeCameraForRide (g_cameraSteps);
		g_cameraSteps++;
	}

	// make the screen update 
	glutPostRedisplay();
}

// Reshape function -- used to set up the perspective view matrix
void reshapeFunc(int w, int h) {
	glViewport(0, 0, w, h);

	// Set up our perspective matrix -- this will not change
	g_matrices.SetMatrixMode (OpenGLMatrix::Projection);
	g_matrices.LoadIdentity ();
	g_matrices.Perspective (FOV, (float)windowWidth / (float)windowHeight, 0.01f, 1000.0f);
	g_matrices.GetMatrix (proj);

	// Send matrix to the pipeline
	g_pipeline.Bind();
	g_pipeline.SetProjectionMatrix (proj);

	g_splinePipeline.Bind();
	g_splinePipeline.SetProjectionMatrix (proj);
}

// Track mouse movement while one of the buttons is pressed.
void mouseMotionDragFunc(int x, int y) {
	// EMPTY FOR NOW
}

void mouseMotionFunc(int x, int y) {
	// mouse has moved
	// store the new mouse position
	mousePos[0] = x;
	mousePos[1] = y;
}

void mouseButtonFunc(int button, int state, int x, int y) {
	// a mouse button has has been pressed or depressed

	// keep track of the mouse button state, in leftMouseButton, middleMouseButton, rightMouseButton variables
	switch (button) {
		case GLUT_LEFT_BUTTON:
			leftMouseButton = (state == GLUT_DOWN);
			break;

		case GLUT_MIDDLE_BUTTON:
			middleMouseButton = (state == GLUT_DOWN);
			break;

		case GLUT_RIGHT_BUTTON:
			rightMouseButton = (state == GLUT_DOWN);
			break;
	}

	// keep track of whether CTRL and SHIFT keys are pressed
	switch (glutGetModifiers()) {
		case GLUT_ACTIVE_CTRL:
			controlState = TRANSLATE;
			break;

		case GLUT_ACTIVE_SHIFT:
			controlState = SCALE;
			break;

			// if CTRL and SHIFT are not pressed, we are in rotate mode
		default:
			controlState = ROTATE;
			break;
	}

	// store the new mouse position
	mousePos[0] = x;
	mousePos[1] = y;
}

void keyboardFunc(unsigned char key, int x, int y) {
	switch (key) {

		// Exit the program
		case 27: // ESC key
			exit(0); 
			break;

		// Start the roller coaster simulation!
		case ' ':
			cout << "You pressed the spacebar." << endl;
			break;

		// Reset the rotation
		case 'r':
			resetCameraForRide();
			g_cameraSteps = 0;
			break;

		// Take a screenshot
		case 'x':
			saveScreenshot("screenshot.jpg");
			break;

		// Start the image animation
		case 'q':
			g_takeScreenshots = true;
			std::cout << "Starting animation!" << std::endl;
			break;
	}
}

// Initialize the scene and all associated values.
void initScene(int argc, char *argv[]) {

	// TODO: Load splines
	loadSplines(argv[1]);

	printf("Loaded %d spline(s).\n", g_numSplines);
	for (int i = 0; i < g_numSplines; i++) {
		printf("Num control points in spline %d: %d.\n", i, g_splines[i].numControlPoints);
	}

	// Set the color to clear -- (black with no alpha)
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	// Load, compile, and initialize the shaders
	if (g_splinePipeline.Init("../openGLHelper-starterCode") != 0) {
		exit(EXIT_FAILURE);
	}
	g_splineProgramID = g_splinePipeline.GetProgramHandle();

	// Load, compile, and initialize the shaders
	if (g_pipeline.Init("../openGLHelper-starterCode") != 0) {
		exit(EXIT_FAILURE);
	}
	g_programID = g_pipeline.GetProgramHandle();

	// Now, load our meshes
	generateMeshes ();

	// Set up our matrices -- start with the ModelView matrix default value
	resetCameraForRide ();

	// Enable depth testing, then prioritize fragments closest to the camera
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	// Enable line smoothing
	glEnable(GL_LINE_SMOOTH);
}

/************************************************************/
// ENTRY DT_POINT FUNCTION
/************************************************************/
int main(int argc, char *argv[]) {

	if (argc < 2) {
		printf ("usage: %s <trackfile>\n", argv[0]);
		exit(0);
	}

	cout << "Initializing GLUT..." << endl;
	glutInit(&argc,argv);

	cout << "Initializing OpenGL..." << endl;

	#ifdef __APPLE__
		glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
	#else
		glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
	#endif

	glutInitWindowSize(windowWidth, windowHeight);
	glutInitWindowPosition(0, 0);	
	glutCreateWindow(windowTitle);

	cout << "OpenGL Version: " << glGetString(GL_VERSION) << endl;
	cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << endl;
	cout << "Shading Language Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;

	// tells glut to use a particular display function to redraw 
	glutDisplayFunc(displayFunc);
	// perform animation inside idleFunc
	glutIdleFunc(idleFunc);
	// callback for mouse drags
	glutMotionFunc(mouseMotionDragFunc);
	// callback for idle mouse movement
	glutPassiveMotionFunc(mouseMotionFunc);
	// callback for mouse button changes
	glutMouseFunc(mouseButtonFunc);
	// callback for resizing the window
	glutReshapeFunc(reshapeFunc);
	// callback for pressing the keys on the keyboard
	glutKeyboardFunc(keyboardFunc);

	// init glew
	#ifdef __APPLE__
		// nothing is needed on Apple
	#else
		// Windows, Linux
		GLint result = glewInit();
		if (result != GLEW_OK) {
			cout << "error: " << glewGetErrorString(result) << endl;
			exit(EXIT_FAILURE);
		}
	#endif

	// do initialization
	initScene(argc, argv);

	// sink forever into the glut loop
	glutMainLoop();
}