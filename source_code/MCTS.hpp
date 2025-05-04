#pragma once

// Godot SDK
#include <Godot/godot.hpp>
#include <iostream>
#include <vector>
#include <cmath>
#include <functional>
#include <memory>
#include <random>
#include <Godot/variant/array.hpp>

// Jenova SDK
#include <JenovaSDK.h>
#include <headers/snake_functions.hpp>
#include <headers/MCTS.hpp>

// Namespaces
using namespace godot;
using namespace jenova::sdk;
using namespace std;

class MCTS {
private:
	//MCTS node structure
	struct Node {
		//tree structure
		weak_ptr<Node> parent;
		vector<shared_ptr<Node>> children;

		//node values
		int total_visits;
		double total_reward;
		Array state;
		int action;

		//node constructor
		Node(shared_ptr<Node> parent = nullptr, Array node_state = Array(), int action = -1) 
		:   parent(parent), total_visits(0), total_reward(0), state(node_state), action(action) {}
	};

	//MCTS assorted values
	shared_ptr<Node> root;
	int max_iterations; //number of nodes to be explored before selecting best node
	int max_rollout_depth; //number of moves to play before terminating rollout
	double exploration_constant; //constant used in the selection function, default to sqrt(2) -> optimal for rewards in [0, 1]
	mt19937 gen; //random number generator

	//MCTS core functionality
	void selection();
	void expansion(shared_ptr<Node> node);
	void rollout(shared_ptr<Node> node);
	void backpropagation(shared_ptr<Node> node, double simulation_result);

	//MCTS additional functionality
	bool is_max_length(Array snake_matrix);
	bool is_terminal(Array snake_matrix);
	double evaluate_state(Array start_state, Array end_state);
	vector<int> get_possible_actions(Array snake_matrix);

public:
	int run_MCTS(); //returns best action
	void update(Array snake_matrix, int played_action); //update MCTS root

	//MCTS constructor default values
	MCTS(Array snake_matrix = Array(), 
		int max_iterations = 100,
		int max_rollout_depth = 100,
		int gen_seed = -1,
		double exploration_constant = sqrt(2));
};
