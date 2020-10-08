#include "StoryMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "Sound.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

#include <hb.h>
#include <hb-ft.h>
#include <freetype/freetype.h>
#include <dirent.h>	
#include <fstream>
#include <string>
#include <utility>

std::string parseHeader(std::string &line){
	size_t pos = line.find(" ");
	std::string token = line.substr(0, pos);
	line.erase(0, pos+1);
	return token;
}


Load<Story> transfer_saga(LoadTagDefault, []() -> Story * {
	Story *ret = new Story();
	std::string path = data_path("script");
	std::ifstream script_file(path);
	std::string line;
	if (script_file.is_open()) {
		// the first character is for narration
		ret->characters.emplace_back(std::make_pair("", glm::vec4(1, 1, 1, 1)));
		// read character data, first line would be the number of characters
		std::string str_num_characters;
		getline(script_file, str_num_characters);
		int num_character = std::stoi(str_num_characters);
		// the following n lines are character data with format: character name r g b a
		std::string character_name;
		float r, g, b, a;
		while (num_character--) {
			script_file >> character_name >> r >> g >> b >> a;
			(ret->characters).emplace_back(std::make_pair(character_name, glm::vec4(r, g, b, a)));
		}

		// reading branches of the story
		while (getline(script_file, line)) {
			// first line is the name of story, create a key-value pair in stories map
			Story::Branch branch;
			std::string name = line;
			if (!line.empty()){
				name = parseHeader(line);
				branch.dtime = stoi(parseHeader(line));
				branch.dbudget = stoi(parseHeader(line));
				branch.dfan= stoi(parseHeader(line));
				branch.dcoach = stoi(parseHeader(line));
			}
			
			
			// read the lines and options in this branch
			while (true) {
				getline(script_file, line);
				if (line.length() == 0) {
					// the branch is finished, go to the next one
					break;
				}
				size_t pos = line.find(".");
				int index = std::stoi(line.substr(0, pos));
				
				// this is a line
				if (index >= 0) {
					branch.lines.push_back(Story::Line(index, line.substr(pos+1, line.length() - pos)));
				} 
				// options
				else {
					// int num_options = -index;
					for (int i=0; i<-index; i++){
						std::string option_line, branch_name;
						getline(script_file, option_line);
						getline(script_file, branch_name);
						branch.option_lines.push_back(option_line);
						branch.next_branch_names.push_back(branch_name);
					}
				}
			}
			ret->stories[name] = branch;
		}
	}
	return ret;
});


StoryMode::StoryMode() : story(*transfer_saga) {
	// set the timer and print the first line
	setCurrentBranch(story.stories.at("Menu"));
	info_line = std::make_shared<view::TextLine>(formatStatus(), 50, 650, glm::uvec4(255,255,255,255), 20, std::nullopt, true);

}

StoryMode::~StoryMode() {
}

bool StoryMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		SDL_Keycode keyCode = evt.key.keysym.sym;
		if (keyCode == SDLK_UP) {
			main_dialog->MoveUp();
			return true;
		} else if (keyCode == SDLK_DOWN) {
			main_dialog->MoveDown();
			return true;
		} else if (keyCode == SDLK_RETURN) {
			std::optional<int> next_branch = main_dialog->Enter();
			if (next_branch.has_value()) {
				std::string next_branch_name = current.next_branch_names[next_branch.value()];
				week += story.stories[next_branch_name].dtime;
				if (week >= 8) setCurrentBranch(story.stories["End4"]);
				budget += story.stories[next_branch_name].dbudget;
				
				fan += story.stories[next_branch_name].dfan;
				fan = std::max(0, fan);
				fan = std::min(10, fan);
				coach += story.stories[next_branch_name].dcoach;
				if (week <= 4){
					coach += story.stories[next_branch_name].dcoach;
				}
				coach = std::max(0, coach);
				coach = std::min(10, coach);
				info_line->setText(formatStatus(), std::nullopt);
				if (next_branch_name.compare("JadonYes") && budget < 0){
					budget -= story.stories[next_branch_name].dbudget;
					next_branch_name = "JadonNoMoney";
				}
				budget = std::max(0, budget);
				setCurrentBranch(story.stories[next_branch_name]);
				return true;
			} else {
				return false;
			}
		}
	}
	return false;
}

void StoryMode::update(float elapsed) {
	main_dialog->update(elapsed);
}


std::string StoryMode::formatStatus(){
	return "Week "+std::to_string(week)+"/8    Remaining Budget: $"
	+std::to_string(budget)+"m    Fan Support: "+std::to_string(fan)
	+"/10    Coach Happiness: "+std::to_string(coach)+"/10";
}

void StoryMode::draw(glm::uvec2 const &drawable_size) {
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.
	{
		info_line->draw();
		main_dialog->draw();
	}
	GL_ERRORS();
}


bool StoryMode::show_next_line() {
	if (current.line_idx < current.lines.size()) {
		// show the current line on the screen
		Story::Line current_line = current.lines.at(current.line_idx);
		std::string to_show = story.characters.at(current_line.character_idx).first + " " + current_line.line;
		// reset timer - TODO set it according to the length of the sentence
		// go to next line
		current.line_idx += 1;

		// TODO show it on the screen
		std::cout << to_show << std::endl;
		return true;
	} else {
		if (current.option_lines.size() > 0) {
			if (!option) {
				option = true;
				for (size_t i = 0; i < current.option_lines.size(); ++i) {
					// TODO show options on screen
					std::string option = "\t" + std::to_string(i+1) + " " + current.option_lines[i];
					std::cout  << option << std::endl;
				}
			}
			return true;
		}
	}
	return false;
}

void StoryMode::setCurrentBranch(const Story::Branch &new_branch) {
	current = new_branch;
	option = true;
	std::vector<std::pair<glm::uvec4, std::string>> prompts;
	for (const auto &line : current.lines) {
		glm::uvec4 color = glm::uvec4(story.characters.at(line.character_idx).second * 255.0f);
		std::string to_show = story.characters.at(line.character_idx).first + " " + line.line;
		prompts.emplace_back(color, to_show);
	}
	main_dialog = std::make_shared<view::Dialog>(prompts, current.option_lines);
}
