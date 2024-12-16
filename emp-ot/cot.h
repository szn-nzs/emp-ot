#ifndef EMP_COT_H__
#define EMP_COT_H__
#include "emp-ot/ot.h"
#include <cstdio>

namespace emp {
// 一个OT协议的消息长度？
const static int64_t ot_bsize = 8;
template<typename T>
class COT : public OT<T>{ 
public:
	T * io = nullptr;
	MITCCRH<ot_bsize> mitccrh;
	block Delta;
	PRG prg;
	virtual void send_cot(block* data0, int64_t length) = 0;
	virtual void recv_cot(block* data, const bool* b, int64_t length) = 0;
	// data0 data1是我们要的m个OT协议里的两组消息，也就是length=m
	void send(const block* data0, const block* data1, int64_t length) override {
		block * data = new block[length];
		// sender先作为一个ot协议中的recv
		// 收到数据data，也就是T^(Delta&r)
		// 一共收到m个block，是相应的m行，每行长度为k
		// 在iknp的实现里默认k=128，即T是一个m行128列的矩阵，每行是一个block
		send_cot(data, length);
		block s;prg.random_block(&s, 1);
		// 哈希key
		io->send_block(&s,1);
		mitccrh.setS(s);
		io->flush();
		block pad[2*ot_bsize];
		for(int64_t i = 0; i < length; i+=ot_bsize) {
			for(int64_t j = i; j < min(i+ot_bsize, length); ++j) {
				// t_i^(Delta&r_i)
				pad[2*(j-i)] = data[j];
				// t_i^(Delta&\bar{r_i})
				pad[2*(j-i)+1] = data[j] ^ Delta;
			}
			mitccrh.hash<ot_bsize, 2>(pad);
			for(int64_t j = i; j < min(i+ot_bsize, length); ++j) {
				// x0^H(T^(Delta&r))
				pad[2*(j-i)] = pad[2*(j-i)] ^ data0[j];
				// x1^H(T^(Delta&\bar{r}))
				pad[2*(j-i)+1] = pad[2*(j-i)+1] ^ data1[j];
			}
			io->send_data(pad, 2*sizeof(block)*min(ot_bsize,length-i));
		}
		delete[] data;
	}

	// data是recv收到的消息，r是选择向量
	void recv(block* data, const bool* r, int64_t length) override {
		// data就是T
		recv_cot(data, r, length);
		block s;
		io->recv_block(&s,1);
		mitccrh.setS(s);
		io->flush();

		block res[2*ot_bsize];
		block pad[ot_bsize];
		for(int64_t i = 0; i < length; i+=ot_bsize) {
			// T的第i行
			memcpy(pad, data+i, min(ot_bsize,length-i)*sizeof(block));
			// 哈希 计算H(t)=H(t)
			// 当r=0时就是H(T^(Delta&r))，当r=1时就是H(T^(Delta&\bar{r}))
			mitccrh.hash<ot_bsize, 1>(pad);
			io->recv_data(res, 2*sizeof(block)*min(ot_bsize,length-i));
			for(int64_t j = 0; j < ot_bsize and j < length-i; ++j) {
				data[i+j] = res[2*j+r[i+j]] ^ pad[j];
			}
		}
	}

	void send_rot(block* data0, block* data1, int64_t length) {
		send_cot(data0, length);
		block s; prg.random_block(&s, 1);
		io->send_block(&s,1);
		mitccrh.setS(s);
		io->flush();

		block pad[ot_bsize*2];
		for(int64_t i = 0; i < length; i+=ot_bsize) {
			for(int64_t j = i; j < min(i+ot_bsize, length); ++j) {
				pad[2*(j-i)] = data0[j];
				pad[2*(j-i)+1] = data0[j] ^ Delta;
			}
			mitccrh.hash<ot_bsize, 2>(pad);
			for(int64_t j = i; j < min(i+ot_bsize, length); ++j) {
				// y0=H(T^(Delta&r))
				data0[j] = pad[2*(j-i)];
				// y1=H(T^(Delta&\bar{r}))
				data1[j] = pad[2*(j-i)+1];
			}
		}
	}

	void recv_rot(block* data, const bool* r, int64_t length) {
		recv_cot(data, r, length);
		block s;
		io->recv_block(&s,1);
		mitccrh.setS(s);
		io->flush();
		block pad[ot_bsize];
		for(int64_t i = 0; i < length; i+=ot_bsize) {
			memcpy(pad, data+i, min(ot_bsize,length-i)*sizeof(block));
			mitccrh.hash<ot_bsize, 1>(pad);

			// y=H(t)
			// 当r=0时，y=H(T^(Delta&r))=y0
			// 当r=1时，y=H(T^(Delta&\bar{r}))=y1
			memcpy(data+i, pad, min(ot_bsize,length-i)*sizeof(block));
		}
	}

	void send_not(const block* data, int64_t maxChoice) {
        int64_t maxChoiceBitLength = std::ceil(log(maxChoice));
        block *userkey0 = new block[maxChoiceBitLength];
        block *userkey1 = new block[maxChoiceBitLength];

	    PRG prg(fix_key);
        prg.random_block(userkey0, maxChoiceBitLength);
	    prg.random_block(userkey1, maxChoiceBitLength);
        send(userkey0, userkey1, maxChoiceBitLength);

        block *res = new block[maxChoice];
        int64_t tmpi;
        AES_KEY aeskey;
        for (int64_t i = 0; i < maxChoice; ++i) {
            tmpi = i;
            res[i]= makeBlock(0, 0);
            for (int64_t j = 0; j < maxChoiceBitLength; ++j) {
                if (tmpi & 1) {
                    res[i] ^= userkey1[j];
                } else {
                    res[i] ^= userkey0[j];
                }
                tmpi >>= 1;
            }
            AES_set_encrypt_key(res[i], &aeskey);

            res[i] = data[i];
            memcpy(res + i, data + i, sizeof(block));
            AES_ecb_encrypt_blks(res + i, 1, &aeskey);
        }
        io->send_data(res, sizeof(block)*maxChoice);

        delete[] userkey0;
        delete[] userkey1;
        delete[] res;
    }

    void recv_not(block* data, const int64_t r, int64_t maxChoice) {
        int64_t maxChoiceBitLength = std::ceil(log(maxChoice));

        int64_t tmpr = r;
        bool *b = new bool[maxChoiceBitLength];
        for (int64_t i = 0; i < maxChoiceBitLength; ++i) {
            b[i] = tmpr & 1;
            tmpr >>= 1;
        }
        recv(data, b, maxChoiceBitLength);

        block userkey = makeBlock(0, 0);
        AES_KEY aeskey;
        for (int64_t i = 0; i < maxChoiceBitLength; ++i) {
            userkey ^= data[i];
        }
        AES_set_decrypt_key(userkey, &aeskey);
        
        block *res = new block[maxChoice];
        io->recv_data(res, sizeof(block)*maxChoice);
        AES_ecb_decrypt_blks(res + r, 1, &aeskey);
        memcpy(data, res + r, sizeof(block));

        delete[] b; 
    }
};
}
#endif
