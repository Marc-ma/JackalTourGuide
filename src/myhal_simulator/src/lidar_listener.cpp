#include "lidar_listener.hh"
#include <string>


GZ_REGISTER_WORLD_PLUGIN(LidarListener);


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
            this->last_actor_pos[act->GetName()] = actor_pos;

            ignition::math::Vector3d min, max;
            
            min = ignition::math::Vector3d(actor_pos.X() - 0.3, actor_pos.Y() - 0.3, 0);
            max = ignition::math::Vector3d(actor_pos.X() + 0.3, actor_pos.Y() + 0.3, 0);
            
            auto box = ignition::math::Box(min,max);
            auto new_node = QTData(box, act, entity_type);
            this->vehicle_quadtree->Insert(new_node);
            continue;
        } 


      
        auto links = model->GetLinks();
        for (gazebo::physics::LinkPtr link: links){
            std::vector<gazebo::physics::CollisionPtr> collision_boxes = link->GetCollisions();
            for (gazebo::physics::CollisionPtr collision_box: collision_boxes){
                auto box = collision_box->BoundingBox();
                if (model->GetName() == "ground_plane"){
                    box.Min().X() = building_box.Min().X()+0.5;
                    box.Min().Y() = building_box.Min().Y()+0.5;
                    box.Max().X() = building_box.Max().X()-0.5;
                    box.Max().Y() = building_box.Max().Y()-0.5;
                }
                box.Max().Z() = 0;
                box.Min().Z() = 0;
                auto new_node = QTData(box, collision_box, entity_type);
                this->static_quadtree->Insert(new_node);
            }
                
        }
    
        
    }

    int argc = 0;
    char **argv = NULL;
    ros::init(argc, argv, "LidarListener");
    
    this->sub = this->nh.subscribe<PointCloud>("velodyne_points", 1, &LidarListener::Callback, this);
    this->ground_pub = nh.advertise<PointCloud>("ground_points", 1);
    this->wall_pub = nh.advertise<PointCloud>("wall_points", 1);
    this->moving_actor_pub = nh.advertise<PointCloud>("moving_actor_points", 1);
    this->still_actor_pub = nh.advertise<PointCloud>("still_actor_points", 1);
    this->table_pub = nh.advertise<PointCloud>("table_points", 1);
    this->chair_pub = nh.advertise<PointCloud>("chair_points", 1);
    ros::AsyncSpinner spinner(boost::thread::hardware_concurrency());
    ros::Rate r = (ros::Rate) this->update_freq;
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
              
            }
        }
    } 

    if (this->robot != nullptr){
    
        this->sensor_pose = this->robot_links[0]->WorldPose();
        this->sensor_pose.Pos().Z() += 0.5767;
    }


   
}

void LidarListener::Callback(const PointCloud::ConstPtr& msg){

    if (this->robot == nullptr){
        return;
    }

    PointCloud::Ptr ground_msg (new PointCloud);
    ground_msg->header.frame_id = "velodyne";
    ground_msg->height = msg->height;
    ground_msg->width = 0;

    PointCloud::Ptr wall_msg (new PointCloud);
    wall_msg->header.frame_id = "velodyne";
    wall_msg->height = msg->height;
    wall_msg->width = 0;
    
    PointCloud::Ptr moving_actor_msg (new PointCloud);
    moving_actor_msg->header.frame_id = "velodyne";
    moving_actor_msg->height = msg->height;
    moving_actor_msg->width = 0;

    PointCloud::Ptr still_actor_msg (new PointCloud);
    still_actor_msg->header.frame_id = "velodyne";
    still_actor_msg->height = msg->height;
    still_actor_msg->width = 0;

    PointCloud::Ptr table_msg (new PointCloud);
    table_msg->header.frame_id = "velodyne";
    table_msg->height = msg->height;
    table_msg->width = 0;

    PointCloud::Ptr chair_msg (new PointCloud);
    chair_msg->header.frame_id = "velodyne";
    chair_msg->height = msg->height;
    chair_msg->width = 0;
    
    // reconstruct vehicle quadtree:
    this->vehicle_quadtree = boost::make_shared<QuadTree>(this->building_box);
    for (auto act: this->actors){

        auto actor_pos = act->WorldPose().Pos();

        double dt = 1/this->update_freq;
        double speed = ((actor_pos-this->last_actor_pos[act->GetName()]).Length())/dt;
        this->actor_speed[act->GetName()] = speed;

        this->last_actor_pos[act->GetName()] = actor_pos;
        auto min = ignition::math::Vector3d(actor_pos.X() - 0.3, actor_pos.Y() - 0.3, 0);
        auto max = ignition::math::Vector3d(actor_pos.X() + 0.3, actor_pos.Y() + 0.3, 0);
        auto box = ignition::math::Box(min,max);
        auto new_node = QTData(box, act, entity_type);
        this->vehicle_quadtree->Insert(new_node);
    }

    for (auto pt : msg->points){

        auto point = this->sensor_pose.CoordPositionAdd(ignition::math::Vector3d(pt.x, pt.y, pt.z));       
   
        std::vector<gazebo::physics::EntityPtr> near_vehicles;
        std::vector<gazebo::physics::EntityPtr> near_objects;

        double resolution = 0.01;
        auto min = ignition::math::Vector3d(point.X() - resolution, point.Y() - resolution, 0);
        auto max = ignition::math::Vector3d(point.X() + resolution, point.Y() + resolution, 0);
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

   
        if (near_objects.size() == 0 && near_vehicles.size() == 0){
            
            ground_msg->points.push_back(pt);
            ground_msg->width++;
        
        } else {

            std::string closest_name; 
            double min_dist = 10e9;
            
            for (auto n: near_objects){
                
                auto dist = utilities::dist_to_box(point, n->BoundingBox());

                if (dist <= min_dist){
                    min_dist = dist;
                    closest_name = n->GetParent()->GetParent()->GetName();
                }
            }

            if (closest_name == this->building_name){
                wall_msg->points.push_back(pt);
                wall_msg->width++;
            } else if (closest_name.substr(0,5) == "table"){
                table_msg->points.push_back(pt);
                table_msg->width++;
            } else if (closest_name.substr(0,5) == "chair" && near_vehicles.size() == 0){
                chair_msg->points.push_back(pt);
                chair_msg->width++;
            } else if (near_vehicles.size() == 0) {
                ground_msg->points.push_back(pt);
                ground_msg->width++;
            }

            
            
            for (auto n: near_vehicles){
                
                if (point.Z() <=0){
                    ground_msg->points.push_back(pt);
                    ground_msg->width++;
                } else{
                    if (this->actor_speed[n->GetName()] < 10e-3){
                        still_actor_msg->points.push_back(pt);
                        still_actor_msg->width++;
                    } else{
                        moving_actor_msg->points.push_back(pt);
                        moving_actor_msg->width++;
                    }
                    
                }
            }
           
        }
        
    }

    pcl_conversions::toPCL(ros::Time::now(), ground_msg->header.stamp);
    pcl_conversions::toPCL(ros::Time::now(), wall_msg->header.stamp);
    pcl_conversions::toPCL(ros::Time::now(), moving_actor_msg->header.stamp);
    pcl_conversions::toPCL(ros::Time::now(), still_actor_msg->header.stamp);
    pcl_conversions::toPCL(ros::Time::now(), table_msg->header.stamp);
    pcl_conversions::toPCL(ros::Time::now(), chair_msg->header.stamp);
   

    this->ground_pub.publish(ground_msg);
    this->wall_pub.publish(wall_msg);
    this->moving_actor_pub.publish(moving_actor_msg);
    this->still_actor_pub.publish(still_actor_msg);
    this->table_pub.publish(table_msg);
    this->chair_pub.publish(chair_msg);
}


void LidarListener::ReadSDF(){
    if (this->sdf->HasElement("building_name")){
        this->building_name =this->sdf->GetElement("building_name")->Get<std::string>();
    }
    if (this->sdf->HasElement("robot_name")){
        this->robot_name = this->sdf->GetElement("robot_name")->Get<std::string>();
    }
}