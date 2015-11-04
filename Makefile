all: train predict
	
train:
	g++ -fopenmp -std=c++11 -O3 -o multiTrain multiTrain.cpp
predict:
	g++ -fopenmp -std=c++11 -O3 -o multiPred multiPred.cpp
