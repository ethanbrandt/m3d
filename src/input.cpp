#include <SDL3/SDL.h>
#include <glm/glm.hpp>

#include <map>
#include <string>
#include <cstring>

#include "input.h"

Input* Input::instance = nullptr;

void Input::handle_keyboard_input_event(SDL_Event* event)
{
	KeyState& keyState = keyboard[event->key.key];
	if (event->type == SDL_EVENT_KEY_DOWN)
	{
		if (!keyState.isPressed)
			keyState.wasPressedDown = true;
		keyState.isPressed = true;
	}
	else if (event->type == SDL_EVENT_KEY_UP)
	{
		if (keyState.isPressed)
			keyState.wasReleased = true;
		keyState.isPressed = false;
	}
}

void Input::handle_mouse_button_event(SDL_Event* event)
{
	KeyState& keyState = mouse[event->button.button];
	if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
	{
		if (!keyState.isPressed)
			keyState.wasPressedDown = true;
		keyState.isPressed = true;
	}
	else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP)
	{
		if (keyState.isPressed)
			keyState.wasReleased = true;
		keyState.isPressed = false;
	}
}

void Input::handle_mouse_wheel_event(SDL_Event* event)
{
	scroll = event->wheel.y;
}

void Input::handle_mouse_motion_event(SDL_Event* event)
{
	mouseDelta.x = event->motion.xrel;
	mouseDelta.y = -event->motion.yrel;

	mousePos.x = event->motion.x;
	mousePos.y = event->motion.y;
}

Input::Input()
{
	Input::instance = this;
}

Input::~Input()
{
	Input::instance = nullptr;
}

SDL_MouseButtonFlags Input::get_mouse_button_from_name(const char* mouseButtonName)
{
	if (mouseButtonName == nullptr)
		return 0;

	if (std::strcmp(mouseButtonName, "LMB") == 0 || std::strcmp(mouseButtonName, "Left Mouse Button") == 0 || std::strcmp(mouseButtonName, "1") == 0)
		return SDL_BUTTON_LEFT;

	if (std::strcmp(mouseButtonName, "RMB") == 0 || std::strcmp(mouseButtonName, "Right Mouse Button") == 0 || std::strcmp(mouseButtonName, "2") == 0)
		return SDL_BUTTON_RIGHT;

	if (std::strcmp(mouseButtonName, "MMB") == 0 || std::strcmp(mouseButtonName, "Middle Mouse Button") == 0 || std::strcmp(mouseButtonName, "3") == 0)
		return SDL_BUTTON_MIDDLE;

	if (std::strcmp(mouseButtonName, "FMB") == 0 || std::strcmp(mouseButtonName, "Forward Mouse Button") == 0 || std::strcmp(mouseButtonName, "4") == 0 || std::strcmp(mouseButtonName, "Mouse Button 4") == 0)
		return SDL_BUTTON_X1;

	if (std::strcmp(mouseButtonName, "BMB") == 0 || std::strcmp(mouseButtonName, "Back Mouse Button") == 0 || std::strcmp(mouseButtonName, "5") == 0 || std::strcmp(mouseButtonName, "Mouse Button 5") == 0)
		return SDL_BUTTON_X2;

	return 0;
}

bool Input::is_key_pressed(SDL_Keycode key)
{
	if (keyboard.find(key) == keyboard.end())
		return false;
	return keyboard[key].isPressed;
}

bool Input::was_key_pressed_down(SDL_Keycode key)
{
	if (keyboard.find(key) == keyboard.end())
		return false;
	return keyboard[key].wasPressedDown;
}

bool Input::was_key_released(SDL_Keycode key)
{
	if (keyboard.find(key) == keyboard.end())
		return false;
	return keyboard[key].wasReleased;
}

bool Input::is_mouse_button_pressed(SDL_MouseButtonFlags button)
{
	if (mouse.find(button) == mouse.end())
		return false;
	return mouse[button].isPressed;
}

bool Input::was_mouse_button_pressed_down(SDL_MouseButtonFlags button)
{
	if (mouse.find(button) == mouse.end())
		return false;
	return mouse[button].wasPressedDown;
}

bool Input::was_mouse_button_released(SDL_MouseButtonFlags button)
{
	if (mouse.find(button) == mouse.end())
		return false;
	return mouse[button].wasReleased;
}

glm::vec2 Input::get_mouse_pos()
{
	return mousePos;
}

glm::vec2 Input::get_mouse_delta()
{
	return mouseDelta;
}

float Input::get_scroll()
{
	return scroll;
}

void Input::update_input()
{
	scroll = 0.0f;
	mouseDelta.x = 0.0f;
	mouseDelta.y = 0.0f;
	
	for (auto it = keyboard.begin(); it != keyboard.end(); ++it)
	{
		if (keyboard[it->first].wasPressedDown)
			keyboard[it->first].wasPressedDown = false;

		if (keyboard[it->first].wasReleased)
			keyboard[it->first].wasReleased = false;
	}

	for (auto it = mouse.begin(); it != mouse.end(); ++it)
	{
		if (mouse[it->first].wasPressedDown)
			mouse[it->first].wasPressedDown = false;

		if (mouse[it->first].wasReleased)
			mouse[it->first].wasReleased = false;
	}
}

void Input::handle_input_event(SDL_Event* event)
{
	if (event->type == SDL_EVENT_MOUSE_MOTION)
	{
		handle_mouse_motion_event(event);
		return;
	}

	if (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP)
	{
		handle_keyboard_input_event(event);
		return;
	}

	if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN || event->type == SDL_EVENT_MOUSE_BUTTON_UP)
	{
		handle_mouse_button_event(event);	
		return;
	}

	if (event->type == SDL_EVENT_MOUSE_WHEEL)
		handle_mouse_wheel_event(event);
}