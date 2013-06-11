#ifndef SALPOINTDB_H
#define SALPOINTDB_H

#include <vector>
#include "RoverState.h"
#include "SALPoint.h"
#include "ptpairs.h"
#include "SALPointVec.h"
#include<ctime>




class SALPointDB{

	public:
		
		std::vector <SALPointVec> SALdata;
		std::vector <ptpairs> ptpars;
		
		SALPointDB(){
		};	
		
		void push_back(SALPointVec in){
			SALdata.push_back(in);
		};
		
		void new_vec(RoverState stateIn, std::time_t tIn){
			SALPointVec latest(stateIn, tIn);
			SALdata.push_back(latest);
		};

		void add_point(SALPoint sa){
			SALdata.at(SALdata.size()-1).push_back(sa);
		};

		int size(){
			return SALdata.size();
		}

		//TODO Eventually conversion stuff HERE


};

#endif