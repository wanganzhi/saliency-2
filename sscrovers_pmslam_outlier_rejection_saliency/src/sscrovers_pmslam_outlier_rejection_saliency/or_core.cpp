#include "or_core.h"

#include <iostream>
#include <sstream>
#include <fstream>


using namespace std;


ORCore::ORCore(ros::NodeHandle *_n)
{
  step_ = 1;
  curr_pose_ptr_ = new RoverState;
  filter_keypoints_ptr_ = new FilterKeypointsOR(&step_, curr_pose_ptr_, &db_);

  NVec = SALPointVec();
  NMinusOneVec = SALPointVec();
  


  // Initialise node parameters from launch file or command line.
  // Use a private node handle so that multiple instances of the node can be run simultaneously
  // while using different parameters.
  ros::NodeHandle private_node_handle("~");
  private_node_handle.param("rate", rate_, int(10));
  // topic names
  private_node_handle.param("output_features_topic_name", sub_features_topic_name_, string("/saliency/features_Hou"));
  private_node_handle.param("sub_trajectory_topic_name", sub_trajectory_topic_name_, string("est_traj"));
  private_node_handle.param("pub_ptpairs_topic_name", pub_ptpairs_topic_name_, string("ptpairs"));
  //camera params - TODO from camera info?, camera image? param server?
  private_node_handle.param("px", filter_keypoints_ptr_->px_, int(320));
  private_node_handle.param("py", filter_keypoints_ptr_->py_, int(240));
  //pmslam params
  private_node_handle.param("lm_track", filter_keypoints_ptr_->lm_track_, int(94));
  private_node_handle.param("scale", filter_keypoints_ptr_->scale_, int(1));
  // debug flags
  private_node_handle.param("features_to_file", features_to_file_f_, bool(false));
  private_node_handle.param("ptpoints_to_file", ptpoints_to_file_f_, bool(false));
  private_node_handle.param("db_to_file", db_to_file_f_, bool(false));
  private_node_handle.param("traj_to_file", traj_to_file_f_, bool(false));



  //subscribers
  features_sub_ = _n->subscribe(sub_features_topic_name_.c_str(), 1, &ORCore::featuresCallback, this);
  trajectory_sub_ = _n->subscribe(sub_trajectory_topic_name_.c_str(), 1, &ORCore::trajectoryCallback, this);

  //publishers
  ptpairs_pub_ = _n->advertise<sscrovers_pmslam_common::PtPairs>(pub_ptpairs_topic_name_.c_str(), 1);
  db_pub_ = _n->advertise<sscrovers_pmslam_common::DynamicArray>("temp_db", 1);

  data_completed_f_ = false;
}

ORCore::~ORCore()
{

}

void ORCore::process()
{
  //put here everything that should run with node loop
  //if get complet data and step is updated
  if ((data_completed_f_) && (step_ > 0))
  {
    data_completed_f_ = false;
    //filter_keypoints_ptr_->filterKeypoints();
    filter();
    //publishPtPairs();
    //publishDB(); //send data to DB
  }
}

void ORCore::sendToSurfDataBase()
{
  //publishing all db
}

void ORCore::publishPtPairs()
{
  // push forward stamp
  ptpairs_msg_.header.stamp = stamp_;
  // size of data to publish
  int size = filter_keypoints_ptr_->pt_pairs_.size();
  // reserve space for data
  ptpairs_msg_.pairs.resize(size);
  // copy data to message structure
  memcpy(ptpairs_msg_.pairs.data(), filter_keypoints_ptr_->pt_pairs_.data(), size * sizeof(int)); //change to ptpairs_msg_
  // publish msg
  ptpairs_pub_.publish(ptpairs_msg_);

}

void ORCore::featuresCallback(const geometry_msgs::PoseArray& msg)
{
	saliency_poses_vec.clear();
	SALPoint spTemp;
	NVec = SALPointVec(*curr_pose_ptr_, std::time(0));
	for(int i=0; i< msg.poses.size();i++){
		saliency_poses p;
		p.centroid_x = msg.poses[i].position.x;
		p.centroid_y = msg.poses[i].position.y;
		p.width = msg.poses[i].orientation.w;
		p.height = msg.poses[i].orientation.z;
		spTemp = SALPoint(msg.poses[i].position.x,msg.poses[i].position.y,msg.poses[i].orientation.w, msg.poses[i].orientation.z );
		NVec.push_back(spTemp);
		saliency_poses_vec.push_back(p);
	}

	process( );
	//filter();

}
 vector<int> ORCore::bubblesort(vector<int> w, vector<int> w2) 
{ 
	ROS_INFO("Bubblesort begin");
	int temp, temp2;
	bool finished = false;
	 while (!finished) 
	{ 
		finished = true;
		for (int i = 0; i < w.size()-1; i++) { 
			if (w[i] > w[i+1]) { 
				temp = w[i];
				temp2 = w2[i];
				 w[i] = w[i+1];
				 w2[i] = w2[i+1];
				 w[i+1] = temp;
				 w2[i+1] = temp2;
				 finished=false; 
			} 
		} 
	} 
	ROS_INFO("Bubblesort end");
	return w2; 
}

ptpairs ORCore::nearestNeighbour(){
	ROS_INFO("NN begin");
	int closest;
	ptpairs ptpairs_local;
	float dmin;	
	float d, area, areaMO;
	for(int i = 0; i<NVec.size(); i++){
		dmin=10000;
		closest=i;
		for(int j=0; j<NMinusOneVec.size(); j++){
			d = NMinusOneVec.at(j).dist(NVec.at(i));
			area = NVec.at(i).height * NVec.at(i).width;
			areaMO = NMinusOneVec.at(j).width * NMinusOneVec.at(j).height;
			if(d<dmin && area < 4*areaMO && 4 * area > areaMO){
				dmin=d;
				closest = j;
			}
		}
		
		ptpairs_local.add(closest,dmin,-1);
	}
	ROS_INFO("NN End");
	return ptpairs_local;
}

ptpairs ORCore::outlierRejection(ptpairs inP){
	vector<cv::Point2f> points1, points2;
	cv::Point2f pTemp;
	ROS_INFO("Outlier Rejection begin, %i", inP.size());
	for(int i = 0; i<inP.size(); i++){
		ROS_INFO("Before j = %i, frame= %i",inP.atJ(i), inP.atFrame(i));
	}
	std::vector <int> lis;
	for(int i = 0; i<inP.size(); i++){
		if(inP.pairs[i].frame == -1){
			pTemp.x = NMinusOneVec.atX(inP.atJ(i));
			pTemp.y = NMinusOneVec.atY(inP.atJ(i));
			points1.push_back(pTemp);
			pTemp.x = NVec.atX(i);
			pTemp.y = NVec.atY(i);
			points2.push_back(pTemp);
			lis.push_back(i);
		}
	}
	vector<uchar> outliers;
	vector<cv::Point2f> points1_temp, points2_temp;
	Mat F = findFundamentalMat_local(Mat(points1), Mat(points2), FM_RANSAC, 1, 0.99, &outliers);
ROS_INFO("sad");
	for(int u=0; u<outliers.size(); u++){
		if (outliers[u] == 1){
ROS_INFO("Outlier %i, %i", u, lis[u]);
			inP.pairs[lis[u]].j = lis[u];
			inP.pairs[lis[u]].d = 0;
			inP.pairs[lis[u]].frame = 0;
		}	
	}
	for(int i = 0; i<inP.size(); i++){
		ROS_INFO("afterj = %i, frame= %i",inP.atJ(i), inP.atFrame(i));
	}
	ROS_INFO("Outlier Rejection end");
	return inP;
}



void ORCore::filter()
{
	if(database.empty()){
		//fill database
		database = saliency_poses_vec;
		database_all = saliency_poses_vec;
		NMinusOneVec = NVec;
		SDB.push_back(NVec);
		ptpair py;
		ptpairs pyVec;
		for(int i=0; i<NVec.size();i++){
			py.i = i;
			py.j = i;
			py.frame =0;
			py.d = 0;
			pyVec.push_back(py);
		}
		SDB.ptpars.push_back(pyVec);
	}else{
		//nearest neighbour
		//for each
		/*
		int closest = -1;
		vector<int> ptpairTemp, ptpairsNew;
		float dmin =10000;	
		float d;
		for(int i = 0; i<database.size(); i++){
			dmin=10000;
			closest=-1;
			for(int j=0; j<saliency_poses_vec.size(); j++){
				d = (database[i].centroid_x- saliency_poses_vec[j].centroid_x)*(database[i].centroid_x- saliency_poses_vec[j].centroid_x) + (database[i].centroid_y- saliency_poses_vec[j].centroid_y)*(database[i].centroid_y- saliency_poses_vec[j].centroid_y);
				if(d<dmin){
					dmin=d;
					closest = j;
				}
			}
			ptpairTemp.push_back(closest);
		}
		//reverse
		vector<int> ptpair_reverse;
		for(int q=0;q<ptpairTemp.size(); q++){
			ptpair_reverse.push_back(q);
		}
		
		//sort
		ptpair_reverse = bubblesort(ptpairTemp,ptpair_reverse);
		for(int qq=0;qq<ptpair_reverse.size(); qq++){
			ROS_INFO("%i", ptpair_reverse[qq]);
		}
		*/

		ptpairs outPa = nearestNeighbour();

		//Outlier Rejection 
		/*
		vector<cv::Point2f> points1, points2;
		cv::Point2f pTemp;
		for(int i = 0; i<database.size(); i++){
			pTemp.x = database[ptpair_reverse[i]].centroid_x;
			pTemp.y = database[ptpair_reverse[i]].centroid_y;
			points1.push_back(pTemp);
			//ROS_INFO("First =%i, x= %f  y= %f",i , pTemp.x,pTemp.y);
			pTemp.x = saliency_poses_vec[i].centroid_x;
			pTemp.y = saliency_poses_vec[i].centroid_y;
			points2.push_back(pTemp);
			//ROS_INFO("Connect =%i, x= %f  y= %f",i , pTemp.x,pTemp.y);
		}

		vector<uchar> outliers;
		vector<cv::Point2f> points1_temp, points2_temp;
		Mat F = findFundamentalMat_local(Mat(points1), Mat(points2), FM_RANSAC, 1, 0.99, &outliers);
		vector<saliency_poses> tempPoses;
		for(int u=0; u<outliers.size(); u++){
  			if (outliers[u] == 0){
				points1_temp.push_back(points1[u]);
				//ROS_INFO("First =%i, x= %f  y= %f",u ,points1_temp[u].x,points1_temp[u].y);
				points2_temp.push_back(points2[u]);
				//ROS_INFO("Connect =%i, x= %f  y= %f",u , points2_temp[u].x,points2_temp[u].y);

				
				ptpairsNew.push_back(ptpair_reverse[u]);
			}	

		}
		*/
		ptpairs outP = outlierRejection(outPa);

		//k-nn tracking all frames
		/*
		database_all.push_back(database)
		for(int gg=0; gg<ptpairsNew.size(); gg++){
			for(int hh=0; hh<ptpairs_all.size(); hh++){
				
			}
		}
*/
		//move database

ROS_INFO("%i == %i == %i",NVec.size(), outPa.size(), outP.size());
		NMinusOneVec = NVec;
		ptpairs ptVec;
		for(int uu=0;uu<NVec.size();uu++ ){
			ptpair ppp;
			ppp.i = uu;
			pairInts pi = resolve(outP.at(uu));
			ppp.j = pi.j;
			ppp.d = outP.atD(uu); 
			ppp.frame = pi.frame;
			ptVec.push_back(ppp);	

		}

		SDB.ptpars.push_back(ptVec);
		SDB.push_back(NVec);
ROS_INFO("se");
		// ptpairs_local -> ptpairs_global

		database = saliency_poses_vec;
		//push  to publish
	}	
}

pairInts ORCore::resolve(ptpair p){
	pairInts pI;
	if (p.frame != 0){
		ptpair match = SDB.ptpars.at(SDB.size()+p.frame).at(p.j);
		if (match.frame == 0){
			pI.frame = SDB.size()-p.frame;
			pI.j = match.j;
		}else{
			pI.frame = match.frame;
			pI.j = match.j;
		}	
	}else{
		pI.j = p.j;
		pI.frame = SDB.size();
	}
		
	return pI;
}

cv::Mat ORCore::findFundamentalMat_local(const Mat& points1, const Mat& points2, int method, double param1, double param2, vector<
    uchar>* mask)
{
  CV_Assert(points1.checkVector(2) >= 0 && points2.checkVector(2) >= 0 &&
      (points1.depth() == CV_32F || points1.depth() == CV_32S) &&
      points1.depth() == points2.depth());  
  Mat F(3, 3, CV_64F);
  CvMat _pt1 = Mat(points1), _pt2 = Mat(points2);
  CvMat matF = F, _mask, *pmask = 0;
  if( mask )
  {
    mask->resize(points1.cols*points1.rows*points1.channels()/2);
    pmask = &(_mask = cvMat(1, (int)mask->size(), CV_8U, (void*)&(*mask)[0]));
  }
  int n = cvFindFundamentalMat( &_pt1, &_pt2, &matF, method, param1, param2, pmask );


  if( n <= 0 )
  F = Scalar(0);
  return F;
}


void ORCore::trajectoryCallback(const nav_msgs::PathConstPtr& msg)
{

  est_traj_msg_ = *msg;
  if (step_ >= 0)
  {
    if ((int)est_traj_msg_.poses.size() > step_)
    {
      curr_pose_ptr_->x = est_traj_msg_.poses[step_].pose.position.x;
      curr_pose_ptr_->y = est_traj_msg_.poses[step_].pose.position.y;
      curr_pose_ptr_->z = est_traj_msg_.poses[step_].pose.position.z;

      double _roll, _pitch, _yaw;
#if ROS_VERSION_MINIMUM(1, 8, 0) // if current ros version is >= 1.8.0 (fuerte)
      //min. fuerte
      tf::Quaternion _q;
      tf::quaternionMsgToTF(est_traj_msg_.poses[step_].pose.orientation, _q);
      tf::Matrix3x3(_q).getRPY(_roll, _pitch, _yaw);
#else
      //electric and older
      btQuaternion _q;
      tf::quaternionMsgToTF(est_traj_msg_.poses[step_].pose.orientation, _q);
      btMatrix3x3(_q).getRPY(_roll, _pitch, _yaw);
#endif

      curr_pose_ptr_->roll = _roll;
      curr_pose_ptr_->pitch = _pitch;
      curr_pose_ptr_->yaw = _yaw;

      data_completed_f_ = true;

    }
    else
    {
      ROS_WARN("There is no trajectory point corresponding to features data...");
      data_completed_f_ = false;
    }
  }

  
}

void ORCore::publishDB()
{
  //ROS_STATIC_ASSERT(sizeof(MyVector3) == 24);

  sscrovers_pmslam_common::DynamicArray serialized_db;

  serialized_db.header.stamp.nsec = step_;

  serialized_db.dims.push_back(db_.storage_->size());
  serialized_db.dims.push_back(1);
  serialized_db.types.push_back("SURFPoint");
  serialized_db.data.resize(sizeof(SURFPoint) * db_.storage_->size());
  memcpy(serialized_db.data.data(), db_.storage_->data(), serialized_db.dims[0] * sizeof(SURFPoint));

  db_pub_.publish(serialized_db);
}

//-----------------------------MAIN-------------------------------

int main(int argc, char **argv)
{
  // Set up ROS.
  ros::init(argc, argv, "outlier_rejection");
  ros::NodeHandle n;

  // Create a new NodeExample object.
  ORCore *or_core = new ORCore(&n); // __atribute__... only for eliminate unused variable warning :)

  //until we use only img callback, below is needless
  // Tell ROS how fast to run this node.
  ros::Rate r(or_core->rate_);

  // Main loop.

  while (n.ok())
  {
    //or_core->Process();
    //fd_core->Publish();
    ros::spinOnce();
    r.sleep();
  }

  return 0;
} // end main()