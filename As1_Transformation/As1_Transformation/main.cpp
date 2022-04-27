#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include<math.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "textfile.h"

#include "Vectors.h"
#include "Matrices.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#ifndef max
# define max(a,b) (((a)>(b))?(a):(b))
# define min(a,b) (((a)<(b))?(a):(b))
#endif

using namespace std;

// Default window size
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;

const int PI = 3.14159265359;
bool isWireframe = false;
bool mouse_pressed = false;
int starting_press_x = -1;
int starting_press_y = -1;

enum TransMode
{
	GeoTranslation = 0,
	GeoRotation = 1,
	GeoScaling = 2,
	ViewCenter = 3,
	ViewEye = 4,
	ViewUp = 5,
};

GLint iLocMVP;

vector<string> filenames; // .obj filename list

struct model
{
	Vector3 position = Vector3(0, 0, 0);
	Vector3 scale = Vector3(1, 1, 1);
	Vector3 rotation = Vector3(0, 0, 0);	// Euler form
};
vector<model> models;

struct camera
{
	Vector3 position;
	Vector3 center;
	Vector3 up_vector;
};
camera main_camera;

struct project_setting
{
	GLfloat nearClip, farClip;
	GLfloat fovy;
	GLfloat aspect;
	GLfloat left, right, top, bottom;
};
project_setting proj;

enum ProjMode
{
	Orthogonal = 0,
	Perspective = 1,
};
ProjMode cur_proj_mode = Orthogonal;
TransMode cur_trans_mode = GeoTranslation;

Matrix4 view_matrix;
Matrix4 project_matrix;


typedef struct
{
	GLuint vao;
	GLuint vbo;
	GLuint vboTex;
	GLuint ebo;
	GLuint p_color;
	int vertex_count;
	GLuint p_normal;
	int materialId;
	int indexCount;
	GLuint m_texture;
} Shape;
Shape quad;
Shape m_shpae;
vector<Shape> m_shape_list;
int cur_idx = 0; // represent which model should be rendered now


// [TODO] given a translation vector then output a Matrix4 (Translation Matrix)
Matrix4 translate(Vector3 vec)
{
    return Matrix4(
        1, 0, 0, vec.x,
        0, 1, 0, vec.y,
        0, 0, 1, vec.z,
        0, 0, 0, 1
    );
}

// [TODO] given a scaling vector then output a Matrix4 (Scaling Matrix)
Matrix4 scaling(Vector3 vec)
{
	return Matrix4(
            vec.x, 0, 0, 0,
            0, vec.y, 0, 0,
            0, 0, vec.z, 0,
            0, 0, 0, 1
	);
}


// [TODO] given a float value then ouput a rotation matrix alone axis-X (rotate alone axis-X)
Matrix4 rotateX(GLfloat val)
{
    // correct?
	return  Matrix4(
	    1, 0, 0, 0,
            0, cos(val), sin(val), 0,
            0, -sin(val), cos(val), 0,
            0, 0, 0, 1
	);
}

// [TODO] given a float value then ouput a rotation matrix alone axis-Y (rotate alone axis-Y)
Matrix4 rotateY(GLfloat val)
{
    // correct?
	return Matrix4(
	    cos(val), 0, -sin(val), 0,
            0, 1, 0, 0,
            sin(val), 0, cos(val), 0,
            0, 0, 0, 1
	);
}

// [TODO] given a float value then ouput a rotation matrix alone axis-Z (rotate alone axis-Z)
Matrix4 rotateZ(GLfloat val)
{
    // correct ?
	return Matrix4(
	    cos(val), -sin(val), 0, 0,
            sin(val), cos(val), 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
	);
}

Matrix4 rotate(Vector3 vec)
{
	return rotateX(vec.x)*rotateY(vec.y)*rotateZ(vec.z);
}

// [TODO] compute viewing matrix accroding to the setting of main_camera
void setViewingMatrix()
{
    Vector3 p1_p2 = main_camera.center - main_camera.position,
            p1_p3 = main_camera.up_vector,
            new_z = -(p1_p2).normalize(),
            new_x = p1_p2.cross(p1_p3).normalize(),
            new_y = new_z.cross(new_x);

    Matrix4 T = translate(-main_camera.position);
    Matrix4 R = Matrix4(
        new_x[0], new_x[1], new_x[2], 0,
        new_y[0], new_y[1], new_y[2], 0,
        new_z[0], new_z[1], new_z[2], 0,
        0, 0, 0, 1
    );

    view_matrix = R * T;
}

// [TODO] compute orthogonal projection matrix
void setOrthogonal()
{
    cur_proj_mode = Orthogonal;
    auto r_l = proj.right - proj.left;
    auto t_b = proj.top - proj.bottom;
    auto f_n = proj.farClip - proj.nearClip;
    auto tx = -(proj.right + proj.left) / r_l;
    auto ty = -(proj.top + proj.bottom) / t_b;
    auto tz = -(proj.nearClip + proj.farClip) / f_n;
    
    project_matrix = Matrix4(
        2/r_l, 0, 0, tx,
        0, 2/t_b, 0, ty,
        0, 0, -2/f_n, tz,
        0, 0, 0, 1
    );
    
}

// [TODO] compute persepective projection matrix
void setPerspective()
{
	cur_proj_mode = Perspective;

	float f = 1 / (tan(proj.fovy / 2 * PI)); // f = cot(fovy / 2 * PI)
	project_matrix = Matrix4(
	    f / proj.aspect, 0, 0, 0,
	    0, f, 0, 0,
	    0, 0, (proj.farClip + proj.nearClip) / (proj.nearClip - proj.farClip), (2 * proj.farClip * proj.nearClip) / (proj.nearClip - proj.farClip),
	    0, 0, -1, 0
	);
}


// Vertex buffers
GLuint VAO, VBO;

// Call back function for window reshape
void ChangeSize(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	// [TODO] change your aspect ratio
}

void drawPlane()
{
	GLfloat vertices[18]{ 1.0, -0.9, -1.0,
		1.0, -0.9,  1.0,
		-1.0, -0.9, -1.0,
		1.0, -0.9,  1.0,
		-1.0, -0.9,  1.0,
		-1.0, -0.9, -1.0 };

	GLfloat colors[18]{ 0.0,1.0,0.0,
		0.0,0.5,0.8,
		0.0,1.0,0.0,
		0.0,0.5,0.8,
		0.0,0.5,0.8,
		0.0,1.0,0.0 };


	// [TODO] draw the plane with above vertices and color
    
	Matrix4 MVP = project_matrix * view_matrix;
	GLfloat mvp[16];

	mvp[0] = MVP[0];  mvp[4] = MVP[1];   mvp[8] = MVP[2];    mvp[12] = MVP[3];
	mvp[1] = MVP[4];  mvp[5] = MVP[5];   mvp[9] = MVP[6];    mvp[13] = MVP[7];
	mvp[2] = MVP[8];  mvp[6] = MVP[9];   mvp[10] = MVP[10];   mvp[14] = MVP[11];
	mvp[3] = MVP[12]; mvp[7] = MVP[13];  mvp[11] = MVP[14];   mvp[15] = MVP[15];

	// VAO, VBO config
	glGenVertexArrays(1, &quad.vao);
	glBindVertexArray(quad.vao);

	glGenBuffers(1, &quad.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, quad.vbo);
	glBufferData(GL_ARRAY_BUFFER, 18 * sizeof(GL_FLOAT), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	quad.vertex_count = 18 / 3;

	glGenBuffers(1, &quad.p_color);
	glBindBuffer(GL_ARRAY_BUFFER, quad.p_color);
	glBufferData(GL_ARRAY_BUFFER, 18 * sizeof(GL_FLOAT), colors, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	// End of configuration

	// Draw arrays
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glUniformMatrix4fv(iLocMVP, 1, GL_FALSE, mvp);
	glDrawArrays(GL_TRIANGLES, 0, quad.vertex_count);
}

// Render function for display rendering
void RenderScene(void) {	
	// clear canvas
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	// wireframe or solid mode
	isWireframe ?
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE) : glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// [TODO] update translation, rotation and scaling
	model cur_model = models[cur_idx];
	Matrix4 T = translate(cur_model.position),
		R = rotate(cur_model.rotation),
		S = scaling(cur_model.scale);

	// [TODO] multiply all the matrix
	Matrix4 MVP = project_matrix * view_matrix * T * R * S;

	// [TODO] row-major ---> column-major
	GLfloat mvp[16];
	mvp[0] = MVP[0];  mvp[4] = MVP[1];   mvp[8] = MVP[2];    mvp[12] = MVP[3];
	mvp[1] = MVP[4];  mvp[5] = MVP[5];   mvp[9] = MVP[6];    mvp[13] = MVP[7];
	mvp[2] = MVP[8];  mvp[6] = MVP[9];   mvp[10] = MVP[10];   mvp[14] = MVP[11];
	mvp[3] = MVP[12]; mvp[7] = MVP[13];  mvp[11] = MVP[14];   mvp[15] = MVP[15];

	// use uniform to send mvp to vertex shader
	glUniformMatrix4fv(iLocMVP, 1, GL_FALSE, mvp);
	glBindVertexArray(m_shape_list[cur_idx].vao);
	glDrawArrays(GL_TRIANGLES, 0, m_shape_list[cur_idx].vertex_count);
	drawPlane();

}


void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // [TODO] Call back function for keyboard
    if( !(action == GLFW_PRESS) ) return;
    
    unsigned int num_models = (unsigned int) models.size();
    
    switch (key) {
        case GLFW_KEY_ESCAPE:
            exit(0);
                
        case GLFW_KEY_W:
            isWireframe = !isWireframe;
            break;
                
        case GLFW_KEY_Z:
            cur_idx = (cur_idx + num_models - 1) % num_models;
            break;
                
        case GLFW_KEY_X:
            cur_idx = (cur_idx + 1) % num_models;
            break;
                                                    
        case GLFW_KEY_O:
            setOrthogonal();
            break;
                                                                     
        case GLFW_KEY_P:
            setPerspective();
            break;
                                                                                    
        case GLFW_KEY_T:
            cur_trans_mode = GeoTranslation;
            break;
                                                                                
        case GLFW_KEY_S:
            cur_trans_mode = GeoScaling;
            break;
                                                                                 
        case GLFW_KEY_R:
            cur_trans_mode = GeoRotation;
            break;
                                                                             
        case GLFW_KEY_E:
            cur_trans_mode = ViewEye;
            break;
                                                                                
        case GLFW_KEY_C:
            cur_trans_mode = ViewCenter;
            break;
                                                                            
        case GLFW_KEY_U:
            cur_trans_mode = ViewUp;
            break;
                                                                                
        case GLFW_KEY_I:
            auto cur_model = models[cur_idx];
            auto T = translate(cur_model.position);
            auto R = rotate(cur_model.rotation);
            auto S = scaling(cur_model.scale);
            cout << "Translation matrix:" << std::endl;
            cout << T << std::endl;
            cout << "Scaling matrix:" << std::endl;
            cout << S << std::endl;
            cout << "Rotation matrix:" << std::endl;
            cout << R << std::endl;
            cout << "Viewing matrix:" << std::endl;
            cout << view_matrix << std::endl;
            cout << "Projection Matrix:" << std::endl;
            cout << project_matrix << std::endl;
            break;
    }

}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	// [TODO] scroll up positive, otherwise it would be negtive
    const float translation_factor = 0.001;
    const float scaling_factor = 0.001;
    const float rotation_factor = 0.001;
    const float eye_translation_factor = 0.002;
    const float center_translation_factor = 0.002;
    const float up_vector_translation_factor = 0.02;
    
                                
    switch (cur_trans_mode) {
        case GeoTranslation: // TODO plus or minus?
            models[cur_idx].position.z -= yoffset * translation_factor;
            break;
                    
        case GeoScaling: // TODO plus or minus?
            models[cur_idx].scale.z -= yoffset * scaling_factor;
            break;
                    
        case GeoRotation: // TODO plus or minus?
            models[cur_idx].rotation.z -= PI * yoffset * rotation_factor;
            break;
                    
        case ViewEye:
            main_camera.position.z -= yoffset * eye_translation_factor;
            setViewingMatrix();
            break;
                    
        case ViewCenter:
            main_camera.center.z -= yoffset * center_translation_factor;
            setViewingMatrix();
            break;
                    
        case ViewUp:
            main_camera.up_vector.z -= yoffset * up_vector_translation_factor;
            setViewingMatrix();
            break;
    }
    
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	// [TODO] mouse press callback function
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) mouse_pressed = true;
        if (action == GLFW_RELEASE) {
            mouse_pressed = false;
            starting_press_x = -1;
            starting_press_y = -1;
        }
    }
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
	// [TODO] cursor position callback function
    if(!mouse_pressed) return;
    
    if(starting_press_x == -1 || starting_press_y == -1) {
        starting_press_x = xpos;
        starting_press_y = ypos;
        return;
    }
    
    int delta_x = xpos - starting_press_x;
    int delta_y = ypos - starting_press_y;
    
    const float translation_factor = 0.001;
    const float scaling_factor = 0.001;
    const float rotation_factor = 0.001;
    const float eye_translation_factor = 0.002;
    const float center_translation_factor = 0.002;
    const float up_vector_translation_factor = 0.02;
    
                                
    switch (cur_trans_mode) {
        case GeoTranslation: // TODO plus or minus?
            models[cur_idx].position.x += delta_x * translation_factor;
            models[cur_idx].position.y -= delta_y * translation_factor;
            break;
                    
        case GeoScaling: // TODO plus or minus?
            models[cur_idx].scale.x -= delta_x * scaling_factor;
            models[cur_idx].scale.y -= delta_y * scaling_factor;
            break;
                    
        case GeoRotation: // TODO plus or minus?
            models[cur_idx].rotation.x += PI * delta_y * rotation_factor;
            models[cur_idx].rotation.y += PI * delta_x * rotation_factor;
            break;
                    
        case ViewEye:
            main_camera.position.x -= delta_x * eye_translation_factor;
            main_camera.position.y -= delta_y * eye_translation_factor;
            setViewingMatrix();
            break;
                    
        case ViewCenter:
            main_camera.center.x -= delta_x * center_translation_factor;
            main_camera.center.y += delta_y * center_translation_factor;
            setViewingMatrix();
            break;
                    
        case ViewUp:
            main_camera.up_vector.x -= delta_x * up_vector_translation_factor;
            main_camera.up_vector.y -= delta_y * up_vector_translation_factor;
            setViewingMatrix();
            break;
    }
    
    starting_press_x = xpos;
    starting_press_y = ypos;
}

void setShaders()
{
	GLuint v, f, p;
	char *vs = NULL;
	char *fs = NULL;

	v = glCreateShader(GL_VERTEX_SHADER);
	f = glCreateShader(GL_FRAGMENT_SHADER);

	vs = textFileRead("shader.vs");
	fs = textFileRead("shader.fs");

	glShaderSource(v, 1, (const GLchar**)&vs, NULL);
	glShaderSource(f, 1, (const GLchar**)&fs, NULL);

	free(vs);
	free(fs);

	GLint success;
	char infoLog[1000];
	// compile vertex shader
	glCompileShader(v);
	// check for shader compile errors
	glGetShaderiv(v, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(v, 1000, NULL, infoLog);
		std::cout << "ERROR: VERTEX SHADER COMPILATION FAILED\n" << infoLog << std::endl;
	}

	// compile fragment shader
	glCompileShader(f);
	// check for shader compile errors
	glGetShaderiv(f, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(f, 1000, NULL, infoLog);
		std::cout << "ERROR: FRAGMENT SHADER COMPILATION FAILED\n" << infoLog << std::endl;
	}

	// create program object
	p = glCreateProgram();

	// attach shaders to program object
	glAttachShader(p,f);
	glAttachShader(p,v);

	// link program
	glLinkProgram(p);
	// check for linking errors
	glGetProgramiv(p, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(p, 1000, NULL, infoLog);
		std::cout << "ERROR: SHADER PROGRAM LINKING FAILED\n" << infoLog << std::endl;
	}

	glDeleteShader(v);
	glDeleteShader(f);

	iLocMVP = glGetUniformLocation(p, "mvp");

	if (success)
		glUseProgram(p);
    else
    {
        system("pause");
        exit(123);
    }
}

void normalization(tinyobj::attrib_t* attrib, vector<GLfloat>& vertices, vector<GLfloat>& colors, tinyobj::shape_t* shape)
{
	vector<float> xVector, yVector, zVector;
	float minX = 10000, maxX = -10000, minY = 10000, maxY = -10000, minZ = 10000, maxZ = -10000;

	// find out min and max value of X, Y and Z axis
	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		//maxs = max(maxs, attrib->vertices.at(i));
		if (i % 3 == 0)
		{

			xVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minX)
			{
				minX = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxX)
			{
				maxX = attrib->vertices.at(i);
			}
		}
		else if (i % 3 == 1)
		{
			yVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minY)
			{
				minY = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxY)
			{
				maxY = attrib->vertices.at(i);
			}
		}
		else if (i % 3 == 2)
		{
			zVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minZ)
			{
				minZ = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxZ)
			{
				maxZ = attrib->vertices.at(i);
			}
		}
	}

	float offsetX = (maxX + minX) / 2;
	float offsetY = (maxY + minY) / 2;
	float offsetZ = (maxZ + minZ) / 2;

	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		if (offsetX != 0 && i % 3 == 0)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetX;
		}
		else if (offsetY != 0 && i % 3 == 1)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetY;
		}
		else if (offsetZ != 0 && i % 3 == 2)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetZ;
		}
	}

	float greatestAxis = maxX - minX;
	float distanceOfYAxis = maxY - minY;
	float distanceOfZAxis = maxZ - minZ;

	if (distanceOfYAxis > greatestAxis)
	{
		greatestAxis = distanceOfYAxis;
	}

	if (distanceOfZAxis > greatestAxis)
	{
		greatestAxis = distanceOfZAxis;
	}

	float scale = greatestAxis / 2;

	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		//std::cout << i << " = " << (double)(attrib.vertices.at(i) / greatestAxis) << std::endl;
		attrib->vertices.at(i) = attrib->vertices.at(i)/ scale;
	}
	size_t index_offset = 0;
	vertices.reserve(shape->mesh.num_face_vertices.size() * 3);
	colors.reserve(shape->mesh.num_face_vertices.size() * 3);
	for (size_t f = 0; f < shape->mesh.num_face_vertices.size(); f++) {
		int fv = shape->mesh.num_face_vertices[f];

		// Loop over vertices in the face.
		for (size_t v = 0; v < fv; v++) {
			// access to vertex
			tinyobj::index_t idx = shape->mesh.indices[index_offset + v];
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 0]);
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 1]);
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 2]);
			// Optional: vertex colors
			colors.push_back(attrib->colors[3 * idx.vertex_index + 0]);
			colors.push_back(attrib->colors[3 * idx.vertex_index + 1]);
			colors.push_back(attrib->colors[3 * idx.vertex_index + 2]);
		}
		index_offset += fv;
	}
}

void LoadModels(string model_path)
{
	vector<tinyobj::shape_t> shapes;
	vector<tinyobj::material_t> materials;
	tinyobj::attrib_t attrib;
	vector<GLfloat> vertices;
	vector<GLfloat> colors;

	string err;
	string warn;

	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, model_path.c_str());

	if (!warn.empty()) {
		cout << warn << std::endl;
	}

	if (!err.empty()) {
		cerr << err << std::endl;
	}

	if (!ret) {
		exit(1);
	}

	printf("Load Models Success ! Shapes size %d Maerial size %d\n", (int)shapes.size(), (int)materials.size());
	
	normalization(&attrib, vertices, colors, &shapes[0]);

	Shape tmp_shape;
	glGenVertexArrays(1, &tmp_shape.vao);
	glBindVertexArray(tmp_shape.vao);

	glGenBuffers(1, &tmp_shape.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GL_FLOAT), &vertices.at(0), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	tmp_shape.vertex_count = (int)vertices.size() / 3;

	glGenBuffers(1, &tmp_shape.p_color);
	glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.p_color);
	glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(GL_FLOAT), &colors.at(0), GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

	m_shape_list.push_back(tmp_shape);
	model tmp_model;
	models.push_back(tmp_model);


	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	shapes.clear();
	materials.clear();
}

void initParameter()
{
	proj.left = -1;
	proj.right = 1;
	proj.top = 1;
	proj.bottom = -1;
	proj.nearClip = 0.001;
	proj.farClip = 100.0;
	proj.fovy = 80;
	proj.aspect = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;

	main_camera.position = Vector3(0.0f, 0.0f, 2.0f);
	main_camera.center = Vector3(0.0f, 0.0f, 0.0f);
	main_camera.up_vector = Vector3(0.0f, 1.0f, 0.0f);

	setViewingMatrix();
	setPerspective();	//set default projection matrix as perspective matrix
}

void setupRC()
{
	// setup shaders
	setShaders();
	initParameter();

	// OpenGL States and Values
	glClearColor(0.2, 0.2, 0.2, 1.0);
	vector<string> model_list{ "../ColorModels/bunny5KC.obj", "../ColorModels/dragon10KC.obj", "../ColorModels/lucy25KC.obj", "../ColorModels/teapot4KC.obj", "../ColorModels/dolphinC.obj"};
	// [TODO] Load five model at here
    for(auto m: model_list) {
        LoadModels(m);
    }
}

void glPrintContextInfo(bool printExtension)
{
	cout << "GL_VENDOR = " << (const char*)glGetString(GL_VENDOR) << endl;
	cout << "GL_RENDERER = " << (const char*)glGetString(GL_RENDERER) << endl;
	cout << "GL_VERSION = " << (const char*)glGetString(GL_VERSION) << endl;
	cout << "GL_SHADING_LANGUAGE_VERSION = " << (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
	if (printExtension)
	{
		GLint numExt;
		glGetIntegerv(GL_NUM_EXTENSIONS, &numExt);
		cout << "GL_EXTENSIONS =" << endl;
		for (GLint i = 0; i < numExt; i++)
		{
			cout << "\t" << (const char*)glGetStringi(GL_EXTENSIONS, i) << endl;
		}
	}
}


int main(int argc, char **argv)
{
    // initial glfw
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // fix compilation on OS X
#endif

    
    // create window
	GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Student ID HW1", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    
    
    // load OpenGL function pointer
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    
	// register glfw callback functions
    glfwSetKeyCallback(window, KeyCallback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetCursorPosCallback(window, cursor_pos_callback);

    glfwSetFramebufferSizeCallback(window, ChangeSize);
	glEnable(GL_DEPTH_TEST);
	// Setup render context
	setupRC();

	// main loop
    while (!glfwWindowShouldClose(window))
    {
        // render
        RenderScene();
        
        // swap buffer from back to front
        glfwSwapBuffers(window);
        
        // Poll input event
        glfwPollEvents();
    }
	
	// just for compatibiliy purposes
	return 0;
}