#include "world_factory.hh"
#include <iterator>
#include <algorithm>   
#include <ctime>        
#include <cstdlib> 
#include <iostream>
#include <fstream>

int main(int argc, char ** argv){

    ROS_WARN("WORLD GENERATION BEGINNING:");

    auto world_handler = WorldHandler();

    world_handler.Load();
    

    return 0;
}

void WorldHandler::Load(){

    ROS_INFO("Loading Parameters");

    this->LoadParams();

    ROS_INFO("Filling Rooms");
    
    for (auto r_info: this->rooms){
        this->FillRoom(r_info);
        
        r_info->room->AddToWorld(this->world_string);

    }

    ROS_INFO("Writing to file");

    this->WriteToFile("myhal_sim.world");

    ROS_WARN("WORLD CREATED!");
    std::cout << "WORLD CREATED\n";
}

WorldHandler::WorldHandler(){
    this->world_string = "";
}

void WorldHandler::LoadParams(){

    //TODO: fix error handelling 

    int argc = 0;
    char **argv = NULL;
    ros::init(argc, argv, "WorldHandler");
    ros::NodeHandle nh;


    /// READ PLUGIN INFO
    
    std::vector<std::string> plugin_names;
    if (!nh.getParam("plugin_names", plugin_names)){
        ROS_ERROR("ERROR READING PLUGIN NAMES");
        return;
    }

    for (auto name: plugin_names){
        //std::cout << name << std::endl;
        
        std::map<std::string, std::string> info;
        if (!nh.getParam(name, info)){
            ROS_ERROR("ERROR READING PLUGIN PARAMS");
            return;
        }

        std::shared_ptr<SDFPlugin> plugin = std::make_shared<SDFPlugin>(info["name"], info["filename"]);

        std::map<std::string, std::string>::iterator it;

        for ( it = info.begin(); it != info.end(); it++ ){
            if (it->first == "name"){
                continue;
            }

            if (it->first == "max_speed"){
                double speed = std::stod(info[it->first]);
                speed += ignition::math::Rand::DblUniform(-speed/5, speed/5);
                plugin->AddSubtag(it->first, std::to_string(speed));
            } else{
                plugin->AddSubtag(it->first, info[it->first]);
            }
            
        }
        this->vehicle_plugins[name] = plugin;
    }

    /// READ ANIMATION INFO

    std::vector<std::string> animation_names;
    if (!nh.getParam("animation_names", animation_names)){
        ROS_ERROR("ERROR READING ANIMATION NAMES");
        return;
    }

    for (auto name: animation_names){
        //std::cout << name << std::endl;
        
        std::map<std::string, std::string> info;
        if (!nh.getParam(name, info)){
            ROS_ERROR("ERROR READING ANIMATION PARAMS");
            return;
        }

        std::shared_ptr<SDFAnimation> animation = std::make_shared<SDFAnimation>(name, info["filename"], true);

        this->animation_list.push_back(animation);
       
    }

    /// READ MODEL INFO

    std::vector<std::string> model_names;
    if (!nh.getParam("model_names", model_names)){
        ROS_ERROR("ERROR READING MODEL NAMES");
        return;
    }

    for (auto name: model_names){
      
        
        std::map<std::string, std::string> info;
        if (!nh.getParam(name, info)){
            ROS_ERROR("ERROR READING MODEL PARAMS");
            return;
        }

        std::shared_ptr<ModelInfo> m_info = std::make_shared<ModelInfo>(name, info["filename"], std::stod(info["width"]), std::stod(info["length"]));
        this->model_info[name] = m_info;
    }

    /// READ TABLE INFO

    std::vector<std::string> table_group_names;
    if (!nh.getParam("table_group_names", table_group_names)){
        ROS_ERROR("ERROR READING TABLE GROUP NAMES");
        return;
    }

    for (auto name: table_group_names){
        std::map<std::string, std::string> info;
    

        if (!nh.getParam(name,info)){
            ROS_ERROR("ERROR READING TABLE GROUP PARAMS");
            return;
        }

        std::shared_ptr<TableInfo> t_info = std::make_shared<TableInfo>(name, info["table"], info["chair"]);
        this->table_info[name] = t_info;
    }


    /// READ ACTOR INFO 

    std::vector<std::string> actor_names;
    if (!nh.getParam("actor_names", actor_names)){
        ROS_ERROR("ERROR READING ACTOR NAMES");
        return;
    }

    for (auto name: actor_names){
        
        std::map<std::string, std::string> info;
        if (!nh.getParam(name, info)){
            ROS_ERROR("ERROR READING ACTOR PARAMS");
            return;
        }

        std::shared_ptr<ActorInfo> a_info = std::make_shared<ActorInfo>(name, info["filename"], info["plugin"], std::stod(info["obstacle_margin"]));

        this->actor_info[name] = a_info;
    }

    /// READ SCENARIO INFO

    std::vector<std::string> scenario_names;
    if (!nh.getParam("scenario_names", scenario_names)){
        ROS_ERROR("ERROR READING SCENARIO NAMES");
        return;
    }

    for (auto name: scenario_names){

        
        std::map<std::string, std::string> info;
        if (!nh.getParam(name, info)){
            ROS_ERROR("ERROR READING SCENARIO PARAMS");
            return;
        }

        std::shared_ptr<Scenario> scenario = std::make_shared<Scenario>(std::stod(info["pop_density"]), std::stod(info["table_percentage"]), info["actor"]);

        std::vector<std::string> model_list; 

        if (!nh.getParam(info["model_list"], model_list)){
            ROS_ERROR("ERROR READING MODEL LIST");
            return;
        }


        for (auto model_name: model_list){
            scenario->AddModel(this->model_info[model_name]);
        }


        std::vector<std::string> table_group_list; 

        if (!nh.getParam(info["table_group_list"], table_group_list)){
            ROS_ERROR("ERROR READING TABLE GROUP LIST");
            return;
        }


        for (auto table_group_name: table_group_list){
            scenario->AddTable(this->table_info[table_group_name]);
        }

        this->scenarios[name] = scenario;
    }


    /// READ ROOM INFO

    std::vector<std::string> room_names;
    if (!nh.getParam("room_names", room_names)){
        ROS_ERROR("ERROR READING ROOM NAMES");
        return;
    }

    for (auto name: room_names){
        //std::cout << name << std::endl;
        
        std::map<std::string, std::string> info;
        if (!nh.getParam(name, info)){
            ROS_ERROR("ERROR READING ROOM PARAMS");
            return;
        }

        std::map<std::string, double> geometry; 

        if (!nh.getParam(info["geometry"], geometry)){
            ROS_ERROR("ERROR READING ROOM GEOMETRY");
            return;
        }

        std::shared_ptr<myhal::Room> room = std::make_shared<myhal::Room>(geometry["x_min"], geometry["y_min"], geometry["x_max"], geometry["y_max"], (bool) std::stoi(info["enclosed"]));

        std::vector<double> poses;

        if (!nh.getParam(info["positions"], poses)){
            ROS_ERROR("ERROR READING POSITION PARAMS");
            return;
        }
        std::vector<std::vector<double>> positions;
        for (int j =0; j < (int) poses.size()-1; j+=2){
            positions.push_back({poses[j],poses[j+1]});
        }
  

        auto r_info = std::make_shared<RoomInfo>(room, info["scenario"], positions);
        this->rooms.push_back(r_info);
        
    }
}

void WorldHandler::FillRoom(std::shared_ptr<RoomInfo> room_info){

    auto scenario = this->scenarios[room_info->scenario];
    
    int num_models = (int) (scenario->model_percentage*((room_info->positions.size())));
    
    std::random_shuffle(room_info->positions.begin(), room_info->positions.end());
    
    for (int i = 0; i < num_models; ++i){
        //auto m_info = scenario->GetRandomModel();

        auto t_info = scenario->GetRandomTable();

        auto random_pose = ignition::math::Pose3d(room_info->positions[i][0], room_info->positions[i][1], 0, 0, 0, 0); //TODO: specify randomization parameters in yaml
        if (t_info){
            //auto new_model = std::make_shared<myhal::IncludeModel>(m_info->name, random_pose, m_info->filename, m_info->width, m_info->length);
            auto t_model_info = this->model_info[t_info->table_name];
            auto c_model_info = this->model_info[t_info->chair_name];
            auto table_model = std::make_shared<myhal::IncludeModel>(t_model_info->name, random_pose, t_model_info->filename, t_model_info->width, t_model_info->length);
            auto chair_model = std::make_shared<myhal::IncludeModel>(c_model_info->name, random_pose, c_model_info->filename, c_model_info->width, c_model_info->length);

            double rotation = 1.5707 * ignition::math::Rand::IntUniform(0,1);
            auto table_group = std::make_shared<myhal::TableGroup>(table_model, chair_model, ignition::math::Rand::IntUniform(0,4), rotation); 
            room_info->room->AddModel(table_group->table_model);
            for (auto chair: table_group->chairs){
                room_info->room->AddModel(chair);
            }
            
        } 
    }


    int num_actors = (int) ((scenario->pop_density)*(room_info->room->Area()));
    auto a_info = this->actor_info[scenario->actor];
    auto plugin = this->vehicle_plugins[a_info->plugin];

    for (int i =0; i<num_actors; i++){
        auto new_actor = std::make_shared<myhal::Actor>(a_info->name, ignition::math::Pose3d(0,0,1,0,0,ignition::math::Rand::DblUniform(0,6.28)), a_info->filename, a_info->width, a_info->length); //TODO randomize initial Rot

        for (auto animation: this->animation_list){
            new_actor->AddAnimation(animation);
            
        }
        
        new_actor->AddPlugin(plugin);
        
        
       
        room_info->room->AddModelRandomly(new_actor);
    }

}


void WorldHandler::WriteToFile(std::string out_name){
    std::ifstream in = std::ifstream("/home/default/catkin_ws/src/myhal_simulator/worlds/myhal_template.txt");

    if (in){
        ROS_INFO("TEMPLATE FILE FOUND");
	} else{
        ROS_ERROR("TEMPLATE FILE NOT FOUND");
        return;
	}

    std::ofstream out;
    out.open("/home/default/catkin_ws/src/myhal_simulator/worlds/" + out_name);

    char str[255];
	int line =0;

	while(in) {
		in.getline(str, 255);  // delim defaults to '\n'
		if(in) {
			
			if (line == 102){
				// insert writing people and furnature here
				
				out << this->world_string;
			}

			out << str << std::endl;
			line++;

		}
	}	

    out.close();
    in.close();
}

Scenario::Scenario(double _pop_density, double _model_percentage, std::string _actor){
    this->pop_density = _pop_density;
    this->model_percentage = _model_percentage;
    this->actor = _actor;
}

void Scenario::AddModel(std::shared_ptr<ModelInfo> model){
    this->models.push_back(model);
}

void Scenario::AddTable(std::shared_ptr<TableInfo> table){
    this->tables.push_back(table);
}

std::shared_ptr<ModelInfo> Scenario::GetRandomModel(){
    if (this->models.size() <= 0){
        std::cout << "ERROR NO MODEL FOUND" << std::endl;
        return nullptr;
    }
   
    int i = ignition::math::Rand::IntUniform(0, this->models.size()-1);

    return this->models[i];
}

std::shared_ptr<TableInfo> Scenario::GetRandomTable(){
    if (this->tables.size() <= 0){
        std::cout << "ERROR NO TABLE FOUND" << std::endl;
        return nullptr;
    }
   
    int i = ignition::math::Rand::IntUniform(0, this->tables.size()-1);

    return this->tables[i];
}