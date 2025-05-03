// Godot SDK
#include <Godot/godot.hpp>
#include <iostream>
#include <vector>
#include <cmath>
#include <functional>
#include <memory>
#include <random>
#include <Godot/variant/array.hpp>
#include <Godot/variant/utility_functions.hpp>

// Jenova SDK
#include <JenovaSDK.h>
#include <headers/snake_functions.hpp>
#include <headers/MCTS.hpp>

// Namespaces
using namespace godot;
using namespace jenova::sdk;
using namespace std;

MCTS::MCTS(Array snake_matrix, int max_iterations, int max_rollout_depth, int gen_seed, double exploration_constant)
	:   root(make_shared<Node>(nullptr, snake_matrix)), //initialize root
		max_iterations(max_iterations),
		max_rollout_depth(max_rollout_depth),
		gen((gen_seed == -1) ? random_device{}() : gen_seed), //initialize random number generator
		exploration_constant(exploration_constant) {}
		
int MCTS::run_MCTS() {
	//run MCTS for set number of iterations
	for(int i = 0; i < max_iterations; i++) {
		selection();
	}

	//return best action after runtime completes
	if(root->children.empty()) {
		UtilityFunctions::print("MCTS REACHED END OF GAME");
		return -1;
	}

	//best action is chosen from the child with the most visits, the best action will be a child node of root
	vector<shared_ptr<Node>> best_children = {root->children[0]};
	for(int i = 1; i < root->children.size(); i++) {
		if(best_children[0]->total_visits < root->children[i]->total_visits) { //select child node with most visits
			best_children = {root->children[i]};
		}
		else if(best_children[0]->total_visits == root->children[i]->total_visits) { //if multiple best childen then add as candidate
			best_children.push_back(root->children[i]);
		}
	}

	//randomly select a best child if multiple best children exist
	uniform_int_distribution<int> dis(0, best_children.size() - 1);
	shared_ptr<Node> best_child = best_children[dis(gen)];

	return best_child->action;
}

void MCTS::update(Array snake_matrix, int played_action) {
	Vector2 prev_fruit_pos, curr_fruit_pos;
	for(int y = 0; y < snake_matrix.size(); y++) {
		Array row_prev = root->state[y];
		Array row_curr = snake_matrix[y];
		for(int x = 0; x < row_prev.size(); x++) {
			int cell_value_prev = row_prev[x];
			int cell_value_curr = row_curr[x];
			
			//find both fruit positions
			if(cell_value_prev == -1) {
				prev_fruit_pos = Vector2(x, y);
			}

			if(cell_value_curr == -1) {
				curr_fruit_pos = Vector2(x, y);
			}
		}
	}

	//soft update, matching fruit positions
	/*if(prev_fruit_pos == curr_fruit_pos) {
		for(auto& child : root->children) {
			if(child->action == played_action) {
				root = child;
				root->parent.reset();

				return;
			}
		}
	}

	//hard update, non-matching fruit positions
	else {
		root = make_shared<Node>(nullptr, snake_matrix);
	}*/
	root = make_shared<Node>(nullptr, snake_matrix);
}

void MCTS::selection() {
	shared_ptr<Node> current_node = root;
	
	//select the best node balancing exploration and expansion
	while(!current_node->children.empty()) {
		double best_UCT = -numeric_limits<double>::infinity();
		vector<shared_ptr<Node>> best_children;

		//calculate UCT (Upper Confidence bounds applied to Trees) for each node
		for(const auto& child : current_node->children) {
			double UCT;

			//prioritize children with no visits
			if(child->total_visits == 0) {
				UCT = numeric_limits<double>::infinity();
			}
			else {
				double exploitation = child->total_reward / child->total_visits;
				double exploration = sqrt(log(current_node->total_visits) / child->total_visits);
				UCT = exploitation + exploration_constant * exploration;
			}

			//new best UCT found, update candidate values
			if(UCT > best_UCT) {
				best_UCT = UCT;
				best_children = {child};
			}
			else if(UCT == best_UCT) {
				best_children.push_back(child);
			}
		}
		
		//continue down the tree until a leaf node is reached
		uniform_int_distribution<int> dis(0, best_children.size() - 1);
		current_node = best_children[dis(gen)];
	}

	//node selected, move to expansion
	expansion(current_node);
}

void MCTS::expansion(shared_ptr<Node> node) {
	//node is terminal, do not expand, immediately evaluate and backpropagate
	if(is_terminal(node->state)) {
		double reward = evaluate_state(node->state, node->state);
		backpropagation(node, reward);

		return ;
	}

	//get possible actions and add each as a child node to current node
	vector<int> possible_actions = get_possible_actions(node->state);
	for(int action : possible_actions) {
		Array next_state = move_snake(deep_duplicate(node->state), action); //simulate each possible action and get the next game state
		shared_ptr<Node> child_node = make_shared<Node>(node, next_state, action); //create child node containing the next game state
		node->children.push_back(child_node); //add child node to current node
	}

	//choose a random child to perform a rollout
	if(!node->children.empty()) {
		uniform_int_distribution<int> dis(0, node->children.size() - 1);
		shared_ptr<Node> random_child = node->children[dis(gen)];

		//random child selected, call rollout
		rollout(random_child);
	}
}

void MCTS::rollout(shared_ptr<Node> node) {
	//rollout perfoms random playouts from node to begin node evaluation
	Array start_state = node->state;
	Array end_state = start_state;

	int rollout_iterations = 0;
	while(!is_terminal(end_state) && rollout_iterations != max_rollout_depth) {
		//perform random playout from possible actions
		vector<int> possible_actions = get_possible_actions(start_state);

		//randomly select an action
		uniform_int_distribution<int> dis(0, possible_actions.size() - 1);
		int random_action = possible_actions[dis(gen)];

		//simulate selected action and update the current state
		end_state = move_snake(deep_duplicate(start_state), random_action);
		rollout_iterations++;
	}
	
	//evaluate final node state from random playout and backpropogate the result
	double result = evaluate_state(start_state, end_state);
	backpropagation(node, result);
}

void MCTS::backpropagation(shared_ptr<Node> node, double simulation_reward) {
	//backpropagate to every node up to the root node 
	while(node) {
		node->total_visits++;
		node->total_reward += simulation_reward;
		node = node->parent.lock();
	}
}

bool MCTS::is_terminal(Array snake_matrix) {
	int one_counter = 0;
	for(int y = 0; y < snake_matrix.size(); y++) {
		Array row = snake_matrix[y];
		for(int x = 0; x < row.size(); x++) {
			int cell_value = row[x];
			
			//count number of snake segments labeled 1
			if(cell_value == 1) {
				one_counter++;

				//more than one segment labeled 1, state is terminal
				if(one_counter > 1) {
					return true;
				}
			}
		}
	}

	return false;
}

double MCTS::evaluate_state(Array start_state, Array end_state) {
	if(start_state == end_state ) {
		return 0.0;
	}

	Vector2 head_pos, fruit_pos;

	double snake_length_start = 0;
	double snake_length_end = 0;
	for(int y = 0; y < start_state.size(); y++) {
		Array row_start = start_state[y];
		Array row_end = end_state[y];
		for(int x = 0; x < row_start.size(); x++) {
			int cell_value_start = row_start[x];
			int cell_value_end = row_end[x];
			
			//find snake length and head position
			if(cell_value_start > snake_length_start) {
				snake_length_start = cell_value_start;
			}

			if(cell_value_end > snake_length_end) {
				snake_length_end = cell_value_end;
				head_pos = Vector2(x, y);
			}

			//find fruit position
			if(cell_value_end == -1) {
				fruit_pos = Vector2(x, y);
			}
		}
	}

	//calculate reward
	double length_reward;
	double fruit_reward;

	Array row = start_state[0];
	double max_dist = start_state.size() * row.size();

	double reward;
	if(snake_length_end == start_state.size() * row.size()) {
		reward = 76.0;
	}
	else if(snake_length_end > snake_length_start) {
		reward = snake_length_end / max_dist;
	}
	else {
		double dist = abs(head_pos.x - fruit_pos.x) + abs(head_pos.y - fruit_pos.y); //manhattan distance
		reward = (max_dist * snake_length_start + max_dist - dist) / (max_dist * start_state.size() * row.size());
	}

	//UtilityFunctions::print("REWARD: " + String::num_real(reward));
	return reward;
}

vector<int> MCTS::get_possible_actions(Array snake_matrix) {
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
		return {};
	}

	//find direction snake moved previously
	int prev_move_dir;
	Vector2 movement_delta = head_pos - prev_head_pos;
	if     (movement_delta == Vector2(0, -1)) prev_move_dir = 0; //prev move north
	else if(movement_delta == Vector2(1, 0))  prev_move_dir = 1; //prev move east
	else if(movement_delta == Vector2(0, 1))  prev_move_dir = 2; //prev move south
	else if(movement_delta == Vector2(-1, 0)) prev_move_dir = 3; //prev move west

	//return all moves excluding 180 degree turn
	vector<int> action_vec;
	for(int i = 0; i < 4; i++) {
		if(i == (prev_move_dir + 2) % 4) {
			continue;
		}
		else {
			action_vec.push_back(i);
		}
	}

	//allow snake to win
	if(snake_length == snake_matrix.size() * row.size() - 1) {
		return action_vec;
	}

	//only return safe moves, do not allow snake to move into death unless only move available
	vector<int> safe_moves;
	for(int action : action_vec) {
		Array potential_safe_move = move_snake(deep_duplicate(snake_matrix), action);

		//check if move is safe
		if(!is_terminal(potential_safe_move)) {
			safe_moves.push_back(action);
		}
	}

	if(!safe_moves.empty()) {
		return safe_moves;
	}
	else {
		return action_vec;
	}
}
