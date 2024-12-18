#include "emp-ot/myIKNP.h"
#include "emp-ot/myCOT.h"
#include "emp-ot/mybaseOT.h"
#include "test/test.h"
#include <cstdio>
#include <emp-tool/utils/group.h>
#include <memory>
using namespace std;

const static int threads = 1;

int main(int argc, char** argv) {
	int length; // make sure all functions work for non-power-of-two lengths
	// if (argc <= 3)
	// 	length = (1<<20) + 101;
	// else
	// 	length = (1<<atoi(argv[3])) + 101;
	// int maxChoice = 4;

	std::shared_ptr<Group> G = make_shared<Group>();
	BaseOTSender *ot_sender = new BaseOTSender(G, 128);
	BaseOTReceiver *ot_receiver = new BaseOTReceiver(G, 128);
	cout <<"BaseOT:\t"<<test_mybaseOT(ot_sender, ot_receiver, 128)<<" us"<<endl;

	myIKNPSender *iknp_sender = new myIKNPSender(G);
	myIKNPReceiver *iknp_receiver = new myIKNPReceiver(G);
	cout <<"COT:\t"<<test_myIKNP(iknp_sender, iknp_receiver, 1 << 20)<<" us"<<endl;

	myCOTSender *cot_sender = new myCOTSender(G);
	myCOTReceiver *cot_receiver = new myCOTReceiver(G);
	cout <<"NOT:\t"<<test_myCOT(cot_sender, cot_receiver, 1000)<<" us"<<endl;

	myNOTSender *not_sender = new myNOTSender(G, 4, 128);
	myNOTReceiver *not_receiver = new myNOTReceiver(G, 4, 128);
	cout <<"NOT:\t"<<test_myNOT(not_sender, not_receiver, 4, 128)<<" us"<<endl;
}

