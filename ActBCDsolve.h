#include "util.h"
#include "multi.h"

class ActBCDsolve{
	
	public:
	ActBCDsolve(Param* param){
		
		prob = param->prob;
		lambda = param->lambda;
		C = param->C;
		
		data = &(prob->data);
		labels = &(prob->labels);
		D = prob->D;
		N = prob->N;
		K = prob->K;

		max_iter = param->max_iter;
		max_select = param->max_select;
	}
	
	~ActBCDsolve(){
	}

	Model* solve(){
		
		//initialize alpha and v ( s.t. v = X^Talpha )
		alpha = new double*[N];
		for(int i=0;i<N;i++){
			alpha[i] = new double[K];
			for(int k=0;k<K;k++)
				alpha[i][k] = 0.0;
		}
		v = new double[D*K]; //w = prox(v);
		for(int i=0;i<D*K;i++)
			v[i] = 0.0;
		
		//initialize Q_diag (Q=X*X') for the diagonal Hessian of each i-th subproblem
		Q_diag = new double[N];
		for(int i=0;i<N;i++){
			SparseVec* ins = data->at(i);
			double sq_sum = 0.0;
			for(SparseVec::iterator it=ins->begin(); it!=ins->end(); it++)
				sq_sum += it->second*it->second;
			Q_diag[i] = sq_sum;
		}
		
		//indexes for permutation of [N]
		int* index = new int[N];
		for(int i=0;i<N;i++)
			index[i] = i;
		//initialize active set out of [K] for each sample i
		vector<int>* act_k_index = new vector<int>[N];
		
		//main loop
		double starttime = omp_get_wtime();
		double search_time=0.0, subsolve_time=0.0, maintain_time=0.0;
		double* alpha_i_new = new double[K];
		int iter = 0;
		while( iter < max_iter ){
			
			random_shuffle( index, index+N );
			for(int r=0;r<N;r++){	
				
				int i = index[r];
				double* alpha_i = alpha[i];
				
				//search active
				
				//solve subproblem
				if( act_k_index[i].size() < 2 )
					continue;
				subsolve_time -= omp_get_wtime();
				subSolve(i, act_k_index[i], alpha_i_new);
				subsolve_time += omp_get_wtime();
				
				//maintain v =  X^T\alpha;  w = prox_{l1}(v);
				maintain_time -= omp_get_wtime();
				SparseVec* x_i = data->at(i);
				for(SparseVec::iterator it=x_i->begin(); it!=x_i->end(); it++){

					int f_offset = it->first*K;
					double f_val = it->second;
					for(vector<int>::iterator it=act_k_index[i].begin(); it!=act_k_index[i].end(); it++){
						int k = *it;
						int ind = f_offset+k;
						v[ ind ] += f_val*(alpha_i_new[k]-alpha_i[k]);
					}
				}
				//update alpha
				for(vector<int>::iterator it=act_k_index[i].begin(); it!=act_k_index[i].end(); it++){
					int k = *it;
					alpha_i[k] = alpha_i_new[k];
				}
				//shrink act_k_index
				vector<int> tmp_vec;
				tmp_vec.reserve(act_k_index[i].size());
				for(vector<int>::iterator it=act_k_index[i].begin(); it!=act_k_index[i].end(); it++){
					int k = *it;
					if( alpha_i[k] != 0.0 )
						tmp_vec.push_back(k);
				}
				act_k_index[i] = tmp_vec;
				
				maintain_time += omp_get_wtime();
			}
			
			if( iter % 1 == 0 ){
				cerr << "." ;
				/*double act_size_avg = 0.0;
				for(int i=0;i<N;i++){
					act_size_avg += act_k_index[i].size();	
				}
				act_size_avg /= N;
				cerr << "act="<< act_size_avg << endl;*/
			}
			
			iter++;
		}
		double endtime = omp_get_wtime();
		cerr << endl;
		
		//convert v into w
		for(int i=0;i<D*K;i++)
			v[i] = prox_l1(v[i],lambda);
		double* w = v;
		
		double d_obj = 0.0;
		int nSV = 0;
		int nnz_w = 0;
		int jk=0;
		for(int j=0;j<D;j++){
			for(int k=0;k<K;k++,jk++){
				if( fabs(w[jk]) > 1e-12 ){
					d_obj += w[jk]*w[jk];
					nnz_w++;
				}
			}
		}
		d_obj/=2.0;
		for(int i=0;i<N;i++){
			int yi = labels->at(i);
			for(int k=0;k<K;k++){
				if(k!=yi)
					d_obj += alpha[i][k];
				if( fabs( alpha[i][k] ) > 1e-12 )
					nSV++;
			}
		}
		cerr << "dual_obj=" << d_obj << endl;
		cerr << "nSV=" << nSV << " (NK=" << N*K << ")"<< endl;
		cerr << "nnz_w=" << nnz_w << " (DK=" << D*K << ")" << endl;
		cerr << "train time=" << endtime-starttime << endl;
		cerr << "search time=" << search_time << endl;
		cerr << "subsolve time=" << subsolve_time << endl;
		cerr << "maintain time=" << maintain_time << endl;
		//delete algorithm-specific variables
		delete[] alpha_i_new;
		delete[] act_k_index;
		for(int i=0;i<N;i++)
			delete[] alpha[i];
		delete[] alpha;
		delete[] Q_diag;
		
		return new Model(prob, w); //v is w
	}
	
	/*void searchActive( double* v, vector<int>* act_k_index){
		
		// convert v to w (in SparseMtrix format)
		SparseVec* w = new SparseVec[D];
		for(int j=0;j<D;j++){
			w[j].reserve(K);
			int j_offset = j*K;
			for(int k=0;k<K;k++){
				double v_jk = v[j_offset + k];
				if( fabs(v_jk) > lambda ){
					if(v_jk>0.0) w[j].push_back(make_pair(k,v_jk-lambda));
					else	     w[j].push_back(make_pair(k,v_jk+lambda));
				}	
			}
		}
		
		// Row-wise Sparse Matrix Multiplication X*W (x_i*W, i=1...N)
		double* prod_xi_w = new double[K];
		int* k_index = new int[K];
		for(int k=0;k<K;k++)
			k_index[k] = k;
		for(int i=0;i<N;i++){
			SparseVec* xi = data->at(i);
			int yi = labels->at(i);

			for(int k=0;k<K;k++)
				prod_xi_w[k] = 0.0;
			for(SparseVec::iterator it=xi->begin(); it!=xi->end(); it++){
				int j = it->first;
				double xij = it->second;
				for(SparseVec::iterator it2=w[j].begin(); it2!=w[j].end(); it2++){
					prod_xi_w[it2->first] += xij*it2->second;
				}
			}	

			sort(k_index, k_index+K, ScoreComp(prod_xi_w));
			int num_select=0;
			for(int r=0;r<K;r++){
				int k = k_index[r];
				if( k == yi || alpha[i][k]<0.0 )
					continue;
				if( prod_xi_w[k] > -1.0 ){
					act_k_index[i].push_back(k);
					num_select++;
					if( num_select >= max_select )
						break;
				}
			}
		}
		
		delete[] k_index;
		delete[] prod_xi_w;
		delete[] w;
	}*/
	
	void subSolve(int i, vector<int>& act_k_index, double* alpha_i_new){
		
		int act_k_size = act_k_index.size();
		grad = new double[act_k_size];
		Dk = new double[act_k_size];
		
		int yi = labels->at(i);
		SparseVec* x_i = data->at(i);
		double Qii = Q_diag[i];
		double* alpha_i = alpha[i];
		//compute gradient of each k
		for(int j=0;j<act_k_size;j++){
			int k = act_k_index[j];
			if( k!=yi )
				grad[j] = 1.0 - Qii*alpha_i[k];
			else
				grad[j] = - Qii*alpha_i[k];
		}
		for(SparseVec::iterator it=x_i->begin(); it!=x_i->end(); it++){
			int fea_offset = it->first*K;
			double fea_val = it->second;
			for(int j=0;j<act_k_size;j++){
				int k = act_k_index[j];
				double vjk = v[fea_offset+k];
				if( fabs(vjk) > lambda ){
					if( vjk > 0 )
						grad[j] += (vjk-lambda)*fea_val;
					else
						grad[j] += (vjk+lambda)*fea_val;
				}
			}
		}
		for(int j=0;j<act_k_size;j++){
			int k = act_k_index[j];
			if( k!=yi )
				Dk[j] = grad[j];
			else
				Dk[j] = grad[j] + Qii*C;
		}
		
		//sort according to D_k = grad_k + Qii*((k==yi)?C:0)
		sort( Dk, Dk+act_k_size, greater<double>() );
		
		//compute beta by traversing k in descending order of D_k
		double beta = Dk[0] - Qii*C;
		int r;
		for(r=1;r<act_k_size && beta<r*Dk[r];r++){
			beta += Dk[r];
		}
		beta = beta / r;
		
		//update alpha
		for(int j=0;j<act_k_size;j++){
			int k = act_k_index[j];
			alpha_i_new[k] = min( (k!=yi)?0.0:C, (beta-grad[j])/Qii );
		}
		
		delete[] grad;
		delete[] Dk;
	}

	private:
	Problem* prob;
	double lambda;
	double C;
	vector<SparseVec*>* data ;
	vector<int>* labels ;
	int D; 
	int N;
	int K;
	double* Q_diag;
	double** alpha;
	double* v;

	int max_iter;
	int max_select;
	
	double* grad;
	double* Dk;
};
