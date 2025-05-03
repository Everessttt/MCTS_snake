#pragma once

#include <Godot/variant/array.hpp>

godot::Array deep_duplicate(const godot::Array& matrix);
godot::Array spawn_fruit(godot::Array snake_matrix);
godot::Array move_snake(godot::Array snake_matrix, int move_dir);
