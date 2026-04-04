#pragma once

#include <SDL3/SDL.h>
#include <glm/glm.hpp>

#include <map>
#include <string>

class Input
{
	private:
		struct KeyState
		{
			bool isPressed = false;
			bool wasPressedDown = false;
			bool wasReleased = false;
		};
		std::map<SDL_Keycode, KeyState> keyboard;
		std::map<SDL_MouseButtonFlags, KeyState> mouse;
		glm::vec2 mousePos = glm::vec2(0, 0);
		glm::vec2 mouseDelta = glm::vec2(0, 0);
		float scroll = 0.0f;

		void handle_keyboard_input_event(SDL_Event* event);

		void handle_mouse_button_event(SDL_Event* event);
		void handle_mouse_wheel_event(SDL_Event* event);
		void handle_mouse_motion_event(SDL_Event* event);


	public:
		static Input* instance;

		Input();
		~Input();

		static SDL_MouseButtonFlags get_mouse_button_from_name(const char* mouseButtonName);

		bool is_key_pressed(SDL_Keycode key);
		bool was_key_pressed_down(SDL_Keycode key);
		bool was_key_released(SDL_Keycode key);

		bool is_mouse_button_pressed(SDL_MouseButtonFlags button);
		bool was_mouse_button_pressed_down(SDL_MouseButtonFlags button);
		bool was_mouse_button_released(SDL_MouseButtonFlags button);

		glm::vec2 get_mouse_pos();
		glm::vec2 get_mouse_delta();

		float get_scroll();

		void update_input();

		void handle_input_event(SDL_Event* event);
};