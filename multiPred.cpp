#include "multi.h"

Model* readModel(char* file){
	
	Model* model = new Model();

	ifstream fin(file);
	char* tmp = new char[LINE_LEN];
	fin >> tmp >> (model->K);
	
	fin >> tmp;
	string name;
	for(int k=0;k<model->K;k++){
		fin >> name;
		model->label_name_list->push_back(name);
		model->label_index_map->insert(make_pair(name,k));
	}
	
	fin >> tmp >> (model->D);
	model->w = new HashVec*[model->D];
	
	vector<string> ind_val;
	int nnz_j;
	for(int j=0;j<model->D;j++){
		fin >> nnz_j;
		HashVec* wj = new HashVec(nnz_j);
		for(int r=0;r<nnz_j;r++){
			fin >> tmp;
			ind_val = split(tmp,":");
			int k = atoi(ind_val[0].c_str());
			double val = atof(ind_val[1].c_str());
			wj->insert(make_pair(k,val));
		}
		model->w[j] = wj;
	}
	
	delete[] tmp;
	return model;
}

int main(int argc, char** argv){
	
	if( argc < 1+2 ){
		cerr << "multiPred [testfile] [model]" << endl;
		exit(0);
	}

	char* testFile = argv[1];
	char* modelFile = argv[2];
	
	Model* model;
	model = readModel(modelFile);
	
	Problem* prob = new Problem();
	readData( testFile, prob );

	cerr << "Ntest=" << prob->N << endl;
	
	//compute accuracy
	vector<SparseVec*>* data = &(prob->data);
	vector<int>* labels = &(prob->labels);
	int hit=0;
	double* prod = new double[model->K];
	for(int i=0;i<prob->N;i++){
		for(int k=0;k<model->K;k++)
			prod[k] = 0.0;
		
		SparseVec* xi = data->at(i);
		int yi = labels->at(i);
		for(SparseVec::iterator it=xi->begin(); it!=xi->end(); it++){

			int j= it->first;
			double xij = it->second;
			if( j >= model->D )
				continue;
			
			HashVec* wj = model->w[j];
			for(HashVec::iterator it2=wj->begin(); it2!=wj->end(); it2++){
				prod[it2->first] += it2->second*xij;
			}
		}

		double max_val = -1e300;
		int argmax;
		for(int k=0;k<model->K;k++)
			if( prod[k] > max_val ){
				max_val = prod[k];
				argmax = k;
			}

		if( prob->label_name_list[yi] == model->label_name_list->at(argmax) )
			hit++;
	}

	cerr << "Acc=" << ((double)hit/prob->N) << endl;
}
