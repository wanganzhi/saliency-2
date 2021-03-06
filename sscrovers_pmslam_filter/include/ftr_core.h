#ifndef FTR_CORE_H
#define FTR_CORE_H

// ROS includes
#include "ros/ros.h"
#include "ros/time.h"
#include "tf/tf.h"

//ROS messages
#include <nav_msgs/Path.h>
#include <sscrovers_pmslam_common/ControlVector.h>
#include <sscrovers_pmslam_common/DynamicArray.h>
#include <sscrovers_pmslam_common/PairedPoints3D.h>
#include <sscrovers_pmslam_common/Map3D.h>
#include <sscrovers_pmslam_common/PMSlamData.h>

#include "information_filter_ftr.h"

class FtrCore
{
public:
  //! Constructor.
  FtrCore(ros::NodeHandle *_n);
  //! Destructor.
  ~FtrCore();

  //! rate for node main loop
  int rate_;

  //!Everything in this function is processed in node loop
  void process();

private:

  //! current step
  int step_;

  //! update flags
  bool first_frame_f_;
  bool traj_update_f_;

  //! to file flags
  bool db_to_file_f_;
  bool pt3d_to_file_f_;

  //! topic names
  string pub_map3d_topic_name_, pub_output_data_topic_name_, sub_ctrl_vec_topic_name_, sub_trajectory_topic_name_,
         sub_ptpairs3d_topic_name_, sub_db_topic_name_;

  //! output map publisher
  ros::Publisher map3d_pub_;

  //! output pm_data publisher
  ros::Publisher pmdata_pub_;

  //! temporary db publisher
  ros::Publisher db_pub_;

  //! control vector subscriber
  ros::Subscriber ctrl_vec_sub_;

  //! travelled trajectory subscriber
  ros::Subscriber trajectory_sub_;

  //! 3d points with indexes to data base subscriber
  ros::Subscriber ptpairs3d_sub_;

  //! features database subscriber
  ros::Subscriber features_db_sub_;

  //! Object of functional class
  InformationFilterFtr* info_filter_ptr_;

  //! output Map3d message
  sscrovers_pmslam_common::Map3D map3d_msg_;

  //! output PMSlamData message
  sscrovers_pmslam_common::PMSlamData pmslam_data_msg_;

  //! input control vector message
  geometry_msgs::PoseStamped ctrl_vec_;

  //! input trajectory
  nav_msgs::Path est_traj_;

  //! input points 3d
  vector<CvPoint3D64f> points3d_;

  //! input point pairs
  vector<int> ptpairs_;

  //! current pose (state) of rover
  RoverState curr_pose_;

  //! input features database object
  FeaturesDB_t db_;

  //! temporary - publish changed db for visualization module - should be changed to add to db
  void publishDB();

  //! output map3d publication
  void publishMap3D();

  //! output pmslam data publication
  void publishPMSlamData();

  //! Callback function for control vector subscription
  void ctrlvecCallBack(const geometry_msgs::PoseStampedConstPtr& msg);

  //! Callback function for travelled trajectory subscription
  void trajectoryCallBack(const nav_msgs::PathConstPtr& msg);

  //! Callback function for 3d points with index to pair with data base record subscription
  void ptpairs3dCallBack(const sscrovers_pmslam_common::PairedPoints3DConstPtr& msg);

  //! Callback function for features database subscription.
  void featuresDBCallBack(const sscrovers_pmslam_common::DynamicArrayConstPtr& msg);
};

#endif //FTR_CORE_H
