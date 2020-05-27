#include "world_entities.hh"

using namespace myhal;

int Model::num_models = 0;

Model::Model(std::string _name, ignition::math::Pose3d _pose, std::string _model_file){
    this->name = _name + "_" + std::to_string(num_models); //this ensures name uniqueness 
    this->pose = _pose;
    this->model_file = _model_file;
    num_models++;
}


void Model::AddPlugin(std::shared_ptr<SDFPlugin>  plugin){
    this->plugins.push_back(plugin);
}

std::string Model::CreateSDF(){
    return "";
}

void Model::AddToWorld(gazebo::physics::WorldPtr _world){


    for (int i =0; i < _world->ModelCount(); i++){
        //TODO
        //std::cout << this->name << " vs " << _world->ModelByIndex(i)->GetName() << std::endl;
        if (_world->ModelByIndex(i)->GetName() == this->name){
            //std::cout << "DEBUG\n";
            return;
        }
    }

    sdf::SDF modelSDF;
    modelSDF.SetFromString(this->CreateSDF());

    _world->InsertModelSDF(modelSDF);

}

std::string IncludeModel::CreateSDF(){

    std::shared_ptr<HeaderTag> include = std::make_shared<HeaderTag>("include");

    std::shared_ptr<DataTag> name = std::make_shared<DataTag>("name", this->name);

    std::string pose_string = std::to_string(this->pose.Pos().X()) + " " + std::to_string(this->pose.Pos().Y()) + " " + std::to_string(this->pose.Pos().Z()) + " " + std::to_string(this->pose.Rot().Roll()) + " " + std::to_string(this->pose.Rot().Pitch()) + " " + std::to_string(this->pose.Rot().Yaw());
    std::shared_ptr<DataTag> pose_tag = std::make_shared<DataTag>("pose", pose_string);

    std::shared_ptr<DataTag> uri = std::make_shared<DataTag>("uri", this->model_file);

    include->AddSubtag(name);
    include->AddSubtag(pose_tag);
    include->AddSubtag(uri);

    std::stringstream sdf;

    sdf<< "<sdf version ='1.6'>\n";

    sdf << include->WriteTag(1);

    sdf << "</sdf>\n";

    return sdf.str();
}


void Actor::AddAnimation(std::shared_ptr<SDFAnimation> animation){
    this->animations.push_back(animation);
}

std::string Actor::CreateSDF(){

    //make appropriate tags:

    //skin
    std::shared_ptr<DataTag> s_file = std::make_shared<DataTag>("filename", this->model_file);
    std::shared_ptr<HeaderTag> s_header = std::make_shared<HeaderTag>("skin");
    s_header->AddSubtag(s_file);

    //pose
    std::string pose_string = std::to_string(this->pose.Pos().X()) + " " + std::to_string(this->pose.Pos().Y()) + " " + std::to_string(this->pose.Pos().Z()) + " " + std::to_string(this->pose.Rot().Roll()) + " " + std::to_string(this->pose.Rot().Pitch()) + " " + std::to_string(this->pose.Rot().Yaw());
    std::shared_ptr<DataTag> pose_tag = std::make_shared<DataTag>("pose", pose_string);

    std::shared_ptr<HeaderTag> actor = std::make_shared<HeaderTag>("actor");

    actor->AddAttribute("name", this->name);
    actor->AddSubtag(pose_tag);
    actor->AddSubtag(s_header);
   

    for (std::shared_ptr<SDFAnimation> animation : this->animations){
        actor->AddSubtag(animation);
    }

    for (std::shared_ptr<SDFPlugin> plugin : this->plugins){
        actor->AddSubtag(plugin);
    }

    //write to stream

    std::stringstream sdf;

    sdf<< "<sdf version ='1.6'>\n";

    sdf << actor->WriteTag(1);

    sdf << "</sdf>\n";

    return sdf.str();
}

Room::Room(double x_min, double y_min, double x_max, double y_max){
    this->boundary = ignition::math::Box(ignition::math::Vector3d(x_min,y_min,0), ignition::math::Vector3d(x_max,y_max,0));
}

void Room::AddModel(std::shared_ptr<Model> model){
    this->models.push_back(model);
}

void Room::AddToWorld(gazebo::physics::WorldPtr _world){
    for (std::shared_ptr<Model> model : this->models){
        model->AddToWorld(_world);
    }
}