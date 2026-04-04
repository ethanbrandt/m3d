#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

class Shader
{
public:
	unsigned int id;

	Shader() = default;
	Shader(const char* vertPath, const char* fragPath);
	void use();
	
	void set_bool(const std::string& name, bool val) const;
	void set_int(const std::string& name, int val) const;
	void set_float(const std::string& name, float val) const;

	void set_vec2(const std::string& name, glm::vec2 val) const;
	void set_vec3(const std::string& name, glm::vec3 val) const;
	void set_vec4(const std::string& name, glm::vec4 val) const;
	
	void set_mat2(const std::string& name, glm::mat2 val) const;
	void set_mat3(const std::string& name, glm::mat3 val) const;
	void set_mat4(const std::string& name, glm::mat4 val) const;
private:
	mutable std::unordered_map<std::string, int> uniformLocationCache;

	void check_and_log_shader_compile_status(const int& s, const std::string& type);
	void check_and_log_shader_link_status(const int& s);

	int get_uniform_location(const std::string& name) const;
};

inline int Shader::get_uniform_location(const std::string& name) const
{
	auto it = uniformLocationCache.find(name);
	if (it != uniformLocationCache.end())
		return it->second;
	
	int location = glGetUniformLocation(id, name.c_str());
	uniformLocationCache.emplace(name, location);
	return location;
}

inline Shader::Shader(const char* vertPath, const char* fragPath)
{
	std::string vertCode;
	std::string fragCode;
	std::ifstream vShaderFile;
	std::ifstream fShaderFile;

	vShaderFile.open(vertPath);
	fShaderFile.open(fragPath);

	if (!vShaderFile.is_open())
		std::cout << "Failed to load vertex shader file at: " << vertPath << " (double check this exists / is named correctly)" << std::endl;
	if (!fShaderFile.is_open())
		std::cout << "Failed to load fragment shader file at: " << fragPath << " (double check this exists / is named correctly)" << std::endl;

	std::stringstream vShaderStream;
	std::stringstream fShaderStream;

	vShaderStream << vShaderFile.rdbuf();
	fShaderStream << fShaderFile.rdbuf();

	if (vShaderFile.fail())
		std::cout << "Error while reading vertex shader file at: " << vertPath << std::endl;
	if (fShaderFile.fail())
		std::cout << "Error while reading fragment shader file at: " << fragPath << std::endl;

	vShaderFile.close();
	fShaderFile.close();

	vertCode = vShaderStream.str();
	fragCode = fShaderStream.str();

	const char* vShaderCode = vertCode.c_str();
	const char* fShaderCode = fragCode.c_str();

	unsigned int vert, frag;

	vert = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vert, 1, &vShaderCode, NULL);
	glCompileShader(vert);
	check_and_log_shader_compile_status(vert, "vertex");	

	frag = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(frag, 1, &fShaderCode, NULL);
	glCompileShader(frag);
	check_and_log_shader_compile_status(frag, "fragment");

	id = glCreateProgram();
	glAttachShader(id, vert);
	glAttachShader(id, frag);
	glLinkProgram(id);
	check_and_log_shader_link_status(id);

	glDeleteShader(vert);
	glDeleteShader(frag);
}

inline void Shader::check_and_log_shader_compile_status(const int& s, const std::string& type)
{
	int success;
	char infoLog[512];
	glGetShaderiv(s, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(s, 512, NULL, infoLog);
		std::cout << "Error while compiling " << type << " shader:\n" << infoLog << std::endl;
	}
}

inline void Shader::check_and_log_shader_link_status(const int& s)
{
	int success;
	char infoLog[512];
	glGetProgramiv(s, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(s, 512, NULL, infoLog);
		std::cout << "Error while linking shader program:\n" << infoLog << std::endl;
	}
}

inline void Shader::use()
{
	glUseProgram(id);
}

inline void Shader::set_bool(const std::string &name, bool val) const
{
	glUniform1i(get_uniform_location(name), (int)val);
}

inline void Shader::set_int(const std::string &name, int val) const
{
	glUniform1i(get_uniform_location(name), val);
}

inline void Shader::set_float(const std::string &name, float val) const
{
	glUniform1f(get_uniform_location(name), val);
}

inline void Shader::set_vec2(const std::string &name, glm::vec2 val) const
{
	glUniform2fv(get_uniform_location(name), 1, &val[0]);
}

inline void Shader::set_vec3(const std::string &name, glm::vec3 val) const
{
	glUniform3fv(get_uniform_location(name), 1, &val[0]);
}

inline void Shader::set_vec4(const std::string &name, glm::vec4 val) const
{
	glUniform4fv(get_uniform_location(name), 1, &val[0]);
}

inline void Shader::set_mat2(const std::string &name, glm::mat2 val) const
{
	glUniformMatrix2fv(get_uniform_location(name), 1, GL_FALSE, &val[0][0]);
}

inline void Shader::set_mat3(const std::string &name, glm::mat3 val) const
{
	glUniformMatrix3fv(get_uniform_location(name), 1, GL_FALSE, &val[0][0]);
}

inline void Shader::set_mat4(const std::string &name, glm::mat4 val) const
{
	glUniformMatrix4fv(get_uniform_location(name), 1, GL_FALSE, &val[0][0]);
}