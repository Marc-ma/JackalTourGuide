#include "bag_tools.hh"
#include "costmap.hh"

int main(int argc, char ** argv){

    if (argc ==1){
        std::cout << "must input folder name and true/false\n";
        return 0;
    }

    std::string user_name = "default";
    if (const char * user = std::getenv("USER")){
        user_name = user;
    } 

    std::string time_name = argv[1];

    std::string filepath = "/home/" + user_name + "/Myhal_Simulation/simulated_runs/" + time_name + "/";

    BagTools handle = BagTools(filepath);

    auto amcl_traj = handle.GetTrajectory("/amcl_pose");
    auto gt_traj = handle.GetTrajectory("/ground_truth/state");

    happly::PLYData plyOut;
    AddTrajectory(plyOut, amcl_traj);
    plyOut.write(filepath + "amcl_pose.ply", happly::DataFormat::Binary);

    auto trans_drift = handle.TranslationDrift(gt_traj, amcl_traj);

    std::ofstream out(filepath + "translation_drift.csv");
    out << "Distance (m), Translation Drift (m)\n";
    for (auto row: trans_drift){
        out << row[0] << "," << row[1] << std::endl;
    }

    out.close();

    // read the information from the static objects file to creat the costmap 

    happly::PLYData plyIn(filepath + "static_objects.ply");
    auto static_objects = ReadObjects(plyIn);

    double min_x = 10e9;
    double min_y = 10e9;
    double max_x = -10e9;
    double max_y = -10e9;

    for (auto obj: static_objects){
        min_x = std::min(obj.MinX(), min_x);
        min_y = std::min(obj.MinY(), min_y);
        max_x = std::max(obj.MaxX(), max_x);
        max_y = std::max(obj.MaxY(), max_y);
    }

    auto boundary = ignition::math::Box(min_x-1, min_y-1, 0, max_x+1, max_y+1, 0);

    double robot_radius = std::sqrt((0.21*0.21) + (0.165*0.165));
    
    Costmap costmap = Costmap(boundary, 0.1);

    for (auto obj: static_objects){
        if (obj.MinZ() < 1.5){
            auto box = obj.Box();

            // TODO: inflate box
            box.Min().X()-=robot_radius;
            box.Min().Y()-=robot_radius;
            box.Max().X()+=robot_radius;
            box.Max().Y()+=robot_radius;

            costmap.AddObject(box);
        }
    }

    //std::ofstream test(filepath+"test.txt");

    //test << costmap.ToString();

    //test.close();

    // read navigation goals from tour:
  

    auto goals = handle.TourTargets();

    auto times = handle.TargetSuccessTimes();
    auto num_reached = times.size();

    if (num_reached == 0){
        return 0;
    }
    
    std::vector<std::vector<ignition::math::Vector3d>> paths;
    
    for (int first = 0; first < num_reached; first++){
      
        auto start = goals[first];
       
        auto end = goals[first+1];
        std::vector<ignition::math::Vector3d> path;
        
  
        costmap.FindPath(start.Pos(), end.Pos(), path);
        paths.push_back(path);
    }

    // compute length of each path 
 

    std::vector<double> optimal_lengths;
    
    for (auto path: paths){
        optimal_lengths.push_back(0);
        int count = 0;
        ignition::math::Vector3d last_pose;
        for (auto pose: path){
            if (count == 0){
                last_pose = pose;
                count++;
                continue;
            }
            count ++;
            
            optimal_lengths.back() += (pose - last_pose).Length();
            
            last_pose = pose;
        }
        std::cout << optimal_lengths.back() << std::endl;
    }

    // find how far the robot travelled in the times from 0->times[0], times[0]->times[1] ...

    std::vector<double> actual_lengths;

    int traj_ind = 0;

    for (int i =0; i<times.size(); i++){
        double time = times[i];
        actual_lengths.push_back(0);
        int count = 0;
        ignition::math::Vector3d last_pose;
        while (traj_ind < gt_traj.size() && gt_traj[traj_ind].time <= time){
            if (count == 0){
                last_pose = gt_traj[traj_ind].pose.Pos();
                count++;
                continue;
            }
            count++;
            actual_lengths.back() += (gt_traj[traj_ind].pose.Pos() - last_pose).Length();
            last_pose = gt_traj[traj_ind].pose.Pos();
            traj_ind++;
        }
        std::cout << actual_lengths.back() << std::endl;
    }
    

    return 0;
}