#ifndef EMP_MYBASEOT_H__
#define EMP_MYBASEOT_H__
#include "emp-ot/ot.h"
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <emp-tool/emp-tool.h>
#include <emp-tool/utils/block.h>
#include <emp-tool/utils/group.h>
#include <emp-tool/utils/utils.h>
#include <memory>
#include <vector>
#include <span>
namespace emp {
void print128_num(__m128i var)
{
    uint16_t val[8];
    memcpy(val, &var, sizeof(val));
    printf("Numerical: %i %i %i %i %i %i %i %i \n", 
           val[0], val[1], val[2], val[3], val[4], val[5], 
           val[6], val[7]);
}

class BaseOT {
public:
	using EType = vector<vector<block>>;
	BaseOT(std::shared_ptr<Group> _G, int64_t _length) : length(_length) {
        if (_G == nullptr) {
            error("_G cannot be nullptr");
        }
		G = std::move(_G);
	}
	~BaseOT() {}
	
protected:
	std::shared_ptr<Group> G = nullptr;
	int64_t length;
};

class BaseOTSender : public BaseOT {
public:
    BaseOTSender(std::shared_ptr<Group> _G, int64_t _length)
	 : BaseOT(std::move(_G), _length) {}
    ~BaseOTSender() = default;

    const Point& sendA() {
		G->get_rand_bn(a);
		A = G->mul_gen(a);

        return A;
    }

	EType sendE(vector<Point> B, std::span<const block> data0, std::span<const block> data1) {
		Point AaInv;
		AaInv = A.mul(a);
		AaInv = AaInv.inv();

		if (B.size() != length) {
			error("B.size error\n");
		}
		if (data0.size() != length || data1.size() != length) {
			error("data.size errot\n");
		}
		vector<Point> BA(length);
		vector<Point> BAaInv(length);
		for (int64_t i = 0; i < length; ++i) {
			BA[i] = B[i].mul(a);
			BAaInv[i] = BA[i].add(AaInv);
		}
		
		EType res;
		res.reserve(length);
		for(int64_t i = 0; i < length; ++i) {
			vector<block> tmp(2);

			tmp[0] = Hash::KDF(BA[i], i) ^ data0[i];
			tmp[1] = Hash::KDF(BAaInv[i], i) ^  data1[i];
			res.emplace_back(std::move(tmp));
		}

		return res;
	}

private:
	BigInt a;
	Point A;
};

class BaseOTReceiver : public BaseOT {
public:
    BaseOTReceiver(std::shared_ptr<Group> _G, int64_t _length)
	 : BaseOT(std::move(_G), _length) {
		B.resize(length);
		bb.resize(length);
	 }
    ~BaseOTReceiver() = default;


	const vector<Point>& sendB(Point A, std::span<const bool> b) {
		B.clear();
		bb.clear();
		B.resize(length);
		bb.resize(length);

		for(int64_t i = 0; i < length; ++i)
			G->get_rand_bn(bb[i]);
		for(int64_t i = 0; i < length; ++i) {
			B[i] = G->mul_gen(bb[i]);
			if (b[i]) 
				B[i] = B[i].add(A);
		}

		return B;
	}

	vector<block> getM(Point A, const EType& E, std::span<const bool> b) {
		vector<Point> As(length);
		vector<block> data(length);
		for(int64_t i = 0; i < length; ++i) {
			As[i] = A.mul(bb[i]);
		}

		if (E.size() != length) {
			error("E.size error\n");
		}
		if (b.size() != length) {
			error("b.size error\n");
		}
		for(int64_t i = 0; i < length; ++i) {
			assert(E[i].size() == 2);
			data[i] = Hash::KDF(As[i], i);

			if(b[i])
				data[i] = data[i] ^ E[i][1];
			else
				data[i] = data[i] ^ E[i][0];
		}

		return data;
	}

	// void endToEndOT() {
	// 	block *r = new block[length];

	// 	auto start = clock_start();
	// 	Point A = sender.sendA();
	// 	std::vector<Point> B = sendB(A, b);
	// 	BaseOTSender::EType E = sender.sendE(B, params.b0, params.b1);
	// 	getM(A, E, r, b);
	// 	long long t = time_from(start);
		
	// 	for (int64_t i = 0; i < length; ++i) {
	// 		if (b[i]){ 
	// 			if(!cmpBlock(&r[i], &params.b1[i], 1)) {
	// 				std::cout <<i<<"\n";
	// 				error("wrong!\n");
	// 			}
	// 		}
	// 		else { 
	// 			if(!cmpBlock(&r[i], &params.b0[i], 1)) {
	// 				std::cout <<i<<"\n";
	// 				error("wrong!\n");
	// 			}
	// 		}
	// 	}
	// 	std::cout << "Tests passed.\t";
	// 	delete[] r;
	// }
    
private:
	vector<BigInt> bb;
	vector<Point> B;
};
}
#endif