#pragma once 

#include <string>
#include <vector>
#include <ros/ros.h>
#include "frame.hh"
#include <rosbag/bag.h>
#include <rosbag/view.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/PoseWithCovariance.h>
#include <tf2_msgs/TFMessage.h>
#include <geometry_msgs/TransformStamped.h>
#include <nav_msgs/Odometry.h>
#include <geometry_msgs/PoseWithCovarianceStamped.h>
#include <move_base_msgs/MoveBaseActionResult.h>
#include <stdio.h>
#include "happily.h"
#include <map>
#include <utility>  
#include "utilities.hh"
#include <fstream>
#include <iostream>
#include "process_bag.hh"


struct TimeBool{

    double time;
    bool success;

    TimeBool(double time, bool success): time(time), success(success){}
};

class BagTools{

    private:

        std::string filepath;

        std::string bag_name;

        rosbag::Bag bag;


    public:

        BagTools(std::string filepath, std::string bag_name = "raw_data.bag"){
            this->filepath = filepath;
            this->bag_name = bag_name;
        }


        std::vector<std::vector<double>> LocalizationError(std::vector<TrajPoint> gt, std::vector<TrajPoint> other){

            std::vector<std::vector<double>> error;

            double min_time = 10e9;
            double max_time = -10e9;
            for (auto frame: gt){
                min_time = std::min(min_time, frame.time);
                max_time = std::max(max_time, frame.time);
            }

            int last = 0;
            ignition::math::Pose3d last_gt = gt[0].pose;
            double dist = 0;

            //for every frame in amcl pose
            for (auto frame: other){
                double time = frame.time;

                
                if (time < min_time || time > max_time){
                    continue;
                }

                int lower_ind = last;

                // find closest two gt poses 
                while (gt[lower_ind].time > time || gt[lower_ind+1].time < time){
                        
                    if (gt[lower_ind].time > time){
                        lower_ind -=1;
                    } else {
                        lower_ind +=1;
                    }
                }

                last = lower_ind+1;

                auto pose1 = gt[lower_ind].pose;
                    
                auto pose2 = gt[lower_ind+1].pose;

                double t1 = gt[lower_ind].time;
                double t2 = gt[lower_ind+1].time;
                
                // interpolate gt pose to the time of amcl pose
                auto interpolated_pose = utilities::InterpolatePose(time, t1, t2, pose1, pose2);
                interpolated_pose.Pos().Z() = 0;
                last_gt.Pos().Z() = 0;
                dist += (interpolated_pose.Pos()-last_gt.Pos()).Length();
                last_gt = interpolated_pose;

                // compute drift
                error.push_back({dist, (interpolated_pose.Pos() - frame.pose.Pos()).Length(), std::abs(interpolated_pose.Rot().Yaw() - frame.pose.Rot().Yaw())});
            }

            return error;
        }

        std::vector<TrajPoint> ShiftTrajectory(std::vector<TrajPoint> traj, std::vector<TrajPoint> transform){
            std::vector<TrajPoint> transformed_traj;

            double min_time = 10e9;
            double max_time = -10e9;
            for (auto frame: traj){
                min_time = std::min(min_time, frame.time);
                max_time = std::max(max_time, frame.time);
            }

            int last = 0;
            ignition::math::Pose3d last_pose = traj[0].pose;
            double dist = 0;

            //for every transformation
            for (auto frame: transform){
                double time = frame.time;

                if (time < min_time || time > max_time){
                    continue;
                }

                int lower_ind = last;

                // find closest two poses 
                while (traj[lower_ind].time > time || traj[lower_ind+1].time < time){
                        
                    if (traj[lower_ind].time > time){
                        lower_ind -=1;
                    } else {
                        lower_ind +=1;
                    }
                }

                last = lower_ind+1;

                auto pose1 = traj[lower_ind].pose;
                    
                auto pose2 = traj[lower_ind+1].pose;

                double t1 = traj[lower_ind].time;
                double t2 = traj[lower_ind+1].time;
                
                // interpolate gt pose to the time of transform
                auto interpolated_pose = utilities::InterpolatePose(time, t1, t2, pose1, pose2);

                // transform interpolated pose by transform 

                interpolated_pose = interpolated_pose + frame.pose;

                auto traj_point = TrajPoint(interpolated_pose, time);

                transformed_traj.push_back(traj_point);
               
            }

            return transformed_traj;
        }


        // may take either nav_msgs::Odometry or geometry_msgs::PoseWithCovarianceStamped
        std::vector<TrajPoint> GetTrajectory(std::string topic){
            std::vector<TrajPoint> trajectory;
            rosbag::Bag bag;
            bag.open(this->filepath + this->bag_name, rosbag::bagmode::Read);
            rosbag::View view(bag, rosbag::TopicQuery({topic}));

            for (auto msg: view){
                
                if (msg.getTopic() == topic){
                
                    auto pose = msg.instantiate<nav_msgs::Odometry>();
                    if (pose != nullptr) {
                        trajectory.push_back(TrajPoint(ignition::math::Pose3d(pose->pose.pose.position.x, pose->pose.pose.position.y, pose->pose.pose.position.z, pose->pose.pose.orientation.w, pose->pose.pose.orientation.x, pose->pose.pose.orientation.y, pose->pose.pose.orientation.z), pose->header.stamp.toSec()));
                       
                    } else{ 
                        auto p = msg.instantiate<geometry_msgs::PoseWithCovarianceStamped>();
                        trajectory.push_back(TrajPoint(ignition::math::Pose3d(p->pose.pose.position.x, p->pose.pose.position.y, p->pose.pose.position.z, p->pose.pose.orientation.w, p->pose.pose.orientation.x, p->pose.pose.orientation.y, p->pose.pose.orientation.z), p->header.stamp.toSec()));
                        
                    }
                    
                } 
            }


            return trajectory;
            bag.close();
        }

        // we will use TrajPoint to store the time and transfrom 

        std::vector<TrajPoint> GetTransforms(std::string parent, std::string child, std::string topic = "/tf"){
            std::vector<TrajPoint> transform_list;
            rosbag::Bag bag;
            bag.open(this->filepath + this->bag_name, rosbag::bagmode::Read);
            rosbag::View view(bag, rosbag::TopicQuery({topic}));

            for (auto msg: view){
                if (msg.getTopic() == topic){

                    auto transforms = msg.instantiate<tf2_msgs::TFMessage>();
                    for (auto transform: transforms->transforms){
                        if ((transform.header.frame_id == parent) && (transform.child_frame_id == child)){
                            auto tf_pose = ignition::math::Pose3d(ignition::math::Vector3d(transform.transform.translation.x, transform.transform.translation.y, transform.transform.translation.z), ignition::math::Quaterniond(transform.transform.rotation.w,transform.transform.rotation.x, transform.transform.rotation.y, transform.transform.rotation.z));
                            TrajPoint tf = TrajPoint(tf_pose, transform.header.stamp.toSec());
                            transform_list.push_back(tf);
                            //std::cout << transform.header.frame_id << " -> " << transform.child_frame_id << std::endl;
                        }
                    }
                }

            }

            return transform_list;
        }

        std::vector<ignition::math::Pose3d> TourTargets(){
            std::vector<ignition::math::Pose3d> goals = {ignition::math::Pose3d(0,0,0,0,0,0)};
            rosbag::Bag bag;
            bag.open(this->filepath + this->bag_name, rosbag::bagmode::Read);
            
            std::vector<std::string> topics = {"/tour_data"};
            rosbag::View view(bag, rosbag::TopicQuery(topics));

            for (auto msg: view){
                
                auto pose = msg.instantiate<geometry_msgs::PoseStamped>();
                if (pose!=nullptr){
                    auto goal = ignition::math::Pose3d(pose->pose.position.x, pose->pose.position.y, pose->pose.position.z, pose->pose.orientation.w, pose->pose.orientation.x, pose->pose.orientation.y, pose->pose.orientation.z);
                    goals.push_back(goal);
                }
            }

            return goals;

        }

        std::vector<TimeBool> TargetTimes(){
            std::vector<TimeBool> times;

            
            rosbag::Bag bag;
            bag.open(this->filepath + this->bag_name, rosbag::bagmode::Read);
            
            std::vector<std::string> topics = {"/move_base/result"};
            rosbag::View view(bag, rosbag::TopicQuery(topics));

            for (auto msg: view){
                auto res = msg.instantiate<move_base_msgs::MoveBaseActionResult>();
                if (res != nullptr && res->status.status == 3){
                    times.push_back(TimeBool(res->header.stamp.toSec(), true));
                } else if (res != nullptr && res->status.status == 4){
                    times.push_back(TimeBool(res->header.stamp.toSec(), false));
                }
            }

            return times;
        }

        double PathLength(std::vector<ignition::math::Vector3d> path){
            double length = 0;

            for (int i =0; i< path.size() -1; ++i){
                length += (path[i+1]-path[i]).Length();
            }

            return length;
        }

        void NewLidarTopic(std::string path){

            // read velodyne_points frames 

            BagProcessor processor = BagProcessor(this->filepath, "/ground_truth/state", "/velodyne_points", false);
    
            auto bag_data = processor.GetData();
            rosbag::Bag bag;
            bag.open(this->filepath + this->bag_name, rosbag::bagmode::Append);

            // read in hugues frames 
            std::cout << "Reading Frames\n";
            for (auto v_frame: bag_data.frames){
                std::string frame_name = std::to_string(v_frame.Time()) + ".ply";
                //std::cout << frame_name  << std::endl;

                Frame h_frame = ReadFrame(path + frame_name);

                // reclassify each point in v_frame as a point in h_frame 

                for (int i =0; i< v_frame.points.size(); ++i){
                    int h_cat = h_frame.points[i].Cat();
                    int new_cat;

                    if (h_cat == 0){
                        new_cat = 5;
                    } else if (h_cat == 1){
                        new_cat = 0;
                    } else if (h_cat == 2){
                        new_cat = 5;
                    } else if (h_cat == 3){
                        new_cat = 2;
                    } else if (h_cat == 4){
                        new_cat = 4;
                    }

                    v_frame.points[i].SetCat(new_cat);
                }

                // write frame as time stamped message to bag file 

                pcl::PointCloud<pcl::PointXYZI>::Ptr cloud_ptr(new pcl::PointCloud<pcl::PointXYZI>);
                cloud_ptr->header.frame_id = "velodyne";
                cloud_ptr->height = 1;
                cloud_ptr->width = 0;

                ros::Time frame_time(v_frame.Time());
            
                pcl_conversions::toPCL(frame_time, cloud_ptr->header.stamp);
                

                for (auto pt: v_frame.points){
                    pcl::PointXYZI point;

                    point.x = pt.X();
                    point.y = pt.Y();
                    point.z = pt.Z();
                    point.intensity = pt.Cat();
                    
                    cloud_ptr->points.push_back(point);
                    
                    cloud_ptr->width++;
                }

                bag.write("/hugues_points",frame_time, cloud_ptr);
                
            }
            std::cout << "Done\n";
            bag.close();
        }


        int MessageCount(std::string topic){
            rosbag::Bag bag;
            bag.open(this->filepath + this->bag_name, rosbag::bagmode::Read);
            
            std::vector<std::string> topics = {topic};
            rosbag::View view(bag, rosbag::TopicQuery(topics));

            int count = 0;

            for (auto msg: view){
                if (msg.getTopic() == topic){
                    count++;
                }
            }

            return count;
        }
};