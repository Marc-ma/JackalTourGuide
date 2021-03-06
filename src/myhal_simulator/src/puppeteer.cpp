#include "puppeteer.hh"
#include "utilities.hh"
#include "parse_tour.hh"
#include <ros/forwards.h>



GZ_REGISTER_WORLD_PLUGIN(Puppeteer);

//PUPPETEER CLASS

void Puppeteer::Load(gazebo::physics::WorldPtr _world, sdf::ElementPtr _sdf){
    //note: world name is default

    this->world = _world;

   
    
    this->sdf = _sdf;
    this->update_connection = gazebo::event::Events::ConnectWorldUpdateBegin(std::bind(&Puppeteer::OnUpdate, this, std::placeholders::_1));

    this->user_name = "default";
    if (const char * user = std::getenv("USER")){
        this->user_name = user;
    } 

    this->ReadSDF();

    this->ReadParams();
     
    path_pub = nh.advertise<geometry_msgs::PoseStamped>("optimal_path",1000);

    auto building = this->world->ModelByName(this->building_name);

    this->building_box = building->BoundingBox();
    this->building_box.Min().X()-=1;
    this->building_box.Min().Y()-=1;
    this->building_box.Max().X()+=1;
    this->building_box.Max().Y()+=1;
    //std::printf("%f, %f, %f, %f\n", building_box.Min().X(), building_box.Min().Y(), building_box.Max().X(), building_box.Max().Y());
    //std::cout << "min_x: " << building_box.Min().X() << " max_y: " << building_box.Max().Y() << std::endl;
    this->static_quadtree = boost::make_shared<QuadTree>(this->building_box);
    this->vehicle_quadtree = boost::make_shared<QuadTree>(this->building_box);
    this->costmap = boost::make_shared<Costmap>(this->building_box, 0.2);

    
    
    happly::PLYData static_objects;
    std::vector<BoxObject> boxes;

    
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

       
        if (model->GetName() != "ground_plane"){
            
            auto links = model->GetLinks();
            for (gazebo::physics::LinkPtr link: links){
                std::vector<gazebo::physics::CollisionPtr> collision_boxes = link->GetCollisions();
                for (gazebo::physics::CollisionPtr collision_box: collision_boxes){

                    
                    
                    this->collision_entities.push_back(collision_box);
                    auto box = collision_box->BoundingBox();
                    boxes.push_back(BoxObject(box, -1));
                    
                    box.Max().Z() = 0;
                    box.Min().Z() = 0;
                    
                    auto new_node = QTData(box, collision_box, entity_type);
                    this->static_quadtree->Insert(new_node);

                    if (collision_box->BoundingBox().Min().Z() < 1.5 && collision_box->BoundingBox().Max().Z() > 10e-2){
                        box.Min().X()-=0.2;
                        box.Min().Y()-=0.2;
                        box.Max().X()+=0.2;
                        box.Max().Y()+=0.2;
                        this->costmap->AddObject(box);
                    }
                    
                    
                }
                    
            }
            
        }

    
       
    }

    AddBoxes(static_objects, boxes);
    if (this->start_time != ""){
        static_objects.write("/home/" + this->user_name + "/Myhal_Simulation/simulated_runs/" + this->start_time + "/logs-" +this->start_time +"/static_objects.ply", happly::DataFormat::ASCII);
    }

    // find and publish optimal route

    TourParser parser = TourParser(this->tour_name);
    auto route = parser.GetRoute();
    route.insert(route.begin(), ignition::math::Pose3d(ignition::math::Vector3d(0,0,0), ignition::math::Quaterniond(0,0,0,1)));



    for (int i =0; i< route.size()-1; i++){
        auto start = route[i];
        auto end = route[i+1];

        std::vector<ignition::math::Vector3d> path;
        this->costmap->ThetaStar(start.Pos(), end.Pos(), path);
        
        paths.push_back(path);
        //paths.insert(paths.end(),path.begin(),path.end());
    }



    std::cout << "LOADED ALL VEHICLES\n";

}

void Puppeteer::OnUpdate(const gazebo::common::UpdateInfo &_info){
    double dt = (_info.simTime - this->last_update).Double();

    if (dt < 1/this->update_freq){
        return;
    }

    this->last_update = _info.simTime;


    if ((this->robot_name != "") && this->robot == nullptr){
        for (unsigned int i = 0; i < world->ModelCount(); ++i) {
            auto model = world->ModelByIndex(i);
            if (model->GetName() == this->robot_name){
                this->robot = model;
                for (auto vehicle: this->follower_queue){
                    vehicle->LoadLeader(this->robot);
                }
                this->robot_links = this->robot->GetLinks();
                std::cout << "ADDED ROBOT: " << this->robot->GetName() << std::endl;
            }
        }
       
        double i = 0;
        for (auto path: paths){
            
            for (auto pose: path){
                geometry_msgs::PoseStamped msg;
                msg.pose.position.x = pose.X();
                msg.pose.position.y = pose.Y();
                msg.pose.position.z = pose.Z();
                msg.header.stamp = ros::Time(i);
                path_pub.publish(msg);
            }
            i++;
        }
    }


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

        auto vehicle_pos = vehicle->GetPose();

        auto min = ignition::math::Vector3d(vehicle_pos.Pos().X() - 2, vehicle_pos.Pos().Y() - 2, 0);
        auto max = ignition::math::Vector3d(vehicle_pos.Pos().X() + 2, vehicle_pos.Pos().Y() + 2, 0);
        auto query_range = ignition::math::Box(min,max);

        std::vector<QTData> query_objects = this->static_quadtree->QueryRange(query_range);
        for (auto n: query_objects){
            if (n.type == entity_type){
                near_objects.push_back(boost::static_pointer_cast<gazebo::physics::Entity>(n.data));
            }
            
        }

        if (this->robot != nullptr && (vehicle_pos.Pos() - this->robot->WorldPose().Pos()).Length()<2){
            near_objects.push_back(this->robot);
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
    
    if (this->sdf->HasElement("robot_name")){
        this->robot_name = this->sdf->GetElement("robot_name")->Get<std::string>();
        //std::cout << this->robot_name << std::endl;
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

            res = boost::make_shared<Stander>(actor, 1, 10, max_speed, actor->WorldPose(), ignition::math::Vector3d(0,0,0), this->collision_entities, standing_duration, walking_duration, this->vehicle_params["start_mode"]); // read in as params 
            
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
                
               
                res = boost::make_shared<Follower>(actor, 1, 10, max_speed, actor->WorldPose(), ignition::math::Vector3d(0,0,0), this->collision_entities, leader_name, (bool) this->vehicle_params["blocking"]); // read in as params 
                this->follower_queue.push_back(boost::dynamic_pointer_cast<Follower>(res));
            } else{
                std::cout << "leader name not found\n";
            }
        } else if (actor_info["vehicle_type"] == "path_follower"){

            res = boost::make_shared<PathFollower>(actor, this->vehicle_params["mass"], this->vehicle_params["max_force"], max_speed, actor->WorldPose(), ignition::math::Vector3d(0,0,0), this->collision_entities, this->costmap);
            
        } else {
            std::cout << "INVALID VEHICLE TYPE\n";
        }
    }
    

    return res;
}

void Puppeteer::ReadParams(){
    int argc = 0;
    char **argv = NULL;
    ros::init(argc, argv, "Puppeteer");


    if (!nh.getParam("common_vehicle_params", this->vehicle_params)){
        ROS_ERROR("ERROR READING COMMON VEHICLE PARAMS");
        vehicle_params["mass"] = 1;
        vehicle_params["max_force"] = 10;
        vehicle_params["slowing_distance"] =  2;
        vehicle_params["arrival_distance"] = 0.5;
        vehicle_params["obstacle_margin"] = 0.6;
        vehicle_params["blocking"] = 0;
        vehicle_params["start_mode"] = 2;
    }

    if (!nh.getParam("common_boid_params", this->boid_params)){
        ROS_ERROR("ERROR READING COMMON BOID PARAMS");
        boid_params["alignement"] =  0.1;
        boid_params["cohesion"] =  0.01;
        boid_params["separation"] =  2;
        boid_params["FOV_angle"] =  4;
        boid_params["FOV_radius"] =  3;
    }

    if (!nh.getParam("start_time", this->start_time)){
        std::cout << "ERROR SETTING START TIME\n";
        this->start_time = "";
    }

    if (!nh.getParam("tour_name", this->tour_name)){
        std::cout << "ERROR READING TOUR NAME\n";
        this->tour_name = "A_tour";
        return;
    }


}
