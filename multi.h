#ifndef MULTITRAIN
#define MULTITRAIN

#include "util.h"

class Problem{
	public:
	map<string,int> label_index_map;
	vector<string> label_name_list;
	vector<SparseVec*> data;
	vector<int> labels;
	int N;//number of samples
	int D;//dimension
	int K;
};


class Param{
	public:
	char* trainFname;
	char* modelFname;
	double lambda; //for L1-norm (default 1/N)
	double C; //weight of loss
	int max_iter;
	Problem* prob;
	Param(){
		lambda = 1.0;
		C = 1.0;
		max_iter = 20;
	}
};

class Model{
	public:
	Model(Problem* prob, double* _w){
		D = prob->D;
		K = prob->K;
		label_name_list = &(prob->label_name_list);
		w = _w;
	}
		
	int D;
	int K;
	double* w;
	vector<string>* label_name_list;
};

#endif
