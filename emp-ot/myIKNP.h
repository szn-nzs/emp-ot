#ifndef EMP_MYIKNP_H__
#define EMP_MYIKNP_H__
#include "emp-ot/cot.h"
#include "emp-ot/co.h"
#include "emp-ot/mybaseOT.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <emp-tool/utils/utils.h>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace emp {
const static int64_t block_size = 1024*2;

class myIKNP {
public: 
	myIKNP() {}
protected:
	// IKNPOptions options;
    PRG prg;
};

class myIKNPSender : public myIKNP {
public:
	myIKNPSender(std::shared_ptr<Group> _G) : ot_receiver(_G, 128) {}
	const block& getDelta() {
		return Delta;
	}
	const vector<block>& getQ() {
		return q;
	}

	void setupSend() {
		// 生成一个选择向量s，长度为128
		prg.random_bool(s, 128);
		Delta = bool_to_block(s);
	}
	const vector<Point>& baseOTMsg1(Point A) {
		return ot_receiver.sendB(A, s);
	}
	void baseOTGetData(Point A, const BaseOT::EType &E) {
		vector<block> data = ot_receiver.getM(A, E, s);
		for(int64_t i = 0; i < 128; ++i) {
			k0[i] = data[i];
			G0[i].reseed(&k0[i]);
		}
	}
	
	void sendPre(const vector<vector<block>>& U, int64_t length) {
		q.clear();
		q.resize(length);

		int64_t j = 0;
		block local_out[block_size];
		
		for (; j < length/block_size; ++j) {
				sendPreBlock(U[j], q.data() + j*block_size, block_size);
		}
		int64_t remain = length % block_size;
		if (remain > 0) {
			sendPreBlock(U[j], local_out, remain);
			memcpy(q.data()+j*block_size, local_out, sizeof(block)*remain);
		}
	}

private:
	bool s[128];
	block k0[128], Delta;
	PRG G0[128];
	vector<block> q;
	BaseOTReceiver ot_receiver;
	
	void sendPreBlock(const vector<block>& U, block * out, int64_t len) {
		block t[block_size];
		memset(t, 0, block_size * sizeof(block));
		int64_t local_block_size = (len+127)/128*128;
		
		for(int64_t i = 0; i < 128; ++i) {
			// G
			G0[i].random_data(t+(i*block_size/128), local_block_size/8);
			if (s[i]) {
				xorBlocks_arr(t+(i*block_size/128), t+(i*block_size/128), U.data()+(i*block_size/128), local_block_size/128);
			}
		}
		sse_trans((uint8_t *)(out), (uint8_t*)t, 128, block_size);
	}
};

class myIKNPReceiver : public myIKNP {
public:
	myIKNPReceiver(std::shared_ptr<Group> _G) : ot_sender(_G, 128) {}
	const vector<block>& getT() {
		return T;
	}

	void setupRecv() {
		prg.random_block(k0, 128);
		prg.random_block(k1, 128);

		for(int64_t i = 0; i < 128; ++i) {
			G0[i].reseed(&k0[i]);
			G1[i].reseed(&k1[i]);
		}
	}
	const Point& baseOTMsg1() {
		return ot_sender.sendA();
	}
	BaseOT::EType baseOTMsg2(vector<Point> B) {
		return ot_sender.sendE(B, k0, k1);
	}

	vector<vector<block>> recvPre(std::span<const bool> r, int64_t length) {
		if (r.size() != length) {
			error("r.size error\n");
		}
		T.clear();
		T.resize(length);
		vector<vector<block>> res;

		block *block_r = new block[(length+127)/128];
		for(int64_t i = 0; i < length/128; ++i)
			block_r[i] = bool_to_block(&r[i*128]);
		if (length%128 != 0) {
			bool tmp_bool_array[128];
			memset(tmp_bool_array, 0, 128);
			int64_t start_point = (length / 128)*128;
			memcpy(tmp_bool_array, &r[start_point], length % 128);
			block_r[length/128] = bool_to_block(tmp_bool_array);
		}
		
		res.reserve(length/block_size + 1);
		int64_t j = 0;
		for (; j < length/block_size; ++j) {
				vector<block> tmp = recvPreBlock(T.data()+j*block_size, std::span(block_r + (j*block_size/128), block_size));
				res.emplace_back(std::move(tmp));
		}
		int64_t remain = length % block_size;
		if (remain > 0) {
			block local_out[block_size];
			vector<block> tmp = recvPreBlock(local_out, std::span(block_r + (j*block_size/128), remain));
			memcpy(T.data()+j*block_size, local_out, sizeof(block)*remain);
			res.emplace_back(std::move(tmp));
		}
		delete[] block_r;

		return res;
	}

private:
	block k0[128], k1[128];
	PRG G0[128], G1[128];
	vector<block> T;
	BaseOTSender ot_sender;

	vector<block> recvPreBlock(block *out, std::span<const block> r) {
		int64_t len = r.size();
		block t[block_size];
		memset(t, 0, block_size*sizeof(block));
		vector<block> tmp;
		tmp.clear();
		tmp.resize(block_size);

		int64_t local_block_size = (len+127)/128 * 128;
		for(int64_t i = 0; i < 128; ++i) {
			G0[i].random_data(t+(i*block_size/128), local_block_size/8);
			G1[i].random_data(tmp.data()+(i*block_size/128), local_block_size/8);
			xorBlocks_arr(tmp.data()+(i*block_size/128), t+(i*block_size/128), tmp.data()+(i*block_size/128), local_block_size/128);
			xorBlocks_arr(tmp.data()+(i*block_size/128), r.data(), tmp.data()+(i*block_size/128), local_block_size/128);
		}

		//转置 
		sse_trans((uint8_t *)(out), (uint8_t*)t, 128, block_size);
		return tmp;
	}
};

}//namespace
#endif
