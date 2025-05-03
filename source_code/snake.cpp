// Godot SDK
#include <Godot/godot.hpp>
#include <Godot/classes/node.hpp>
#include <Godot/variant/variant.hpp>
#include <Godot/classes/input.hpp>
#include <Godot/classes/node2d.hpp>
#include <Godot/classes/timer.hpp>
#include <Godot/variant/array.hpp>
#include <Godot/classes/canvas_layer.hpp>
#include <Godot/classes/tile_map_layer.hpp>
#include <Godot/classes/tile_data.hpp>
#include <cstdlib>
#include <ctime>
#include <Godot/variant/utility_functions.hpp>
#include <Godot/classes/label.hpp>
#include <Godot/classes/check_button.hpp>
#include <Godot/classes/line_edit.hpp>
#include <Godot/classes/button.hpp>
#include <chrono>

// Jenova SDK
#include <JenovaSDK.h>
#include <headers/snake_functions.hpp>
#include <headers/MCTS.hpp>

// Namespaces
using namespace godot;
using namespace jenova::sdk;

// Start Jenova Script
JENOVA_SCRIPT_BEGIN

void start_game(Caller* instance);

// Called When Node Enters Scene Tree
void OnAwake(Caller* instance)
{

}

// Called When Node Exits Scene Tree
void OnDestroy(Caller* instance)
{

}

// Called When Node and All It's Children Entered Scene Tree
void OnReady(Caller* instance) {
	Node2D* self = GetSelf<Node2D>(instance);

	//initialize end screen
	CanvasLayer* game_over = GetNode<CanvasLayer>("game/game_over");
	game_over->set_deferred("visible", false);

	//wait until game is started by user
	self->set_meta("is_game_started", false);

	//initialize timer
	Timer* timer = GetNode<Timer>("game/Timer");
	timer->set_wait_time(0.1);
	timer->set_one_shot(false);
	Callable call = Callable(self, "on_timer_timeout").bind(self);
	timer->connect("timeout", call);
}

// Called On Every Frame
void OnProcess(Caller* instance, Variant* _delta) {
	Node2D* self = GetSelf<Node2D>(instance);
	Input* input = Input::get_singleton();

	Button* start_snake = GetNode<Button>("game/ui/MarginContainer/VBoxContainer/play");
	bool is_game_started = self->get_meta("is_game_started");
	if(start_snake->is_pressed() && !is_game_started) {
		start_game(instance);
		self->set_meta("is_game_started", true);
	}

	Button* reset_snake = GetNode<Button>("game/ui/MarginContainer/VBoxContainer/reset");
	if(reset_snake->is_pressed() && is_game_started) {
		self->set_meta("is_game_started", false);
		Timer* timer = GetNode<Timer>("game/Timer");
		timer->stop();
		LineEdit* line_seed = GetNode<LineEdit>("game/ui/MarginContainer/VBoxContainer/seed");
		CheckButton* seed_button = GetNode<CheckButton>("game/ui/MarginContainer/VBoxContainer/seed_button");
		seed_button->set_deferred("disabled", false);
		if(!seed_button->is_pressed()) {
			line_seed->set_text("");
		}

		//clear visuals
		Array snake_matrix = self->get_meta("snake_matrix");
		TileMapLayer* map = GetNode<TileMapLayer>("game/map/TileMapLayer");
		for(int y = 0; y < snake_matrix.size(); y++) {
			Array row = snake_matrix[y];
			for(int x = 0; x < row.size(); x++) {
				int atlas_x = -1;

				//update tile at x, y with correct atlas index
				map->set_cell(Vector2(x - (row.size() / 2), y - (snake_matrix.size() / 2)), 0, Vector2(atlas_x, 0));
			}
		}

		CanvasLayer* game_over = GetNode<CanvasLayer>("game/game_over");
		game_over->set_deferred("visible", false);

		//reset fruit seed
		Node2D* game = GetNode<Node2D>("game");
		game->remove_meta("fruit_seed");
	}

	//listen for inputs and set snake head direction
	if     (input->is_action_just_pressed("W")) self->set_meta("move_dir", 0); //north
	else if(input->is_action_just_pressed("D")) self->set_meta("move_dir", 1); //east
	else if(input->is_action_just_pressed("S")) self->set_meta("move_dir", 2); //south
	else if(input->is_action_just_pressed("A")) self->set_meta("move_dir", 3); //west

	//wait for timer for movement...
}

void start_game(Caller* instance) {
	Node2D* self = GetSelf<Node2D>(instance);

	//start timer
	Timer* timer = GetNode<Timer>("game/Timer");
	timer->call_deferred("start");

	//create matrix to track snake positions
	//	0: empty grid square:
	//	-1: fruit
	//	positive int: int counting down TTL
	LineEdit* line_y = GetNode<LineEdit>("game/ui/MarginContainer/VBoxContainer/HBoxContainer4/y");
	LineEdit* line_x = GetNode<LineEdit>("game/ui/MarginContainer/VBoxContainer/HBoxContainer3/x");
	int grid_y = line_y->get_text().to_int();
	int grid_x = line_x->get_text().to_int();

	Array snake_matrix;
	for(int y = 0; y < grid_y; y++) {
		Array row;
		row.resize(grid_x);
		for(int x = 0; x < grid_x; x++) {
			row[x] = 0;

			if(x == 1 && y == 1) {
				row[x] = 2;
			}
			if(x == 0 && y == 1) {
				row[x] = 1;
			}
		}
		snake_matrix.append(row);
	}

	//initialize head and tail
	Array row = snake_matrix[1];
	row[1] = 2; //head initial TTL
	row[0] = 1; //tail initial TTL
	snake_matrix[1] = row;

	//spawn fruit
	snake_matrix = spawn_fruit(deep_duplicate(snake_matrix));

	//set metadata
	self->set_meta("snake_matrix", snake_matrix);
	self->set_meta("move_dir", 1); //east

	//initialize MCTS
	CheckButton* MCTS_button = GetNode<CheckButton>("game/ui/MarginContainer/VBoxContainer/is_MCTS_playing");
	bool is_MCTS_playing = MCTS_button->is_pressed();
	
	LineEdit* line_iterations = GetNode<LineEdit>("game/ui/MarginContainer/VBoxContainer/HBoxContainer2/MCTS_iterations");
	LineEdit* line_depth = GetNode<LineEdit>("game/ui/MarginContainer/VBoxContainer/HBoxContainer/MCTS_depth");
	int MCTS_iterations = line_iterations->get_text().to_int();
	int MCTS_depth = line_depth->get_text().to_int();
	
	CheckButton* seed_button = GetNode<CheckButton>("game/ui/MarginContainer/VBoxContainer/seed_button");
	seed_button->set_deferred("disabled", true);
	LineEdit* line_seed = GetNode<LineEdit>("game/ui/MarginContainer/VBoxContainer/seed");
	int seed;
	if(line_seed->get_text() == "") {
		seed = random_device{}();
	}
	else {
		seed = line_seed->get_text().to_int();
	}
	line_seed->set_text(String::num_uint64(seed));

	MCTS* MCTS_instance = new MCTS(snake_matrix, MCTS_iterations, MCTS_depth, seed);
	self->set_meta("is_MCTS_playing", is_MCTS_playing);
	self->set_meta("seed", seed);
	self->set_meta("MCTS_instance", reinterpret_cast<uint64_t>(MCTS_instance));

	//track frame number and frame time
	self->set_meta("num_frames", 0);
	auto now = std::chrono::steady_clock::now();
	auto prev_frame_time = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
	self->set_meta("prev_frame_time", prev_frame_time);
}

void on_timer_timeout(Node2D* self) {
	//get player input
	Array snake_matrix = self->get_meta("snake_matrix");
	int move_dir = self->get_meta("move_dir");

	//is MCTS playing?
	bool is_MCTS_playing = self->get_meta("is_MCTS_playing");
	Variant mcts_var = self->get_meta("MCTS_instance");
	MCTS* MCTS_instance = nullptr;
	if(mcts_var.get_type() != Variant::NIL) {
		MCTS_instance = reinterpret_cast<MCTS*>(static_cast<uint64_t>(mcts_var));
	}

	//run MCTS
	if(is_MCTS_playing && MCTS_instance) {
		move_dir = MCTS_instance->run_MCTS();
	}

	//update snake matrix based on input
	snake_matrix = self->get_meta("snake_matrix");
	snake_matrix = move_snake(deep_duplicate(snake_matrix), move_dir);
	self->set_meta("snake_matrix", snake_matrix);

	//update MCTS
	if(is_MCTS_playing && MCTS_instance) {
		MCTS_instance->update(deep_duplicate(snake_matrix), move_dir);
	}

	//update frame counter
	int num_frames = self->get_meta("num_frames");
	num_frames++;
	LineEdit* frame = GetNode<LineEdit>("game/ui/MarginContainer/VBoxContainer/HBoxContainer5/frame");
	frame->set_text(String::num_uint64(num_frames));
	self->set_meta("num_frames", num_frames);

	//track current frame time
	int prev_frame_time = self->get_meta("prev_frame_time");
	auto now = std::chrono::steady_clock::now();
	auto curr_frame_time = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
	LineEdit* frame_time = GetNode<LineEdit>("game/ui/MarginContainer/VBoxContainer/HBoxContainer6/frame_time");
	frame_time->set_text(String::num_uint64(curr_frame_time - prev_frame_time));
	self->set_meta("prev_frame_time", curr_frame_time);

	//find snake length
	double snake_length = 1;
	for(int y = 0; y < snake_matrix.size(); y++) {
		Array row = snake_matrix[y];
		for(int x = 0; x < row.size(); x++) {
			int cell_value = row[x];

			if(cell_value > snake_length) {
				snake_length = cell_value;
			}
		}
	}

	//check if game is over
	bool is_game_won = true;
	int one_counter = 0;
	for(int y = 0; y < snake_matrix.size(); y++) {
		Array row = snake_matrix[y];
		for(int x = 0; x < row.size(); x++) {
			int cell_value = row[x];

			if(cell_value == 0 || cell_value == -1) {
				is_game_won = false;
			}

			//count number of snake segments labeled 1
			if(cell_value == 1) {
				one_counter++;

				//more than one segment labeled 1, state is terminal
				if(one_counter > 1) {
					break;
				}
			}
		}

		if(one_counter > 1) {
			break;
		}
	}

	//game lost
	CanvasLayer* game_over = GetNode<CanvasLayer>("game/game_over");
	Label* label = GetNode<Label>("game/game_over/PanelContainer/Label");
	Timer* timer = GetNode<Timer>("game/Timer");

	if(one_counter > 1) {
		label->set_text("Game Over!");
		game_over->set_deferred("visible", true);
		timer->stop();
		return ;
	}
	
	//game won
	else if(is_game_won) {
		label->set_text("Game Won!");
		game_over->set_deferred("visible", true);
		timer->stop();
	}

	//update visuals using tilemaplayer
	TileMapLayer* map = GetNode<TileMapLayer>("game/map/TileMapLayer");
	for(int y = 0; y < snake_matrix.size(); y++) {
		Array row = snake_matrix[y];
		for(int x = 0; x < row.size(); x++) {
			int cell_value = row[x];
			int atlas_x;
			
			//snake head
			if(cell_value == snake_length) {
				atlas_x = 3;
			}

			//snake cell
			else if(cell_value > 0) {
				atlas_x = 0;
			} 

			//fruit cell
			else if(cell_value == -1) {
				atlas_x = 1;
			} 


			//empty cell
			else {
				atlas_x = 2;
			}

			//update tile at x, y with correct atlas index
			Vector2 tile_coords = Vector2(x - (row.size() / 2), y - (snake_matrix.size() / 2));
			map->set_cell(tile_coords, 0, Vector2(atlas_x, 0));

			//code to change snake color of snake as it moves
			//currently doesn't work as updating the color applies to the entire snake, not individual cells
			/*if(cell_value > 1) {
				TileData* tile = map->get_cell_tile_data(tile_coords);
				double tile_saturation = (cell_value + (snake_matrix.size() * row.size()) - snake_length) / (snake_matrix.size() * row.size());
				UtilityFunctions::print(tile_saturation);
				//UtilityFunctions::print(snake_length);
				tile->set_modulate(Color(1, tile_saturation, 1, 1));
			}*/
		}
	}

	//update score
	LineEdit* score = GetNode<LineEdit>("game/ui/MarginContainer/VBoxContainer/HBoxContainer7/score");
	score->set_text(String::num_uint64(snake_length));
}

// End Jenova Script
JENOVA_SCRIPT_END
