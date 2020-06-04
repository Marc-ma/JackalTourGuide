#include "lidar_listener.hh"


GZ_REGISTER_WORLD_PLUGIN(LidarListener);

//void Callback(const PointCloud::ConstPtr& msg);

void LidarListener::Load(gazebo::physics::WorldPtr _world, sdf::ElementPtr _sdf){
    this->world = _world;
    this->sdf = _sdf;
    this->update_connection = gazebo::event::Events::ConnectWorldUpdateBegin(std::bind(&LidarListener::OnUpdate, this, std::placeholders::_1));

    this->ReadSDF();

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
            this->actors.push_back(act);
            auto actor_pos = act->WorldPose().Pos();
            auto min = ignition::math::Vector3d(actor_pos.X() - 0.25, actor_pos.Y() - 0.25, 0);
            auto max = ignition::math::Vector3d(actor_pos.X() + 0.25, actor_pos.Y() + 0.25, 0);
            auto box = ignition::math::Box(min,max);
            auto new_node = QTData(box, act, entity_type);
            this->vehicle_quadtree->Insert(new_node);
            continue;
        } 
      
        if (model->GetName() == this->building_name){
            auto links = model->GetLinks();
            for (gazebo::physics::LinkPtr link: links){
                std::vector<gazebo::physics::CollisionPtr> collision_boxes = link->GetCollisions();
                for (gazebo::physics::CollisionPtr collision_box: collision_boxes){
                    this->building_collisions.push_back(collision_box); //TODO: check if this is correct (maybe do dynamic pointer cast )
                    auto new_node = QTData(collision_box->BoundingBox(), collision_box, entity_type);
                    this->static_quadtree->Insert(new_node);
                }
                    
            }
        } else if (model->GetName() != "ground_plane"){
            this->model_collisions.push_back(model);
            auto box = model->BoundingBox();
            box.Min().Z() = 0;
            box.Max().Z() = 0;
            auto new_node = QTData(box, model, entity_type);
            this->static_quadtree->Insert(new_node);
        } 
        
    }

    int argc = 0;
    char **argv = NULL;
    ros::init(argc, argv, "LidarListener");
    
    this->sub = this->nh.subscribe<PointCloud>("velodyne_points", 1, &LidarListener::Callback, this);
    ros::AsyncSpinner spinner(boost::thread::hardware_concurrency());
    ros::Rate r= 10;
    spinner.start();
}

void LidarListener::OnUpdate(const gazebo::common::UpdateInfo &_info){
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
                
                std::cout << "ADDED ROBOT: " << this->robot->GetName() << std::endl;
                this->robot_links = this->robot->GetLinks();
                for (auto link: this->robot_links){
                    std::cout << link->GetName() << std::endl;
                }
            }
        }
    } 

    if (this->robot != nullptr){
        this->sensor_pose = this->robot_links[0]->WorldPose();
        this->sensor_pose.Pos().Z() += 0.539;
        //std::printf("%f %f %f\n", sensor_pose.Pos().X(), sensor_pose.Pos().Y(), sensor_pose.Pos().Z());
    }


   
}

void LidarListener::Callback(const PointCloud::ConstPtr& msg){

    if (this->robot == nullptr){
        return;
    }

    //std::printf("Cloud: width = %d, height = %d\n", msg->width, msg->height);

    // reconstruct vehicle quadtree:
    this->vehicle_quadtree = boost::make_shared<QuadTree>(this->building_box);
    for (auto act: this->actors){
        auto actor_pos = act->WorldPose().Pos();
        auto min = ignition::math::Vector3d(actor_pos.X() - 0.25, actor_pos.Y() - 0.25, 0);
        auto max = ignition::math::Vector3d(actor_pos.X() + 0.25, actor_pos.Y() + 0.25, 0);
        auto box = ignition::math::Box(min,max);
        auto new_node = QTData(box, act, entity_type);
        this->vehicle_quadtree->Insert(new_node);
    }

    for (auto pt : msg->points){

        auto point = ignition::math::Vector3d(pt.x, pt.y, pt.z);
        point+=this->sensor_pose.Pos();

        std::vector<gazebo::physics::EntityPtr> near_vehicles;
        std::vector<gazebo::physics::EntityPtr> near_objects;

        auto min = ignition::math::Vector3d(point.X() - 0.1, point.Y() - 0.1, 0);
        auto max = ignition::math::Vector3d(point.X() + 0.1, point.Y() +0.1, 0);
        auto query_range = ignition::math::Box(min,max);

        std::vector<QTData> query_objects = this->static_quadtree->QueryRange(query_range);
        for (auto n: query_objects){
            if (n.type == entity_type){
                near_objects.push_back(boost::static_pointer_cast<gazebo::physics::Entity>(n.data));
                
            }
        }


        std::vector<QTData> query_vehicles = this->vehicle_quadtree->QueryRange(query_range);
        for (auto n: query_vehicles){
            if (n.type == entity_type){
                near_vehicles.push_back(boost::static_pointer_cast<gazebo::physics::Entity>(n.data));
            }
        }

        //std::cout << near_objects.size() << std::endl;
        if (near_objects.size() == 0 && near_vehicles.size() == 0){
            //std::cout << "ground" << std::endl;
        } else{
            /*
            for (auto n: near_objects){
                std::cout << n->GetName() << " ";
            }
            */
            for (auto n: near_vehicles){
                std::cout << n->GetName() << " ";
            }
            std::cout <<std::endl;
        }
        //std::printf ("\t(%f, %f, %f)\n", point.X(), point.Y(), point.Z());
    }

    
    
}


void LidarListener::ReadSDF(){
    if (this->sdf->HasElement("building_name")){
        this->building_name =this->sdf->GetElement("building_name")->Get<std::string>();
    }
    if (this->sdf->HasElement("robot_name")){
        this->robot_name = this->sdf->GetElement("robot_name")->Get<std::string>();
    }
}
