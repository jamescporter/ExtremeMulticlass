#include "util.h"
#include "multi.h"
#include "SBCDsolve.h"
#include "ActBCDsolve.h"

void exit_with_help(){

	cerr << "Usage: ./multiTrain (options) [train_data] (model)" << endl;
	cerr << "options:" << endl;
	cerr << "-s solver: (default 0)" << endl;
	cerr << "	0 -- Stochastic Block Coordinate Descent" << endl;
	cerr << "	1 -- Active Block Coordinate Descent" << endl;
	cerr << "-l lambda: L1 regularization weight (default 1.0)" << endl;
	cerr << "-c cost: cost of each sample (default 1.0)" << endl;
	cerr << "-m max_iter: maximum number of iterations allowed (default 20)" << endl;
	cerr << "-g max_select: maximum number of greedy-selected dual variables per sample (default 10)" << endl;
	exit(0);
}


void parse_cmd_line(int argc, char** argv, Param* param){

	int i;
	for(i=1;i<argc;i++){

		if( argv[i][0] != '-' )
			break;
		if( ++i >= argc )
			exit_with_help();

		switch(argv[i-1][1]){
			
			case 's': param->solver = atoi(argv[i]);
				  break;
			case 'l': param->lambda = atof(argv[i]);
				  break;
			case 'c': param->C = atof(argv[i]);
				  break;
			case 'm': param->max_iter = atoi(argv[i]);
				  break;
			case 'g': param->max_select = atoi(argv[i]);
				  break;
			default:
				  cerr << "unknown option: -" << argv[i-1][1] << endl;
				  exit(0);
		}
	}

	if(i>=argc)
		exit_with_help();

	param->trainFname = argv[i];
	i++;

	if( i<argc )
		param->modelFname = argv[i];
	else{
		param->modelFname = new char[FNAME_LEN];
		strcpy(param->modelFname,"model");
	}
}

void readData(char* fname, Problem* prob)
{
	map<string,int>* label_index_map = &(prob->label_index_map);
	vector<string>* label_name_list = &(prob->label_name_list);
	
	ifstream fin(fname);
	char* line = new char[LINE_LEN];
	
	int d = -1;
	while( !fin.eof() ){
		
		fin.getline(line, LINE_LEN);
		string line_str(line);
		
		if( line_str.length() < 2 && fin.eof() )
			break;

		vector<string> tokens = split(line_str, " ");
		//get label index
		int lab_ind;
		map<string,int>::iterator it;
		if(  (it=label_index_map->find(tokens[0])) == label_index_map->end() ){
			lab_ind = label_index_map->size();
			label_index_map->insert(make_pair(tokens[0],lab_ind));
		}else
			lab_ind = it->second;
		
		
		SparseVec* ins = new SparseVec();
		for(int i=1;i<tokens.size();i++){
			vector<string> kv = split(tokens[i],":");
			int ind = atoi(kv[0].c_str());
			double val = atof(kv[1].c_str());
			ins->push_back(make_pair(ind,val));

			if( ind > d )
				d = ind;
		}
		
		prob->data.push_back(ins);
		prob->labels.push_back(lab_ind);
	}
	fin.close();

	prob->D = d+1; //adding bias
	prob->N = prob->data.size();
	prob->K = label_index_map->size();

	label_name_list->resize(prob->K);
	for(map<string,int>::iterator it=label_index_map->begin();
			it!=label_index_map->end();
			it++)
		(*label_name_list)[it->second] = it->first;
	
	delete[] line;
}

void writeModel( char* fname, Model* model){

	ofstream fout(fname);
	fout << "solver_type MCSVM_CS" << endl;
	fout << "nr_class " << model->K << endl;
	fout << "label ";
	for(vector<string>::iterator it=model->label_name_list->begin(); 
			it!=model->label_name_list->end(); it++)
		fout << *it << " ";
	fout << endl;
	fout << "nr_feature " << model->D << endl;
	fout << "bias -1" << endl << "w" << endl;
	double* w = model->w;
	int D = model->D;
	int K = model->K;
	for(int j=1;j<D;j++){
		for(int k=0;k<K;k++)
			fout << w[j*K+k] << " ";
		fout << endl;
	}
	fout.close();
}

int main(int argc, char** argv){
	
	Param* param = new Param();
	parse_cmd_line(argc, argv, param);
	
	Problem* prob = new Problem();
	readData( param->trainFname, prob);
	param->prob = prob;

	cerr << "N=" << prob->data.size() << endl;
	cerr << "D=" << prob->D << endl; 
	cerr << "K=" << prob->K << endl;
	cerr << "lambda=" << param->lambda << ", C=" << param->C << endl;
	//param->lambda /= prob->N;
	
	if( param->solver == 0 ){
		
		SBCDsolve* solver = new SBCDsolve(param);
		Model* model = solver->solve();
		writeModel(param->modelFname, model);
	}else if( param->solver==1 ){
		
		ActBCDsolve* solver = new ActBCDsolve(param);
		Model* model = solver->solve();
		writeModel(param->modelFname, model);
	}
}
