#include <fstream>
#include <iostream>
#include <math.h>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "textfile.h"

#include "Matrices.h"
#include "Vectors.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

using namespace std;

constexpr double PI = 3.14159265358979323846;

// Default window size
constexpr int WINDOW_WIDTH = 800;
constexpr int WINDOW_HEIGHT = 600;

bool mouse_pressed = false;
double last_x = 0, last_y = 0;

bool is_wireframe = false;

enum TransMode
{
	GeoTranslation = 0,
	GeoRotation = 1,
	GeoScaling = 2,
	ViewCenter = 3,
	ViewEye = 4,
	ViewUp = 5,
};
TransMode cur_trans_mode = GeoTranslation;

enum ProjMode
{
	Orthogonal = 0,
	Perspective = 1,
};
ProjMode cur_proj_mode = Orthogonal;

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
vector<Shape> m_shape_list;
Shape quad;

struct model
{
	Vector3 position = Vector3(0, 0, 0);
	Vector3 scale = Vector3(1, 1, 1);
	Vector3 rotation = Vector3(0, 0, 0); // Euler form
};
vector<model> models;
int cur_idx = 0; // represent which model should be rendered now

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

Matrix4 view_matrix;
Matrix4 project_matrix;

GLint iLocMVP;

static GLvoid Normalize(GLfloat v[3])
{
	GLfloat l = (GLfloat)sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	v[0] /= l;
	v[1] /= l;
	v[2] /= l;
}

static GLvoid Cross(GLfloat u[3], GLfloat v[3], GLfloat n[3])
{
	n[0] = u[1] * v[2] - u[2] * v[1];
	n[1] = u[2] * v[0] - u[0] * v[2];
	n[2] = u[0] * v[1] - u[1] * v[0];
}

// [TODO] given a translation vector then output a Matrix4 (Translation Matrix)
Matrix4 translate(Vector3 vec)
{
	Matrix4 mat;

	mat = Matrix4(
		1, 0, 0, vec.x,
		0, 1, 0, vec.y,
		0, 0, 1, vec.z,
		0, 0, 0,     1
	);

	return mat;
}

// [TODO] given a scaling vector then output a Matrix4 (Scaling Matrix)
Matrix4 scaling(Vector3 vec)
{
	Matrix4 mat;

	mat = Matrix4(
		vec.x,     0,     0, 0,
		    0, vec.y,     0, 0,
		    0,     0, vec.z, 0,
		    0,     0,     0, 1
	);

	return mat;
}

inline float degree2radian(double degree)
{
	return static_cast<float>(degree * PI / 180);
}

// [TODO] given a float value then ouput a rotation matrix alone axis-X (rotate alone axis-X)
Matrix4 rotateX(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(
		1,        0,         0, 0,
		0, cos(val), -sin(val), 0,
		0, sin(val),  cos(val), 0,
		0,        0,         0, 1
	);

	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-Y (rotate alone axis-Y)
Matrix4 rotateY(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(
		 cos(val), 0, sin(val), 0,
		        0, 1,        0, 0,
		-sin(val), 0, cos(val), 0,
		        0, 0,        0, 1
	);

	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-Z (rotate alone axis-Z)
Matrix4 rotateZ(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(
		cos(val), -sin(val), 0, 0,
		sin(val),  cos(val), 0, 0,
		        0,        0, 1, 0,
		        0,        0, 0, 1
	);

	return mat;
}

Matrix4 rotate(Vector3 vec)
{
	return rotateX(vec.x) * rotateY(vec.y) * rotateZ(vec.z);
}

// [TODO] compute viewing matrix accroding to the setting of main_camera
void setViewingMatrix()
{
	Vector3 vec_P1P2 = main_camera.center - main_camera.position;
	float P1P2[3] = {vec_P1P2.x, vec_P1P2.y, vec_P1P2.z};
	Vector3 vec_P1P3 = main_camera.up_vector;
	float P1P3[3] = {vec_P1P3.x, vec_P1P3.y, vec_P1P3.z};

	float R_z[3] = {-P1P2[0], -P1P2[1], -P1P2[2]};
	Normalize(R_z);

	float R_x[3];
	Cross(P1P2, P1P3, R_x);
	Normalize(R_x);

	float R_y[3];
	Cross(R_z, R_x, R_y);
	Normalize(R_y);

	Matrix4 R = Matrix4(
		R_x[0], R_x[1], R_x[2], 0,
		R_y[0], R_y[1], R_y[2], 0,
		R_z[0], R_z[1], R_z[2], 0,
		     0,      0,      0, 1
	);

	Matrix4 T = Matrix4(
		1, 0, 0, -main_camera.position.x,
		0, 1, 0, -main_camera.position.y,
		0, 0, 1, -main_camera.position.z,
		0, 0, 0,                       1
	);

	view_matrix = R * T;
}

// [TODO] compute orthogonal projection matrix
void setOrthogonal()
{
	cur_proj_mode = Orthogonal;

	float x = 2 / (proj.right - proj.left);
	float y = 2 / (proj.top - proj.bottom);
	float z = -2 / (proj.farClip - proj.nearClip);
	float t_x = -(proj.right + proj.left) / (proj.right - proj.left);
	float t_y = -(proj.top + proj.bottom) / (proj.top - proj.bottom);
	float t_z = -(proj.farClip + proj.nearClip) / (proj.farClip - proj.nearClip);

	project_matrix = Matrix4(
		x, 0, 0, t_x,
		0, y, 0, t_y,
		0, 0, z, t_z,
		0, 0, 0,   1
	);

	if (proj.aspect >= 1)
		project_matrix[0] = x / proj.aspect;
	else
		project_matrix[5] = y * proj.aspect;
}

// [TODO] compute persepective projection matrix
void setPerspective()
{
	cur_proj_mode = Perspective;

	float f = cos(degree2radian(proj.fovy) / 2) / sin(degree2radian(proj.fovy) / 2);
	float z = (proj.farClip + proj.nearClip) / (proj.nearClip - proj.farClip);
	float t_z = (2 * proj.farClip * proj.nearClip) / (proj.nearClip - proj.farClip);

	project_matrix = Matrix4(
		f, 0,  0,   0,
		0, f,  0,   0,
		0, 0,  z, t_z,
		0, 0, -1,   0
	);

	if (proj.aspect >= 1)
		project_matrix[0] = f / proj.aspect;
	else
		project_matrix[5] = f * proj.aspect;
}

// Call back function for window reshape
void ChangeSize(GLFWwindow *window, int width, int height)
{
	glViewport(0, 0, width, height);
	// [TODO] change your aspect ratio
	if (width == 0 || height == 0)
		return;

	proj.aspect = static_cast<float>(width) / height;
	if (cur_proj_mode == ProjMode::Orthogonal)
	{
		if (proj.aspect >= 1)
			project_matrix[0] = 2 / (proj.right - proj.left) / proj.aspect;
		else
			project_matrix[5] = 2 / (proj.top - proj.bottom) * proj.aspect;
	}
	else if (cur_proj_mode == ProjMode::Perspective)
	{
		float f = cos(degree2radian(proj.fovy) / 2) / sin(degree2radian(proj.fovy) / 2);
		if (proj.aspect >= 1)
			project_matrix[0] = f / proj.aspect;
		else
			project_matrix[5] = f * proj.aspect;
	}
}

void drawPlane()
{
	// [TODO] draw the plane with above vertices and color
	/* For loading plane, please refer to "loadPlane". */

	/* modify from "RenderScene" */
	Matrix4 MVP;
	GLfloat mvp[16];

	MVP = project_matrix * view_matrix;

	mvp[0] = MVP[0];  mvp[4] = MVP[1];  mvp[8] = MVP[2];   mvp[12] = MVP[3];
	mvp[1] = MVP[4];  mvp[5] = MVP[5];  mvp[9] = MVP[6];   mvp[13] = MVP[7];
	mvp[2] = MVP[8];  mvp[6] = MVP[9];  mvp[10] = MVP[10]; mvp[14] = MVP[11];
	mvp[3] = MVP[12]; mvp[7] = MVP[13]; mvp[11] = MVP[14]; mvp[15] = MVP[15];

	/* let plane be solid */
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glUniformMatrix4fv(iLocMVP, 1, GL_FALSE, mvp);
	glBindVertexArray(quad.vao);
	glDrawArrays(GL_TRIANGLES, 0, quad.vertex_count);
}

// Render function for display rendering
void RenderScene(void)
{
	// clear canvas
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	Matrix4 T, R, S;
	// [TODO] update translation, rotation and scaling
	T = translate(models.at(cur_idx).position);
	R = rotate(models.at(cur_idx).rotation);
	S = scaling(models.at(cur_idx).scale);

	Matrix4 MVP;
	GLfloat mvp[16];

	// [TODO] multiply all the matrix
	MVP = project_matrix * view_matrix * T * R * S;

	// [TODO] row-major ---> column-major
	mvp[0] = MVP[0];  mvp[4] = MVP[1];  mvp[8] = MVP[2];   mvp[12] = MVP[3];
	mvp[1] = MVP[4];  mvp[5] = MVP[5];  mvp[9] = MVP[6];   mvp[13] = MVP[7];
	mvp[2] = MVP[8];  mvp[6] = MVP[9];  mvp[10] = MVP[10]; mvp[14] = MVP[11];
	mvp[3] = MVP[12]; mvp[7] = MVP[13]; mvp[11] = MVP[14]; mvp[15] = MVP[15];

	/* let model be solid or be wireframe */
	if (is_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// use uniform to send mvp to vertex shader
	glUniformMatrix4fv(iLocMVP, 1, GL_FALSE, mvp);
	glBindVertexArray(m_shape_list[cur_idx].vao);
	glDrawArrays(GL_TRIANGLES, 0, m_shape_list[cur_idx].vertex_count);
	drawPlane();
}

void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	// [TODO] Call back function for keyboard
	if (action == GLFW_PRESS)
	{
		switch (key)
		{
		case GLFW_KEY_W:
			is_wireframe = !is_wireframe;
			break;
		case GLFW_KEY_Z:
			cur_idx = (cur_idx == 0) ? static_cast<int>(models.size()) - 1 : cur_idx - 1;
			break;
		case GLFW_KEY_X:
			cur_idx = (cur_idx == static_cast<int>(models.size()) - 1) ? 0 : cur_idx + 1;
			break;
		case GLFW_KEY_O:
			setOrthogonal();
			break;
		case GLFW_KEY_P:
			setPerspective();
			break;
		case GLFW_KEY_T:
			cur_trans_mode = TransMode::GeoTranslation;
			break;
		case GLFW_KEY_S:
			cur_trans_mode = TransMode::GeoScaling;
			break;
		case GLFW_KEY_R:
			cur_trans_mode = TransMode::GeoRotation;
			break;
		case GLFW_KEY_E:
			cur_trans_mode = TransMode::ViewEye;
			break;
		case GLFW_KEY_C:
			cur_trans_mode = TransMode::ViewCenter;
			break;
		case GLFW_KEY_U:
			cur_trans_mode = TransMode::ViewUp;
			break;
		case GLFW_KEY_I:
			std::cout << "Matrix Value:\n"
					  << "Viewing Matrix:\n"
					  << view_matrix << '\n'
					  << "Projection Matrix:\n"
					  << project_matrix << '\n'
					  << "Translation Matrix:\n"
					  << translate(models.at(cur_idx).position) << '\n'
					  << "Rotation Matrix:\n"
					  << rotate(models.at(cur_idx).rotation) << '\n'
					  << "Scaling Matrix:\n"
					  << scaling(models.at(cur_idx).scale) << '\n';
			break;
		default:
			break;
		}
	}
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
	// [TODO] scroll up positive, otherwise it would be negtive
	float diff = static_cast<float>(yoffset);
	switch (cur_trans_mode)
	{
	case TransMode::GeoTranslation:
		models.at(cur_idx).position.z += diff / 10;
		break;
	case TransMode::GeoScaling:
		models.at(cur_idx).scale.z += diff / 10;
		break;
	case TransMode::GeoRotation:
		models.at(cur_idx).rotation.z += degree2radian(diff);
		break;
	case TransMode::ViewEye:
		main_camera.position.z -= diff / 10;
		setViewingMatrix();
		break;
	case TransMode::ViewCenter:
		main_camera.center.z += diff / 10;
		setViewingMatrix();
		break;
	case TransMode::ViewUp:
		main_camera.up_vector.z += diff / 10;
		setViewingMatrix();
		break;
	default:
		break;
	}
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
	// [TODO] mouse press callback function
	if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
		if (action == GLFW_PRESS)
			mouse_pressed = true;
		else if (action == GLFW_RELEASE)
			mouse_pressed = false;
	}
}

static void cursor_pos_callback(GLFWwindow *window, double xpos, double ypos)
{
	// [TODO] cursor position callback function
	if (mouse_pressed)
	{
		float diff_x = static_cast<float>(xpos - last_x);
		float diff_y = static_cast<float>(ypos - last_y);
		switch (cur_trans_mode)
		{
		case TransMode::GeoTranslation:
			models.at(cur_idx).position.x += diff_x / 100;
			models.at(cur_idx).position.y -= diff_y / 100;
			break;
		case TransMode::GeoScaling:
			models.at(cur_idx).scale.x -= diff_x / 100;
			models.at(cur_idx).scale.y -= diff_y / 100;
			break;
		case TransMode::GeoRotation:
			models.at(cur_idx).rotation.x -= degree2radian(diff_y);
			models.at(cur_idx).rotation.y -= degree2radian(diff_x);
			break;
		case TransMode::ViewEye:
			main_camera.position.x -= diff_x / 100;
			main_camera.position.y += diff_y / 100;
			setViewingMatrix();
			break;
		case TransMode::ViewCenter:
			main_camera.center.x -= diff_x / 100;
			main_camera.center.y -= diff_y / 100;
			setViewingMatrix();
			break;
		case TransMode::ViewUp:
			main_camera.up_vector.x -= diff_x / 30;
			main_camera.up_vector.y += diff_y / 30;
			setViewingMatrix();
			break;
		default:
			break;
		}
	}
	last_x = xpos;
	last_y = ypos;
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

	glShaderSource(v, 1, (const GLchar **)&vs, NULL);
	glShaderSource(f, 1, (const GLchar **)&fs, NULL);

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
		std::cout << "ERROR: VERTEX SHADER COMPILATION FAILED\n"
				  << infoLog << std::endl;
	}

	// compile fragment shader
	glCompileShader(f);
	// check for shader compile errors
	glGetShaderiv(f, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(f, 1000, NULL, infoLog);
		std::cout << "ERROR: FRAGMENT SHADER COMPILATION FAILED\n"
				  << infoLog << std::endl;
	}

	// create program object
	p = glCreateProgram();

	// attach shaders to program object
	glAttachShader(p, f);
	glAttachShader(p, v);

	// link program
	glLinkProgram(p);
	// check for linking errors
	glGetProgramiv(p, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(p, 1000, NULL, infoLog);
		std::cout << "ERROR: SHADER PROGRAM LINKING FAILED\n"
				  << infoLog << std::endl;
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

void normalization(tinyobj::attrib_t *attrib, vector<GLfloat> &vertices, vector<GLfloat> &colors, tinyobj::shape_t *shape)
{
	vector<float> xVector, yVector, zVector;
	float minX = 10000, maxX = -10000, minY = 10000, maxY = -10000, minZ = 10000, maxZ = -10000;

	// find out min and max value of X, Y and Z axis
	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		// maxs = max(maxs, attrib->vertices.at(i));
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
		// std::cout << i << " = " << (double)(attrib.vertices.at(i) / greatestAxis) << std::endl;
		attrib->vertices.at(i) = attrib->vertices.at(i) / scale;
	}
	size_t index_offset = 0;
	vertices.reserve(shape->mesh.num_face_vertices.size() * 3);
	colors.reserve(shape->mesh.num_face_vertices.size() * 3);
	for (size_t f = 0; f < shape->mesh.num_face_vertices.size(); f++)
	{
		int fv = shape->mesh.num_face_vertices[f];

		// Loop over vertices in the face.
		for (size_t v = 0; v < fv; v++)
		{
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

void loadPlane()
{
	GLfloat vertices[18]{ 1.0, -0.9, -1.0,
						  1.0, -0.9,  1.0,
						 -1.0, -0.9, -1.0,
						  1.0, -0.9,  1.0,
						 -1.0, -0.9,  1.0,
						 -1.0, -0.9, -1.0};

	GLfloat colors[18]{0.0, 1.0, 0.0,
					   0.0, 0.5, 0.8,
					   0.0, 1.0, 0.0,
					   0.0, 0.5, 0.8,
					   0.0, 0.5, 0.8,
					   0.0, 1.0, 0.0};

	/* modify from "LoadModels" */
	glGenVertexArrays(1, &quad.vao);
	glBindVertexArray(quad.vao);

	glGenBuffers(1, &quad.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, quad.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	quad.vertex_count = sizeof(vertices) / sizeof(GLfloat) / 3;

	glGenBuffers(1, &quad.p_color);
	glBindBuffer(GL_ARRAY_BUFFER, quad.p_color);
	glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
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

	if (!warn.empty())
	{
		cout << warn << std::endl;
	}

	if (!err.empty())
	{
		cerr << err << std::endl;
	}

	if (!ret)
	{
		exit(1);
	}

	printf("Load Models Success ! Shapes size %d Maerial size %d\n", shapes.size(), materials.size());

	normalization(&attrib, vertices, colors, &shapes[0]);

	Shape tmp_shape;
	glGenVertexArrays(1, &tmp_shape.vao);
	glBindVertexArray(tmp_shape.vao);

	glGenBuffers(1, &tmp_shape.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GL_FLOAT), &vertices.at(0), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	tmp_shape.vertex_count = vertices.size() / 3;

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
	setPerspective(); // set default projection matrix as perspective matrix
}

void setupRC()
{
	// setup shaders
	setShaders();
	initParameter();

	// OpenGL States and Values
	glClearColor(0.2, 0.2, 0.2, 1.0);
	vector<string> model_list{"../ColorModels/bunny5KC.obj", "../ColorModels/dragon10KC.obj", "../ColorModels/lucy25KC.obj", "../ColorModels/teapot4KC.obj", "../ColorModels/dolphinC.obj"};
	// [TODO] Load five model at here
	for (const auto &model_path : model_list)
		LoadModels(model_path);
	loadPlane();
}

void glPrintContextInfo(bool printExtension)
{
	cout << "GL_VENDOR = " << (const char *)glGetString(GL_VENDOR) << endl;
	cout << "GL_RENDERER = " << (const char *)glGetString(GL_RENDERER) << endl;
	cout << "GL_VERSION = " << (const char *)glGetString(GL_VERSION) << endl;
	cout << "GL_SHADING_LANGUAGE_VERSION = " << (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
	if (printExtension)
	{
		GLint numExt;
		glGetIntegerv(GL_NUM_EXTENSIONS, &numExt);
		cout << "GL_EXTENSIONS =" << endl;
		for (GLint i = 0; i < numExt; i++)
		{
			cout << "\t" << (const char *)glGetStringi(GL_EXTENSIONS, i) << endl;
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
	GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "110062802 HW1", NULL, NULL);
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
