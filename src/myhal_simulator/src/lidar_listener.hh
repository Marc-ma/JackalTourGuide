#ifndef LIDAR_LISTENER_HH
#define LIDAR_LISTENER_HH

#include "utilities.hh"
#include <ignition/math/Pose3.hh>
#include <ignition/math/Vector3.hh>
#include "gazebo/physics/physics.hh"
#include "gazebo/common/common.hh"
#include "gazebo/gazebo.hh"
#include <utility>
#include <pcl_ros/point_cloud.h>
#include <pcl/point_types.h>
#include <ros/ros.h>
#include <boost/thread.hpp>
#include "quadtree.hh"


typedef pcl::PointCloud<pcl::PointXYZ> PointCloud;

class LidarListener: public gazebo::WorldPlugin{

    private:

        gazebo::event::ConnectionPtr update_connection;

        gazebo::physics::WorldPtr world;

        sdf::ElementPtr sdf;

        double update_freq = 30;

        gazebo::common::Time last_update;

        ros::NodeHandle nh;

        ros::Subscriber sub;

        ros::Publisher ground_pub;

        ros::Publisher wall_pub;

        ros::Publisher moving_actor_pub;

        ros::Publisher still_actor_pub;

        ros::Publisher table_pub;

        ros::Publisher chair_pub;

        std::string robot_name = "";

        gazebo::physics::ModelPtr robot = nullptr;

        std::vector<gazebo::physics::LinkPtr> robot_links;

        ignition::math::Pose3d sensor_pose;

        std::string building_name;

        std::vector<gazebo::physics::ActorPtr> actors;

        ignition::math::Box building_box;

        boost::shared_ptr<QuadTree> static_quadtree; 

        boost::shared_ptr<QuadTree> vehicle_quadtree; 

        std::map<std::string, ignition::math::Vector3d> last_actor_pos;

        std::map<std::string, double> actor_speed;

        void Callback(const PointCloud::ConstPtr& msg);


    public:

        void Load(gazebo::physics::WorldPtr _world, sdf::ElementPtr _sdf);

        void OnUpdate(const gazebo::common::UpdateInfo &_info);

        void ReadSDF();

        

};


#endif