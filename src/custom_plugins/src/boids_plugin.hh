

#ifndef SERVICESIM_PLUGINS_WANDERINGACTORPLUGIN_HH_
#define SERVICESIM_PLUGINS_WANDERINGACTORPLUGIN_HH_

#include <gazebo/common/Plugin.hh>
#include "gazebo/util/system.hh"


namespace servicesim
{
  class ActorPluginPrivate;

  class GAZEBO_VISIBLE ActorPlugin : public gazebo::ModelPlugin
  {
    /// \brief Constructor
    public: ActorPlugin();

    /// \brief Load the actor plugin.
    /// \param[in] _model Pointer to the parent model.
    /// \param[in] _sdf Pointer to the plugin's SDF elements.
    public: virtual void Load(gazebo::physics::ModelPtr _model, sdf::ElementPtr _sdf)
        override;

    /// \brief Function that is called every update cycle.
    /// \param[in] _info Timing information
    private: void OnUpdate(const gazebo::common::UpdateInfo &_info);
    
    private: void NetForceUpdate();
  
    private: ignition::math::Vector3d BoidAvoidance();

	  private: ignition::math::Vector3d ObstacleAvoidance();
    
    private: ignition::math::Vector3d Alignment();
    
    private: ignition::math::Vector3d Cohesion();

    /// \internal
    public: ActorPluginPrivate *dataPtr;

  };
}
#endif