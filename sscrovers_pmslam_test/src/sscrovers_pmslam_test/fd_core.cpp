#include "fd_core.h"
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

static const char WINDOW[] = "fd_window";


FDCore::FDCore(ros::NodeHandle *_n) :
    it_(*_n)
{

#if (CV_MAJOR_VERSION >= 2) && (CV_MINOR_VERSION >= 4)
  cv::initModule_nonfree();
#endif
  // Initialise node parameters from launch file or command line.
  // Use a private node handle so that multiple instances of the node can be run simultaneously
  // while using different parameters.
  ros::NodeHandle private_node_handle("~");
  private_node_handle.param("rate", rate_, int(10));
  //topics name
  private_node_handle.param("input_img_topic_name", in_img_topic_name_, string("org_image"));
  private_node_handle.param("output_img_topic_name", out_img_topic_name_, string("debug_image"));
  private_node_handle.param("output_features_topic_name", out_features_topic_name_, string("features"));
  private_node_handle.param("sub_pt_topic_name", sub_pt_topic_name_, string("ptpairs"));
  private_node_handle.param("sub_db_topic_name", sub_db_topic_name_, string("SAL_db"));
  //debug to files storing
  private_node_handle.param("debug_features_to_file", features_to_file_f_, bool(false));
  //display output img
  private_node_handle.param("disp_img", disp_img_f_, bool(true));
  //publish output img
  private_node_handle.param("pub_img", pub_output_image_f_, bool(false));
  //private_node_handle.param("hess_no", hess_no_, int(500));

  //subscribers
  image_sub_ = it_.subscribe(in_img_topic_name_.c_str(), 1, &FDCore::imageCallBack, this);
  ptpairs_sub_ = _n->subscribe(sub_pt_topic_name_.c_str(), 1, &FDCore::ptpairsCallBack, this);
  features_db_sub_ = _n->subscribe(sub_db_topic_name_.c_str(), 1, &FDCore::featuresDBCallBack, this);

  //initialize empty images
  cv_input_img_ptr_.reset(new cv_bridge::CvImage);
  cv_output_img_ptr_.reset(new cv_bridge::CvImage);

  //initialize features structures
  keypoints_ = 0, descriptors_ = 0;

  //create window for output image
  if (disp_img_f_)
    cv::namedWindow(WINDOW);
}

FDCore::~FDCore()
{
  //destroy window for output image
  if (disp_img_f_)
    cv::destroyWindow(WINDOW);
}

void FDCore::imageCallBack(const sensor_msgs::ImageConstPtr& msg)
{
  try
  {
    cv_input_img_ptr_ = cv_bridge::toCvCopy(msg, enc::MONO8);
  }
  catch (cv_bridge::Exception& e)
  {
    ROS_ERROR("cv_bridge exception: %s", e.what());
    return;
  }

  process();
}


void FDCore::process()
{
  //put here everything that should run with node loop
  //edit image

  editImage();

  publishImage();
}


void FDCore::editImage()
{
	for(int m=0; m<ptpairs_msg_.pairs.size();m++){
		float x = sal_db[ptpairs_msg_.pairs[m]].x;
		float y = sal_db[ptpairs_msg_.pairs[m]].y;
	}
  //cv_input_img_ptr_ 
}


void FDCore::ptpairsCallBack(const sscrovers_pmslam_common::PtPairsConstPtr& msg)
{
  ptpairs_msg_ = *msg;
}

void FDCore::featuresDBCallBack(const sscrovers_pmslam_common::SALVector& msg)
{

  if (msg.dims > 0)
  {
    sal_db.resize(msg.dims);
    memcpy(&sal_db, msg.data.data(), msg.dims * sizeof(sscrovers_pmslam_common::SPoint));
  }
  else
    ROS_ERROR("No data in database topic");

}

void FDCore::publishImage()
{
  cv_output_img_ptr_->header.stamp = cv_input_img_ptr_->header.stamp;

  cv::cvtColor(cv_input_img_ptr_->image, cv_output_img_ptr_->image, CV_GRAY2BGR);

 
    for (int i = 0; i < keypoints_->total; i++)
    {
      CvSURFPoint* r = (CvSURFPoint*)cvGetSeqElem(keypoints_, i);
      CvPoint center;
      int radius;
      center.x = cvRound(r->pt.x);
      center.y = cvRound(r->pt.y);
      radius = cvRound(r->size * 1.2 / 9. * 2);
      cv::circle(cv_output_img_ptr_->image, center, radius, cvScalar(0, 255, 0), 1, 8, 0);
    }

    cv::imshow(WINDOW, cv_output_img_ptr_->image);
    cv::waitKey(3);
  

}



//-----------------------------MAIN----------------------------
int main(int argc, char **argv)
{
  // Set up ROS.
  ros::init(argc, argv, "feature_detection");
  ros::NodeHandle n;
  // Create a new NodeExample object.
  FDCore* __attribute__((unused)) fd_core_ptr_ = new FDCore(&n);

  ros::spin();

  return 0;
} // end main()
