// Godot SDK
#include <Godot/godot.hpp>
#include <Godot/godot.hpp>
#include <Godot/classes/node.hpp>
#include <Godot/variant/variant.hpp>
#include <Godot/classes/input.hpp>
#include <Godot/classes/node2d.hpp>
#include <Godot/classes/timer.hpp>
#include <Godot/variant/array.hpp>
#include <Godot/classes/canvas_layer.hpp>
#include <Godot/classes/color_rect.hpp>
#include <Godot/variant/utility_functions.hpp>
#include <random>
#include <Godot/classes/line_edit.hpp>

// Jenova SDK
#include <JenovaSDK.h>
#include <headers/snake_functions.hpp>

// Namespaces
using namespace godot;
using namespace jenova::sdk;
using namespace std;

Array deep_duplicate(const Array& matrix) {
	Array result;
	for(int i = 0; i < matrix.size(); i++) {
		result.append(matrix[i].duplicate());
	}

	return result;
}

Array spawn_fruit(Array snake_matrix) {
	int grid_size = snake_matrix.size();

	//find all empty squares
	Array empty_squares;
	for(int y = 0; y < grid_size; y++) {
		Array row = snake_matrix[y];
		for(int x = 0; x < row.size(); x++) {
			int cell_value = row[x];
			if(cell_value == 0) {
				empty_squares.append(Vector2(x, y));
			}
		}
	}

	//game won
	if(empty_squares.size() == 0) {
		return snake_matrix;
	}

	//spawn fruit in random empty square
	Node2D* game = GetNode<Node2D>("game");
	int seed;
	if(game->has_meta("fruit_seed")) {
		seed = game->get_meta("fruit_seed");
	} 
	else {
		LineEdit* line_seed = GetNode<LineEdit>("game/ui/MarginContainer/VBoxContainer/seed");
		seed = line_seed->get_text().to_int();
	}
	mt19937 gen(seed);
	uniform_int_distribution<> dis(0, empty_squares.size() - 1);
	int index = dis(gen);
	Vector2 pos = empty_squares[index];
	game->set_meta("fruit_seed", gen());

	//place fruit into matrix
	Array row = snake_matrix[pos.y];
	row[pos.x] = -1;
	snake_matrix[pos.y] = row;

	return snake_matrix;
}

Array move_snake(Array snake_matrix, int move_dir) {
	//move snake by updating matrix
	int grid_size = snake_matrix.size();

	//find current head position and snake length
	Vector2 head_pos, prev_head_pos; 
	int snake_length = 0;
	int prev_head_val = 0;

	Array row;
	int cell_value;
	for(int y = 0; y < snake_matrix.size(); y++) {
		row = snake_matrix[y];
		for(int x = 0; x < row.size(); x++) {
			cell_value = row[x];
			
			//largest cell will contain head position and current length
			if(cell_value > snake_length) {
				prev_head_pos = head_pos;
				prev_head_val = snake_length;

				head_pos = Vector2(x, y);
				snake_length = cell_value;
			}
			else if(cell_value > prev_head_val) {
				prev_head_pos = Vector2(x, y);
				prev_head_val = cell_value;
			}
		}
	}

	if(snake_length < 2) {
		return snake_matrix;
	}

	//find direction snake moved previously
	int prev_move_dir;
	Vector2 movement_delta = head_pos - prev_head_pos;
	if     (movement_delta == Vector2(0, -1)) prev_move_dir = 0; //prev move north
	else if(movement_delta == Vector2(1, 0))  prev_move_dir = 1; //prev move east
	else if(movement_delta == Vector2(0, 1))  prev_move_dir = 2; //prev move south
	else if(movement_delta == Vector2(-1, 0)) prev_move_dir = 3; //prev move west

	//find next head position by moving snake according to current move direction
	//prevent snake from making 180 degree turn, move straight instead
	if(move_dir == (prev_move_dir + 2) % 4) {
		move_dir = prev_move_dir;
	}

	Vector2 next_head_pos = head_pos;
	if     (move_dir == 0) next_head_pos.y -= 1; //move north
	else if(move_dir == 1) next_head_pos.x += 1; //move east
	else if(move_dir == 2) next_head_pos.y += 1; //move south
	else if(move_dir == 3) next_head_pos.x -= 1; //move west

	//check head boundary collision
	bool is_game_over = false;
	if(next_head_pos.x < 0 || next_head_pos.y < 0 || next_head_pos.x >= grid_size || next_head_pos.y >= grid_size) {
		is_game_over = true;
	}

	//check head self collision
	if(!is_game_over) {
		row = snake_matrix[next_head_pos.y];
		cell_value = row[next_head_pos.x];
		if(cell_value > 1) {
			is_game_over = true;
		}
	}

	if(is_game_over) {
		//update matrix
		for(int y = 0; y < snake_matrix.size(); y++) {
			row = snake_matrix[y];
			for(int x = 0; x < row.size(); x++) {
				cell_value = row[x];

				//invert colors
				if(cell_value > 0) {
					row[x] = 1;
				}
				else if(cell_value == 0) {
					row[x] = 0;
				}
			}
		}

		return snake_matrix;
	}

	//check head fruit collision
	bool is_fruit_eaten = false;

	row = snake_matrix[next_head_pos.y];
	cell_value = row[next_head_pos.x];
	if(cell_value == -1) {
		is_fruit_eaten = true;
		snake_length++;

		//spawn new fruit
		snake_matrix = spawn_fruit(deep_duplicate(snake_matrix));
	}

	//move snake
	for(int y = 0; y < snake_matrix.size(); y++) {
		row = snake_matrix[y];
		for(int x = 0; x < row.size(); x++) {
			cell_value = row[x];
			
			//decrement all cells containing snake body
			if(cell_value > 0 && !is_fruit_eaten) {
				row[x] = cell_value - 1;
			}

			if(x == next_head_pos.x && y == next_head_pos.y) {
				row[x] = snake_length;
			}
		}
	}

	return snake_matrix;
}
