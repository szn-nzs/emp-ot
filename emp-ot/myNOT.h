#ifndef EMP_MYNOT_H__
#define EMP_MYNOT_H__
#include "emp-ot/cot.h"
#include "emp-ot/myIKNP.h"
#include "emp-ot/mybaseOT.h"
#include "emp-ot/ot.h"
#include <cstdint>
#include <cstdio>
#include <emp-tool/utils/block.h>
#include <emp-tool/utils/group.h>
#include <emp-tool/utils/utils.h>
#include <span>
#include <vector>

namespace emp {
// const static int64_t ot_bsize = 8;
const static int64_t ot_instance_num = 1000;
class myNOT {
public:
    myNOT(int64_t _maxChoice, int64_t _length) : maxChoice(_maxChoice), length(_length) {}
protected:
    int64_t maxChoice, length;
    PRG prg;
    MITCCRH<ot_bsize> mitccrh;
};
class myNOTSender : public myNOT {
public:
    myNOTSender(std::shared_ptr<Group> _G, int64_t _maxChoice, int64_t _length)
     : myNOT(_maxChoice, _length), iknp_sender(_G), isSetS(false), isSetup(false) {}
    myIKNPSender& getIKNPSender (){
        return iknp_sender;
    }
    const vector<block>& getKey0() {
        return userkey0;
    }
    const vector<block>& getKey1() {
        return userkey1;
    }

    block setS() {
        block s;
        prg.random_block(&s, 1);
		mitccrh.setS(s);

        isSetS = true;
        return s;
    }
    void setupNOT() {
        int64_t maxChoiceBitLength = std::ceil(log(maxChoice));

        userkey0.resize(maxChoiceBitLength);
        userkey1.resize(maxChoiceBitLength);

	    PRG prg(fix_key);
        prg.random_block(userkey0.data(), maxChoiceBitLength);
	    prg.random_block(userkey1.data(), maxChoiceBitLength);

        isSetup = true;
    }
    void setupCOT() {
        iknp_sender.setupSend();
    }
    vector<Point> baseOTMsg1(Point A) {
        return iknp_sender.baseOTMsg1(A);
    }
    void baseOTGetData(Point A, const BaseOT::EType &E) {
        iknp_sender.baseOTGetData(A, E);
    }
    void sendPre(const vector<vector<block>> &U) {
        int64_t maxChoiceBitLength = std::ceil(log(maxChoice));
        iknp_sender.sendPre(U, maxChoiceBitLength);
    }
    // 上面是iknp的部分，生成ot_instance_num个OT实例，其中sender端持有q

    vector<vector<block>> sendCOT() {
        if (!isSetS) {
            error("s not set\n");
        }
        if (!isSetup) {
            error("not setup\n");
        }
        int64_t maxChoiceBitLength = userkey0.size();
    
        // 这里要先执行iknp给q赋值,然后才能getQ
        vector<block> data = iknp_sender.getQ();
        
        vector<vector<block>> pad;
        pad.resize(maxChoiceBitLength / ot_bsize + 1);
		for(int64_t i = 0; i < maxChoiceBitLength; i+=ot_bsize) {
            int64_t cnt = i / ot_bsize;
            pad[cnt].resize(2*ot_bsize);

			for(int64_t j = i; j < min(i+ot_bsize, maxChoiceBitLength); ++j) {
				// t_i^(Delta&r_i)
				pad[cnt][2*(j-i)] = data[j];
				// t_i^(Delta&\bar{r_i})
				pad[cnt][2*(j-i)+1] = data[j] ^ iknp_sender.getDelta();
			}
			mitccrh.hash<ot_bsize, 2>(pad[cnt].data());

			for(int64_t j = i; j < min(i+ot_bsize, maxChoiceBitLength); ++j) {
				// x0^H(T^(Delta&r))
				pad[cnt][2*(j-i)] = pad[cnt][2*(j-i)] ^ userkey0[j];
				// x1^H(T^(Delta&\bar{r}))
				pad[cnt][2*(j-i)+1] = pad[cnt][2*(j-i)+1] ^ userkey1[j];
			}
		}
        
        return pad;
    }

    vector<block> sendNOT(std::span<const block> data) {
        if (!isSetS) {
            error("s not set\n");
        }
        if (data.size() != length * maxChoice) {
            error("data size error\n");
        }
    
        int64_t maxChoiceBitLength = std::ceil(log(maxChoice));
        vector<block> res;
        res.resize(maxChoice*length);
        int64_t tmpi;
        AES_KEY aeskey;
        for (int64_t i = 0; i < maxChoice*length; i += length) {
            tmpi = i / length;
            for (int64_t j = 0; j < length; ++j) {
                res[i+j]= makeBlock(0, 0);
            }
            for (int64_t j = 0; j < maxChoiceBitLength; ++j) {
                if (tmpi & 1) {
                    res[i] ^= userkey1[j];
                } else {
                    res[i] ^= userkey0[j];
                }
                tmpi >>= 1;
            }
            AES_set_encrypt_key(res[i], &aeskey);

            memcpy(res.data() + i, data.data() + i, sizeof(block) * length);
            AES_ecb_encrypt_blks(res.data() + i, length, &aeskey);
        }
        
        return res;
    }

private:
    bool isSetup = false;
    bool isSetS = false;
    vector<block> userkey0, userkey1;
    myIKNPSender iknp_sender;
};
class myNOTReceiver : public myNOT {
public:
    myNOTReceiver(std::shared_ptr<Group> _G, int64_t _maxChoice, int64_t _length)
     : myNOT(_maxChoice, _length), iknp_receiver(_G), isSetS(false), isSetup(false) {}
    myIKNPReceiver& getIKNPReceiver() {
        return iknp_receiver;
    }

    void setupNOT(int64_t r) {
        this->r = r; 
        isSetup = true;
    }
    void setS(block s) {
        mitccrh.setS(s);
        isSetS = true;
    }
    void setupCOT() {
        iknp_receiver.setupRecv();
    }
    Point baseOTMsg1() {
        return iknp_receiver.baseOTMsg1();
    }
    BaseOT::EType baseOTMsg2(vector<Point> B) {
        return iknp_receiver.baseOTMsg2(B);
    }
    vector<vector<block>> recvPre() {
        int64_t tmpr = r;
        int64_t maxChoiceBitLength = std::ceil(log(maxChoice));
        bool *b = new bool[maxChoiceBitLength];
        for (int64_t i = 0; i < maxChoiceBitLength; ++i) {
            b[i] = tmpr & 1;
            tmpr >>= 1;
        }

        return iknp_receiver.recvPre(std::span(b, maxChoiceBitLength), maxChoiceBitLength);
    }
    // 上面是iknp的部分，生成ot_instance_num个OT实例，其中recv端持有T

    vector<block> recvCOT(vector<vector<block>> res) {
        if (!isSetS) {
            error("s not set\n");
        }
        if (!isSetup) {
            error("not setup\n");
        }

        // 这里要先执行iknp给T赋值,然后才能getT
        vector<block> T = iknp_receiver.getT();
        vector<block> data;
        int64_t maxChoiceBitLength = std::ceil(log(maxChoice));
        data.resize(maxChoiceBitLength);

        int64_t tmpr = r;
        bool *b = new bool[maxChoiceBitLength];
        for (int64_t i = 0; i < maxChoiceBitLength; ++i) {
            b[i] = tmpr & 1;
            tmpr >>= 1;
        }

		block pad[ot_bsize];
		for(int64_t i = 0; i < maxChoiceBitLength; i+=ot_bsize) {
            int64_t cnt = i / ot_bsize;
			// T接下来的ot_size行
			memcpy(pad, T.data()+i, min(ot_bsize,maxChoiceBitLength-i)*sizeof(block));
			// 哈希 计算H(t)=H(t)
			// 当r=0时就是H(T^(Delta&r))，当r=1时就是H(T^(Delta&\bar{r}))
			mitccrh.hash<ot_bsize, 1>(pad);
            
			for(int64_t j = 0; j < ot_bsize and j < maxChoiceBitLength-i; ++j) {
				data[i+j] = res[cnt][2*j+b[i+j]] ^ pad[j];
			}
		}

        delete[] b; 
        return data;
    }

    vector<block> recvNOT(vector<block> key, vector<block> res) {
        if (res.size() != maxChoice*length) {
            error("res.size error\n");
        }
        int64_t maxChoiceBitLength = std::ceil(log(maxChoice));
        if (key.size() != maxChoiceBitLength) {
            error("key.size error\n");
        }

        int64_t tmpr = r;
        bool *b = new bool[maxChoiceBitLength];
        for (int64_t i = 0; i < maxChoiceBitLength; ++i) {
            b[i] = tmpr & 1;
            tmpr >>= 1;
        }

        block userkey = makeBlock(0, 0);
        AES_KEY aeskey;
        for (int64_t i = 0; i < maxChoiceBitLength; ++i) {
            userkey ^= key[i];
        }
        AES_set_decrypt_key(userkey, &aeskey);
        
        vector<block> data;
        data.resize(length);
        memcpy(data.data(), res.data() + r*length, length*sizeof(block));
        AES_ecb_decrypt_blks(data.data(), length, &aeskey);

        delete[] b; 
        return data;
    }

private:
    bool isSetup = false;
    bool isSetS = false;
    int64_t r;
    myIKNPReceiver iknp_receiver;
};
}
#endif