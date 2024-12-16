#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <emp-tool/emp-tool.h>
#include "emp-ot/emp-ot.h"
#include "emp-ot/mybaseOT.h"
#include <emp-tool/utils/block.h>
#include <emp-tool/utils/group.h>
#include <iostream>
#include <span>
#include <vector>
using namespace emp;

double test_mybaseOT(BaseOTSender *otsender, BaseOTReceiver *otreceiver, int64_t length) {
	PRG prg(fix_key);
	PRG prg2;

	block *data0 = new block[length];
	block *data1 = new block[length];
	bool *r = new bool[length];
	prg.random_block(data0, length);
	prg.random_block(data1, length);
	prg2.random_bool(r, length);
	// std::span<block> b0(data0, length);
	
	auto start = clock_start();
	Point A = otsender->sendA();
	std::vector<Point> B = otreceiver->sendB(A, std::span(r, length));
	BaseOTSender::EType E = otsender->sendE(B, std::span(data0, length), std::span(data1, length));
	std::vector<block> data = otreceiver->getM(A, E, std::span(r, length));
	long long t = time_from(start);
	
	for (int64_t i = 0; i < length; ++i) {
		if (r[i]){ 
			if(!cmpBlock(&data[i], &data1[i], 1)) {
				std::cout <<i<<"\n";
				error("wrong!\n");
			}
		}
		else { 
			if(!cmpBlock(&data[i], &data0[i], 1)) {
				std::cout <<i<<"\n";
				error("wrong!\n");
			}
		}
	}
	std::cout << "Tests passed.\t";

	delete []data0;
	delete []data1;
	delete []r;
	return t;
}

double test_myIKNP(myIKNPSender* sender, myIKNPReceiver* receiver, int64_t length) {
	PRG prg;

	// block *data0 = new block[length];
	bool *r = new bool[length];
	block delta;
	// prg.random_block(data0, length);
	prg.random_bool(r, length);

	auto start = clock_start();
	sender->setupSend();
	receiver->setupRecv();
	Point A = receiver->baseOTMsg1();
	std::vector<Point> B = sender->baseOTMsg1(A);
	BaseOT::EType E = receiver->baseOTMsg2(B);
	sender->baseOTGetData(A, E);

	vector<vector<block>> U = receiver->recvPre(std::span(r, length), length);
	sender->sendPre(U, length);

	long long t = time_from(start);
	delta = sender->getDelta();
	for (int64_t i = 0; i < length; ++i) {
		block data0 = receiver->getT()[i];
		block data1 = data0 ^ delta;
		if (r[i]) {
			if (!cmpBlock(&sender->getQ()[i], &data1, 1)) {
				print128_num(sender->getQ()[i]);
				print128_num(data1);
				print128_num(data0);
				printf("111pos: %d\n", i);
				error("COT failed!");
			}
		} else {
			if (!cmpBlock(&sender->getQ()[i], &data0, 1)) {
				print128_num(sender->getQ()[i]);
				print128_num(data1);
				print128_num(data0);
				printf("222pos: %d\n", i);
				error("COT failed!");
			}
		}
	}
	printf("Tests passed.\t");
	delete[] r;

	return t;
}

double test_myNOT(myNOTSender *sender, myNOTReceiver *receiver, int64_t length) {
	PRG prg(fix_key);
	PRG prg2;

	block *data0 = new block[length];
	block *data1 = new block[length];
	bool *r = new bool[length];
	prg.random_block(data0, length);
	prg.random_block(data1, length);
	prg2.random_bool(r, length);
	// std::span<block> b0(data0, length);
	
	auto start = clock_start();
	receiver->setS(sender->setS());
	
	sender->getIKNPSender().setupSend();
	receiver->getIKNPReceiver().setupRecv();
	Point A = receiver->getIKNPReceiver().baseOTMsg1();
	std::vector<Point> B = sender->getIKNPSender().baseOTMsg1(A);
	BaseOT::EType E = receiver->getIKNPReceiver().baseOTMsg2(B);
	sender->getIKNPSender().baseOTGetData(A, E);

	vector<vector<block>> U = receiver->getIKNPReceiver().recvPre(std::span(r, length), length);
	sender->getIKNPSender().sendPre(U, length);

	vector<vector<block>> pad = sender->send(std::span(data0, length), std::span(data1, length), length);
	vector<block> data = receiver->recv(pad, std::span(r, length), length);

	long long t = time_from(start);
	
	for (int64_t i = 0; i < length; ++i) {
		if (r[i]){ 
			if(!cmpBlock(&data[i], &data1[i], 1)) {
				print128_num(data[i]);
				print128_num(data1[i]);
				std::cout <<i<<"\n";
				error("wrong!\n");
			}
		}
		else { 
			if(!cmpBlock(&data[i], &data0[i], 1)) {
				print128_num(data[i]);
				print128_num(data0[i]);
				std::cout <<i<<"\n";
				error("wrong!\n");
			}
		}
	}
	std::cout << "Tests passed.\t";

	delete []data0;
	delete []data1;
	delete []r;
	return t;
}

template <typename T>
double test_ot(T * ot, NetIO *io, int party, int64_t length) {
	block *b0 = new block[length], *b1 = new block[length],
	*r = new block[length];
	PRG prg(fix_key);
	prg.random_block(b0, length);
	prg.random_block(b1, length);
	bool *b = new bool[length];
	PRG prg2;
	prg2.random_bool(b, length);

	auto start = clock_start();
	if (party == ALICE) {
		ot->send(b0, b1, length);
	} else {
		ot->recv(r, b, length);
	}
	io->flush();
	long long t = time_from(start);
	if (party == BOB) {
		for (int64_t i = 0; i < length; ++i) {
			if (b[i]){ if(!cmpBlock(&r[i], &b1[i], 1)) {
				std::cout <<i<<"\n";
				error("wrong!\n");
			}}
			else { if(!cmpBlock(&r[i], &b0[i], 1)) {
				std::cout <<i<<"\n";
				error("wrong!\n");
			}}
		}
	}
	std::cout << "Tests passed.\t";
	delete[] b0;
	delete[] b1;
	delete[] r;
	delete[] b;
	return t;
}

template <typename T>
double test_cot(T * ot, NetIO *io, int party, int64_t length) {
	block *b0 = new block[length], *r = new block[length];
	bool *b = new bool[length];
	block delta;
	PRG prg;
	prg.random_block(&delta, 1);
	prg.random_bool(b, length);

	io->sync();
	auto start = clock_start();
	if (party == ALICE) {
		ot->send_cot(b0, length);
		delta = ot->Delta;
	} else {
		ot->recv_cot(r, b, length);
	}
	io->flush();
	long long t = time_from(start);
	if (party == ALICE) {
		io->send_block(&delta, 1);
		io->send_block(b0, length);
	}
	else if (party == BOB) {
		io->recv_block(&delta, 1);
		io->recv_block(b0, length);
		for (int64_t i = 0; i < length; ++i) {
			block b1 = b0[i] ^ delta;
			if (b[i]) {
				if (!cmpBlock(&r[i], &b1, 1))
					error("COT failed!");
			} else {
				if (!cmpBlock(&r[i], &b0[i], 1))
					error("COT failed!");
			}
		}
	}
	std::cout << "Tests passed.\t";
	io->flush();
	delete[] b0;
	delete[] r;
	delete[] b;
	return t;
}

template <typename T>
double test_rot(T* ot, NetIO *io, int party, int64_t length) {
	block *b0 = new block[length], *r = new block[length];
	block *b1 = new block[length];
	bool *b = new bool[length];
	PRG prg;
	prg.random_bool(b, length);

	io->sync();
	auto start = clock_start();
	if (party == ALICE) {
		ot->send_rot(b0, b1, length);
	} else {
		ot->recv_rot(r, b, length);
	}
	io->flush();
	long long t = time_from(start);
	if (party == ALICE) {
		io->send_block(b0, length);
		io->send_block(b1, length);
	} else if (party == BOB) {
		io->recv_block(b0, length);
		io->recv_block(b1, length);
		for (int64_t i = 0; i < length; ++i) {
			if (b[i])
				assert(cmpBlock(&r[i], &b1[i], 1));
			else
				assert(cmpBlock(&r[i], &b0[i], 1));
		}
	}
	std::cout << "Tests passed.\t";
	io->flush();
	delete[] b0;
	delete[] b1;
	delete[] r;
	delete[] b;
	return t;
}

template <typename T>
double test_rcot(T* ot, NetIO *io, int party, int64_t length, bool inplace) {
	block *b = nullptr;
	PRG prg;

	io->sync();
	auto start = clock_start();
	int64_t mem_size;
	if(!inplace) {
		mem_size = length;
		b = new block[length];

		// The RCOTs will be generated in the internal buffer
		// then be copied to the user buffer
		ot->rcot(b, length);
	} else {
		// Call byte_memory_need_inplace() to get the buffer size needed
		mem_size = ot->byte_memory_need_inplace((uint64_t)length);
		b = new block[mem_size];

		// The RCOTs will be generated directly to this buffer
		ot->rcot_inplace(b, mem_size);
	}
	long long t = time_from(start);
	io->sync();
	if (party == ALICE) {
		io->send_block(&ot->Delta, 1);
		io->send_block(b, mem_size);
	}
	else if (party == BOB) {
		block ch[2];
		ch[0] = zero_block;
		block *b0 = new block[mem_size];
		io->recv_block(ch+1, 1);
		io->recv_block(b0, mem_size);
		for (int64_t i = 0; i < mem_size; ++i) {
			b[i] = b[i] ^ ch[getLSB(b[i])];
		}
		if (!cmpBlock(b, b0, mem_size))
			error("RCOT failed");
		delete[] b0;
	}
	std::cout << "Tests passed.\t";
	delete[] b;
	return t;
}


// template <typename T>
// double test_not(T * ot, NetIO *io, int party, int64_t maxChoice) {
// 	block *b = new block[maxChoice], *r = new block[1];
// 	PRG prg(fix_key);
// 	prg.random_block(b, maxChoice);
// 	PRG prg2;
// 	int64_t s;
// 	prg2.random_data(&s, sizeof(s));
// 	s %= maxChoice;

// 	// io->sync();
// 	auto start = clock_start();
// 	if (party == ALICE) {
// 		ot->send_not(b, maxChoice);
// 	} else {
// 		ot->recv_not(r, s, maxChoice);
// 	}
// 	io->flush();
// 	long long t = time_from(start);
// 	if (party == BOB) {
// 		printf("bob\n");
// 		if (!cmpBlock(&b[s], r, 1)) {
// 			error("wrong!\n");
// 		}
// 	}
// 	std::cout << "Tests passed.\t";
// 	delete[] r;
// 	delete[] b;
// 	return t;
// }
