#include "puppeteer.hh"
#include "utilities.hh"

GZ_REGISTER_WORLD_PLUGIN(Puppeteer);

//PUPPETEER CLASS

void Puppeteer::Load(gazebo::physics::WorldPtr _world, sdf::ElementPtr _sdf){

    this->world = _world;
    this->sdf = _sdf;
    this->update_connection = gazebo::event::Events::ConnectWorldUpdateBegin(std::bind(&Puppeteer::OnUpdate, this, std::placeholders::_1));

    this->ReadSDF();

    this->ReadParams();

    auto building = this->world->ModelByName(this->building_name);

    this->building_box = building->BoundingBox();
    this->building_box.Min().X()-=1;
    this->building_box.Min().Y()-=1;
    this->building_box.Max().X()+=1;
    this->building_box.Max().Y()+=1;
    this->static_quadtree = boost::make_shared<QuadTree>(this->building_box);
    this->vehicle_quadtree = boost::make_shared<QuadTree>(this->building_box);

    for (unsigned int i = 0; i < world->ModelCount(); ++i) {
        auto model = world->ModelByIndex(i);

        auto act = boost::dynamic_pointer_cast<gazebo::physics::Actor>(model);

        if (act){
            
            auto new_vehicle = this->CreateVehicle(act);
            this->vehicles.push_back(new_vehicle);
            auto min = ignition::math::Vector3d(new_vehicle->GetPose().Pos().X() - 0.4, new_vehicle->GetPose().Pos().Y() - 0.4, 0);
            auto max = ignition::math::Vector3d(new_vehicle->GetPose().Pos().X() + 0.4, new_vehicle->GetPose().Pos().Y() + 0.4, 0);
            auto box = ignition::math::Box(min,max);
            auto new_node = QTData(box, new_vehicle, vehicle_type);
            this->vehicle_quadtree->Insert(new_node);
            continue;
        } 
      
        if (model->GetName() == this->building_name){
            auto links = model->GetLinks();
            for (gazebo::physics::LinkPtr link: links){
                std::vector<gazebo::physics::CollisionPtr> collision_boxes = link->GetCollisions();
                for (gazebo::physics::CollisionPtr collision_box: collision_boxes){
                    this->collision_entities.push_back(collision_box); //TODO: check if this is correct (maybe do dynamic pointer cast )
                    auto new_node = QTData(collision_box->BoundingBox(), collision_box, entity_type);
                    this->static_quadtree->Insert(new_node);
                }
                    
            }
        } else if (model->GetName() != "ground_plane"){
            this->collision_entities.push_back(model);
            auto box = model->BoundingBox();
            box.Min().Z() = 0;
            box.Max().Z() = 0;
            auto new_node = QTData(box, model, entity_type);
            this->static_quadtree->Insert(new_node);
        }
        
    }

    for (auto vehicle: this->follower_queue){
        vehicle->LoadLeader(this->vehicles);
    }

}

void Puppeteer::OnUpdate(const gazebo::common::UpdateInfo &_info){
    double dt = (_info.simTime - this->last_update).Double();

    if (dt < 1/this->update_freq){
        return;
    }

    this->last_update = _info.simTime;

    // reconstruct vehicle_quad tree

    this->vehicle_quadtree = boost::make_shared<QuadTree>(this->building_box);

    for (auto vehicle: this->vehicles){
        auto min = ignition::math::Vector3d(vehicle->GetPose().Pos().X() - 0.4, vehicle->GetPose().Pos().Y() - 0.4, 0);
        auto max = ignition::math::Vector3d(vehicle->GetPose().Pos().X() + 0.4, vehicle->GetPose().Pos().Y() + 0.4, 0);
        auto box = ignition::math::Box(min,max);
        auto new_node = QTData(box, vehicle, vehicle_type);
        this->vehicle_quadtree->Insert(new_node);
    }

    for (auto vehicle: this->vehicles){
        // query quad tree
        std::vector<boost::shared_ptr<Vehicle>> near_vehicles;

        std::vector<gazebo::physics::EntityPtr> near_objects;

        auto min = ignition::math::Vector3d(vehicle->GetPose().Pos().X() - 2, vehicle->GetPose().Pos().Y() - 2, 0);
        auto max = ignition::math::Vector3d(vehicle->GetPose().Pos().X() + 2, vehicle->GetPose().Pos().Y() + 2, 0);
        auto query_range = ignition::math::Box(min,max);

        std::vector<QTData> query_objects = this->static_quadtree->QueryRange(query_range);
        for (auto n: query_objects){
            if (n.type == entity_type){
                near_objects.push_back(boost::static_pointer_cast<gazebo::physics::Entity>(n.data));
            }
            
        }
        std::vector<QTData> query_vehicles = this->vehicle_quadtree->QueryRange(query_range);
        for (auto n: query_vehicles){
            if (n.type == vehicle_type){
                near_vehicles.push_back(boost::static_pointer_cast<Vehicle>(n.data));
            }
        }

        vehicle->OnUpdate(_info, dt, near_vehicles, near_objects);
    }
}

void Puppeteer::ReadSDF(){
    if (this->sdf->HasElement("building_name")){
        this->building_name =this->sdf->GetElement("building_name")->Get<std::string>();
    }
}

boost::shared_ptr<Vehicle> Puppeteer::CreateVehicle(gazebo::physics::ActorPtr actor){

    boost::shared_ptr<Vehicle> res;
   
    auto sdf = actor->GetSDF();

    std::map<std::string, std::string> actor_info; 

    auto attribute = sdf->GetElement("plugin");
        
	while (attribute){
		actor_info[attribute->GetAttribute("name")->GetAsString()] = attribute->GetAttribute("filename")->GetAsString();
		attribute = attribute->GetNextElement("plugin");
	}

    double max_speed = 1;

    if (actor_info.find("max_speed") != actor_info.end()){
        try{
            max_speed = std::stod(actor_info["max_speed"]);
        } catch (std::exception& e){
            std::cout << "Error converting max_speed argument to double" << std::endl;
        }
    }

    if (actor_info.find("vehicle_type")!=actor_info.end()){
        if (actor_info["vehicle_type"] == "wanderer"){

            res = boost::make_shared<Wanderer>(actor, this->vehicle_params["mass"], this->vehicle_params["max_force"], max_speed, actor->WorldPose(), ignition::math::Vector3d(0,0,0), this->collision_entities);
        
        } else if (actor_info["vehicle_type"] == "random_walker"){
            res = boost::make_shared<RandomWalker>(actor, this->vehicle_params["mass"], this->vehicle_params["max_force"], max_speed, actor->WorldPose(), ignition::math::Vector3d(0,0,0), this->collision_entities);
            
        } else if (actor_info["vehicle_type"] == "boid"){
            auto random_vel = ignition::math::Vector3d(ignition::math::Rand::DblUniform(-1,1),ignition::math::Rand::DblUniform(-1,1),0);
            random_vel.Normalize(); 
            random_vel*=2;
            res = boost::make_shared<Boid>(actor, this->vehicle_params["mass"], this->vehicle_params["max_force"], max_speed, actor->WorldPose(), random_vel, this->collision_entities, this->boid_params["alignement"], this->boid_params["cohesion"], this->boid_params["separation"], this->boid_params["FOV_angle"], this->boid_params["FOV_radius"]); // read in as params 
            
        } else if (actor_info["vehicle_type"] == "stander"){
            
            double standing_duration = 5;
            double walking_duration = 5;

            if (actor_info.find("standing_duration") != actor_info.end()){
                try{
                    standing_duration = std::stod(actor_info["standing_duration"]);
                } catch (std::exception& e){
                    std::cout << "Error converting standing duration argument to double" << std::endl;
                }
            }

            if (actor_info.find("walking_duration") != actor_info.end()){
                try{
                    walking_duration = std::stod(actor_info["walking_duration"]);
                } catch (std::exception& e){
                    std::cout << "Error converting walking duration argument to double" << std::endl;
                }
            }

            res = boost::make_shared<Stander>(actor, 1, 10, max_speed, actor->WorldPose(), ignition::math::Vector3d(0,0,0), this->collision_entities, standing_duration, walking_duration); // read in as params 
            
        } else if (actor_info["vehicle_type"] == "sitter"){

            std::string chair = "";

            if (actor_info.find("chair") != actor_info.end()){
                chair = actor_info["chair"];
            }

            res = boost::make_shared<Sitter>(actor, chair, this->collision_entities, actor->WorldPose().Pos().Z());

        } else if (actor_info["vehicle_type"] == "follower"){

            std::string leader_name = "";
            
            if (actor_info.find("leader") != actor_info.end()){
                leader_name = actor_info["leader"];
               
                res = boost::make_shared<Follower>(actor, 1, 10, max_speed, actor->WorldPose(), ignition::math::Vector3d(0,0,0), this->collision_entities, leader_name); // read in as params 
                this->follower_queue.push_back(boost::dynamic_pointer_cast<Follower>(res));
            } else{
                std::cout << "leader name not found\n";
            }
        }
    }
    

    return res;
}

void Puppeteer::ReadParams(){
    int argc = 0;
    char **argv = NULL;
    ros::init(argc, argv, "Puppeteer");
    ros::NodeHandle nh;

    if (!nh.getParam("common_vehicle_params", this->vehicle_params)){
        ROS_ERROR("ERROR READING COMMON VEHICLE PARAMS");
        vehicle_params["mass"] = 1;
        vehicle_params["max_force"] = 10;
        vehicle_params["slowing_distance"] =  2;
        vehicle_params["arrival_distance"] = 0.5;
        vehicle_params["obstacle_margin"] = 0.6;
    }

    if (!nh.getParam("common_boid_params", this->boid_params)){
        ROS_ERROR("ERROR READING COMMON BOID PARAMS");
        boid_params["alignement"] =  0.1;
        boid_params["cohesion"] =  0.01;
        boid_params["separation"] =  2;
        boid_params["FOV_angle"] =  4;
        boid_params["FOV_radius"] =  3;
    }
}