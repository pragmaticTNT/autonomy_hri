#include "cartesian_grid_interface.h"


CartesianGridInterface::CartesianGridInterface()
{
    ROS_INFO("Constructing an instace of LikelihoodGridInterface.");
}

CartesianGridInterface::CartesianGridInterface(ros::NodeHandle _n, tf::TransformListener *_tf_listener):
    n(_n),
    tf_listener(_tf_listener)
{
    ROS_INFO("Constructing an instace of LikelihoodGridInterface.");
    init();
}

void CartesianGridInterface::init()
{
    ros::param::param("~/LikelihoodGrid/grid_angle_min",FOV.angle.min, -M_PI);
    ros::param::param("~/LikelihoodGrid/grid_angle_max",FOV.angle.max, M_PI);

    ros::param::param("~/LikelihoodGrid/grid_range_min",FOV.range.min, 0.0);
    ros::param::param("~/LikelihoodGrid/grid_range_max",FOV.range.max, 20.0);

    ros::param::param("~/CartesianLikelihoodGrid/resolution",MAP_RESOLUTION, 0.5);
    MAP_SIZE = FOV.range.max * 2 / MAP_RESOLUTION;

    ros::param::param("~/LikelihoodGrid/update_rate",UPDATE_RATE, 0.5);
    ros::param::param("~/LikelihoodGrid/human_cell_probability",CELL_PROBABILITY.human, 0.95);
    ros::param::param("~/LikelihoodGrid/free_cell_probability",CELL_PROBABILITY.free, 0.05);
    ros::param::param("~/LikelihoodGrid/unknown_cell_probability",CELL_PROBABILITY.unknown, 0.25);

    ros::param::param("~/LikelihoodGrid/target_detection_probability",TARGET_DETETION_PROBABILITY, 0.9);
    ros::param::param("~/LikelihoodGrid/false_positive_probability",FALSE_POSITIVE_PROBABILITY, 0.01);

    ros::param::param("~/loop_rate",LOOP_RATE, 10);
    ros::param::param("~/motion_model_enable",MOTION_MODEL_ENABLE, true);
    ros::param::param("~/leg_detection_enable",LEG_DETECTION_ENABLE, true);
    ros::param::param("~/torso_detection_enable",TORSO_DETECTION_ENABLE, true);
    ros::param::param("~/sound_detection_enable",SOUND_DETECTION_ENABLE, true);
    ros::param::param("~/periodic_gesture_detection_enable",PERIODIC_GESTURE_DETECTION_ENABLE, false);

    number_of_sensors = (LEG_DETECTION_ENABLE) + (TORSO_DETECTION_ENABLE)
            + (SOUND_DETECTION_ENABLE) + (PERIODIC_GESTURE_DETECTION_ENABLE);
    ROS_INFO("number_of_sensors is set to %u",number_of_sensors);

    encoder_last_time = ros::Time::now();

    if(PERIODIC_GESTURE_DETECTION_ENABLE){
        FOV.range.max = 30.0;
        SensorFOV_t PERIODIC_DETECTOR_FOV = FOV;
        PERIODIC_DETECTOR_FOV.range.min = 10.00;
        PERIODIC_DETECTOR_FOV.angle.min = toRadian(-65.0/2);
        PERIODIC_DETECTOR_FOV.angle.max = toRadian(65.0/2);

        initPeriodicGrid(PERIODIC_DETECTOR_FOV);

        ros::param::param("~/LikelihoodGrid/periodic_range_stdev",periodic_grid->stdev.range, 1.0);
        ros::param::param("~/LikelihoodGrid/periodic_angle_stdev",periodic_grid->stdev.angle, 1.0);
        periodic_grid_pub = n.advertise<nav_msgs::OccupancyGrid>("periodic_occupancy_grid",10);
    }

    if(LEG_DETECTION_ENABLE){
        SensorFOV_t LEG_DETECTOR_FOV = FOV;

        ros::param::param("~/LikelihoodGrid/leg_range_min",LEG_DETECTOR_FOV.range.min, 1.0);
        ros::param::param("~/LikelihoodGrid/leg_range_max",LEG_DETECTOR_FOV.range.max, 10.0);

        ros::param::param("~/LikelihoodGrid/leg_angle_min",LEG_DETECTOR_FOV.angle.min, toRadian(-120.0));
        ros::param::param("~/LikelihoodGrid/leg_angle_max",LEG_DETECTOR_FOV.angle.max, toRadian(120.0));

        ros::param::param("~/LikelihoodGrid/leg_range_stdev",leg_grid->stdev.range, 0.1);
        ros::param::param("~/LikelihoodGrid/leg_angle_stdev",leg_grid->stdev.angle, 1.0);

        ros::param::param("~/LikelihoodGrid/leg_human_cell_probability",LEG_CELL_PROBABILITY.human, 0.95);
        ros::param::param("~/LikelihoodGrid/leg_free_cell_probability",LEG_CELL_PROBABILITY.free, 0.05);
        ros::param::param("~/LikelihoodGrid/leg_unknown_cell_probability",LEG_CELL_PROBABILITY.unknown, 0.25);

        initLegGrid(LEG_DETECTOR_FOV);

        legs_grid_pub = n.advertise<nav_msgs::OccupancyGrid>("leg_occupancy_grid",10);
        predicted_leg_base_pub = n.advertise<geometry_msgs::PoseArray>("predicted_legs",10);
        last_leg_base_pub = n.advertise<geometry_msgs::PoseArray>("last_legs",10);
        current_leg_base_pub = n.advertise<geometry_msgs::PoseArray>("legs_basefootprint",10);
    }

    if(TORSO_DETECTION_ENABLE){
        SensorFOV_t TORSO_DETECTOR_FOV = FOV;

        ros::param::param("~/LikelihoodGrid/torso_range_min",TORSO_DETECTOR_FOV.range.min, 2.0);
        ros::param::param("~/LikelihoodGrid/torso_range_max",TORSO_DETECTOR_FOV.range.max, 8.0);

        ros::param::param("~/LikelihoodGrid/torso_angle_min",TORSO_DETECTOR_FOV.angle.min, toRadian(-70.0/2));
        ros::param::param("~/LikelihoodGrid/torso_angle_max",TORSO_DETECTOR_FOV.angle.max, toRadian(70.0/2));

        ros::param::param("~/LikelihoodGrid/torso_range_stdev",torso_grid->stdev.range, 0.2);
        ros::param::param("~/LikelihoodGrid/torso_angle_stdev",torso_grid->stdev.angle, 1.0);

        ros::param::param("~/LikelihoodGrid/torso_human_cell_probability",TORSO_CELL_PROBABILITY.human, 0.95);
        ros::param::param("~/LikelihoodGrid/torso_free_cell_probability",TORSO_CELL_PROBABILITY.free, 0.05);
        ros::param::param("~/LikelihoodGrid/torso_unknown_cell_probability",TORSO_CELL_PROBABILITY.unknown, 0.25);

        initTorsoGrid(TORSO_DETECTOR_FOV);
        torso_grid_pub = n.advertise<nav_msgs::OccupancyGrid>("torso_occupancy_grid",10);
    }

    if(SOUND_DETECTION_ENABLE){
        SensorFOV_t SOUND_DETECTOR_FOV = FOV;

        ros::param::param("~/LikelihoodGrid/sound_range_min",SOUND_DETECTOR_FOV.range.min, 1.0);
        ros::param::param("~/LikelihoodGrid/sound_range_max",SOUND_DETECTOR_FOV.range.max, 10.0);

        ros::param::param("~/LikelihoodGrid/sound_angle_min",SOUND_DETECTOR_FOV.angle.min, toRadian(-90.0));
        ros::param::param("~/LikelihoodGrid/sound_angle_max",SOUND_DETECTOR_FOV.angle.max, toRadian(90.0));

        ros::param::param("~/LikelihoodGrid/sound_human_cell_probability",SOUND_CELL_PROBABILITY.human, 0.95);
        ros::param::param("~/LikelihoodGrid/sound_free_cell_probability",SOUND_CELL_PROBABILITY.free, 0.05);
        ros::param::param("~/LikelihoodGrid/sound_unknown_cell_probability",SOUND_CELL_PROBABILITY.unknown, 0.25);

        ros::param::param("~/LikelihoodGrid/sound_range_stdev",sound_grid->stdev.range, 0.5);
        ros::param::param("~/LikelihoodGrid/sound_angle_stdev",sound_grid->stdev.angle, 5.0);

        initSoundGrid(SOUND_DETECTOR_FOV);
        sound_grid_pub = n.advertise<nav_msgs::OccupancyGrid>("sound_occupancy_grid",10);        
    }

    initHumanGrid(FOV);
    human_grid_pub = n.advertise<nav_msgs::OccupancyGrid>("human_occupancy_grid",10);
    local_maxima_pub = n.advertise<geometry_msgs::PoseArray>("local_maxima",10);
    max_prob_pub = n.advertise<geometry_msgs::PointStamped>("maximum_probability",10);

    try
    {
        tf_listener = new tf::TransformListener();
    }
    catch (std::bad_alloc& ba)
    {
      std::cerr << "In Likelihood Grid Interface Constructor: bad_alloc caught: " << ba.what() << '\n';
    }
}

bool CartesianGridInterface::transformToBase(geometry_msgs::PointStamped& source_point,
                                             geometry_msgs::PointStamped& target_point,
                                             bool debug)
{
    bool can_transform;
    try
    {
        tf_listener->transformPoint("base_footprint", source_point, target_point);
        if (debug) {
            tf::StampedTransform _t;
            tf_listener->lookupTransform("base_footprint", source_point.header.frame_id, ros::Time(0), _t);
            ROS_INFO("From %s to bfp: [%.2f, %.2f, %.2f] (%.2f %.2f %.2f %.2f)",
                     source_point.header.frame_id.c_str(),
                     _t.getOrigin().getX(), _t.getOrigin().getY(), _t.getOrigin().getZ(),
                     _t.getRotation().getX(), _t.getRotation().getY(), _t.getRotation().getZ(), _t.getRotation().getW());
        }
        can_transform = true;
    }
    catch(tf::TransformException& ex)
    {
        ROS_ERROR("Received an exception trying to transform a point from \"%s\" to \"base_footprint\": %s", source_point.header.frame_id.c_str(), ex.what());
        can_transform = false;
    }
    return can_transform;
}

bool CartesianGridInterface::transformToBase(const geometry_msgs::PoseArrayConstPtr& source,
                                             geometry_msgs::PoseArray& target,
                                             bool debug)
{
    bool can_transform = true;

    geometry_msgs::PointStamped source_point, target_point;
    geometry_msgs::Pose source_pose, target_pose;

    target_point.header = target.header;
    source_point.header = source->header;

    for(size_t i = 0; i < source->poses.size(); i++){
        source_point.point = source->poses.at(i).position;
        try
        {
            tf_listener->transformPoint("base_footprint", source_point, target_point);
            if (debug) {
                tf::StampedTransform _t;
                tf_listener->lookupTransform("base_footprint", source_point.header.frame_id, ros::Time(0), _t);
                ROS_INFO("From %s to bfp: [%.2f, %.2f, %.2f] (%.2f %.2f %.2f %.2f)",
                         source_point.header.frame_id.c_str(),
                         _t.getOrigin().getX(), _t.getOrigin().getY(), _t.getOrigin().getZ(),
                         _t.getRotation().getX(), _t.getRotation().getY(), _t.getRotation().getZ(), _t.getRotation().getW());
            }
            can_transform = true;
        }
        catch(tf::TransformException& ex)
        {
            ROS_ERROR("Received an exception trying to transform a point from \"%s\" to \"base_footprint\": %s", source_point.header.frame_id.c_str(), ex.what());
            can_transform = false;
        }

        target_pose.position = target_point.point;
        target_pose.position.z = 0.0;
        target.poses.push_back(target_pose);
    }

    return can_transform;
}


void CartesianGridInterface::initLegGrid(SensorFOV_t _FOV)
{
    try
    {
        leg_grid = new CartesianGrid(MAP_SIZE, _FOV, MAP_RESOLUTION, LEG_CELL_PROBABILITY,
                                      TARGET_DETETION_PROBABILITY,
                                      FALSE_POSITIVE_PROBABILITY);
    }
    catch (std::bad_alloc& ba)
    {
        std::cerr << "In new legGrid: bad_alloc caught: " << ba.what() << '\n';
    }
}

void CartesianGridInterface::initTorsoGrid(SensorFOV_t _FOV)
{
    try
    {
        torso_grid = new CartesianGrid(MAP_SIZE, _FOV, MAP_RESOLUTION, TORSO_CELL_PROBABILITY,
                                       TARGET_DETETION_PROBABILITY,
                                       FALSE_POSITIVE_PROBABILITY);
    }
    catch (std::bad_alloc& ba)
    {
        std::cerr << "In new torsoGrid: bad_alloc caught: " << ba.what() << '\n';
    }
}


void CartesianGridInterface::initSoundGrid(SensorFOV_t _FOV)
{
    try
    {
        sound_grid = new CartesianGrid(MAP_SIZE, _FOV, MAP_RESOLUTION, SOUND_CELL_PROBABILITY, TARGET_DETETION_PROBABILITY,
                                       FALSE_POSITIVE_PROBABILITY);
    }
    catch (std::bad_alloc& ba)
    {
        std::cerr << "In new sound_Grid: bad_alloc caught: " << ba.what() << '\n';
    }
}

void CartesianGridInterface::initPeriodicGrid(SensorFOV_t _FOV)
{
    try
    {
        periodic_grid = new CartesianGrid(MAP_SIZE, _FOV, MAP_RESOLUTION, CELL_PROBABILITY, TARGET_DETETION_PROBABILITY,
                                       FALSE_POSITIVE_PROBABILITY);
    }
    catch (std::bad_alloc& ba)
    {
        std::cerr << "In new periodic_grid: bad_alloc caught: " << ba.what() << '\n';
    }
}


void CartesianGridInterface::initHumanGrid(SensorFOV_t _FOV)
{
    try
    {
        human_grid = new CartesianGrid(MAP_SIZE, _FOV, MAP_RESOLUTION, CELL_PROBABILITY,
                                       TARGET_DETETION_PROBABILITY,
                                       FALSE_POSITIVE_PROBABILITY);
    }
    catch (std::bad_alloc& ba)
    {
        std::cerr << "In new humanGrid: bad_alloc caught: " << ba.what() << '\n';
    }
}

void CartesianGridInterface::syncCallBack(const geometry_msgs::PoseArrayConstPtr& leg_msg_crtsn,
                                          const nav_msgs::OdometryConstPtr& encoder_msg)
{
//    ----------   ENCODER CALLBACK   ----------
    if(MOTION_MODEL_ENABLE){
        robot_velocity.linear = sqrt( pow(encoder_msg->twist.twist.linear.x,2) + pow(encoder_msg->twist.twist.linear.y,2) );
        robot_velocity.lin.x = encoder_msg->twist.twist.linear.x;
        robot_velocity.lin.y = encoder_msg->twist.twist.linear.y;
        robot_velocity.angular = encoder_msg->twist.twist.angular.z;
        encoder_diff_time = ros::Time::now() - encoder_last_time;
        encoder_last_time = ros::Time::now();
    }

//    ----------   LEG DETECTION CALLBACK   ----------
    if(LEG_DETECTION_ENABLE){

        leg_grid->crtsn_array.current.poses.clear();

        if(!transformToBase(leg_msg_crtsn, leg_grid->crtsn_array.current)){
            ROS_WARN("Can not transform from laser to base_footprint");
            leg_grid->crtsn_array.current.poses.clear();
        } else{
            ROS_ASSERT(leg_msg_crtsn->poses.size() == leg_grid->crtsn_array.current.poses.size());
            leg_grid->getPose(leg_grid->crtsn_array.current);
            ROS_ASSERT(leg_grid->polar_array.current.size() == leg_msg_crtsn->poses.size());
        }
    }
}

void CartesianGridInterface::torsoCallBack(const autonomy_human::raw_detectionsConstPtr &torso_msg)
{
    if(TORSO_DETECTION_ENABLE)
        torso_grid->getPose(torso_msg);
}


void CartesianGridInterface::soundCallBack(const hark_msgs::HarkSourceConstPtr &sound_msg)
{
    if(SOUND_DETECTION_ENABLE)
        sound_grid->getPose(sound_msg);
}

void CartesianGridInterface::periodicCallBack(const autonomy_human::raw_detectionsConstPtr &periodic_msg)
{
    if(PERIODIC_GESTURE_DETECTION_ENABLE)
        periodic_grid->getPose(periodic_msg);
}

void CartesianGridInterface::occupancyGrid(CartesianGrid* grid, nav_msgs::OccupancyGrid *occupancy_grid)
{
    if(!occupancy_grid->header.frame_id.size()){
        occupancy_grid->info.height = grid->map.height;
        occupancy_grid->info.width = grid->map.width;
        occupancy_grid->info.origin = grid->map.origin;
        occupancy_grid->info.resolution = grid->map.resolution;
        occupancy_grid->header.frame_id = "base_footprint";
    }else{
        occupancy_grid->data.clear();
    }

    for(size_t i = 0; i < grid->grid_size; i++){
        uint cell_prob = (uint) 100 * grid->posterior[i];
        occupancy_grid->data.push_back(cell_prob);
    }
}


void CartesianGridInterface::spin()
{

    leg_grid->diff_time = ros::Time::now() - last_time;
    leg_grid->predict(robot_velocity);

    /* FOR RVIZ */
    leg_grid->crtsn_array.current.header.frame_id = "base_footprint";
    current_leg_base_pub.publish(leg_grid->crtsn_array.current);

    leg_grid->polar2Crtsn(leg_grid->polar_array.predicted, leg_grid->crtsn_array.predicted);
    predicted_leg_base_pub.publish(leg_grid->crtsn_array.predicted);

    leg_grid->polar2Crtsn(leg_grid->polar_array.past, leg_grid->crtsn_array.past);
    last_leg_base_pub.publish(leg_grid->crtsn_array.past);
    /* ******* */

    leg_grid->bayesOccupancyFilter();


    //PUBLISH LEG OCCUPANCY GRID
    occupancyGrid(leg_grid, &leg_occupancy_grid);
    leg_occupancy_grid.header.stamp = ros::Time::now();
    legs_grid_pub.publish(leg_occupancy_grid);
    //---------------------------------------------

    torso_grid->diff_time = ros::Time::now() - last_time;;
    torso_grid->predict(robot_velocity);
    torso_grid->bayesOccupancyFilter();

    //PUBLISH TORSO OCCUPANCY GRID
    occupancyGrid(torso_grid, &torso_occupancy_grid);
    torso_occupancy_grid.header.stamp = ros::Time::now();
    torso_grid_pub.publish(torso_occupancy_grid);
    //---------------------------------------------

    sound_grid->diff_time = ros::Time::now() - last_time;;
    sound_grid->predict(robot_velocity);
    sound_grid->bayesOccupancyFilter();

    //PUBLISH SOUND OCCUPANCY GRID
    occupancyGrid(sound_grid, &sound_occupancy_grid);
    sound_occupancy_grid.header.stamp = ros::Time::now();
    sound_grid_pub.publish(sound_occupancy_grid);

    //---------------------------------------------


    human_grid->diff_time = ros::Time::now() - last_time;;
    human_grid->fuse(sound_grid->posterior, leg_grid->posterior, torso_grid->posterior,
                      FUSE_MULTIPLY);
    human_grid->predict(robot_velocity); // TODO: FIX THIS

    //PUBLISH LOCAL MAXIMA
    human_grid->updateLocalMaximas();
    local_maxima_pub.publish(human_grid->local_maxima_poses);

    //PUBLISH HIGHEST PROBABILITY OF INTEGRATED GRID
    maximum_probability = human_grid->highest_prob_point;
    maximum_probability.header.frame_id = "base_footprint";
    maximum_probability.header.stamp = ros::Time::now();
    max_prob_pub.publish(maximum_probability);

    //PUBLISH INTEGRATED OCCUPANCY GRID
    occupancyGrid(human_grid, &human_occupancy_grid);
    human_occupancy_grid.header.stamp = ros::Time::now();
    human_grid_pub.publish(human_occupancy_grid);
    last_time = ros::Time::now();
}

CartesianGridInterface::~CartesianGridInterface()
{
    ROS_INFO("Deconstructing the constructed LikelihoodGridInterface.");
    if(LEG_DETECTION_ENABLE) delete leg_grid;
    if(TORSO_DETECTION_ENABLE) delete torso_grid;
    if(SOUND_DETECTION_ENABLE) delete sound_grid;
    if(PERIODIC_GESTURE_DETECTION_ENABLE) delete periodic_grid;
    delete human_grid;
    delete tf_listener;
}
