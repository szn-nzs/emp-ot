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

class myNOT {
protected:
    PRG prg;
    MITCCRH<ot_bsize> mitccrh;
};
class myNOTSender : public myNOT {
public:
    myNOTSender(std::shared_ptr<Group> _G) : iknp_sender(_G), isSetS(false) {}
    myIKNPSender& getIKNPSender (){
        return iknp_sender;
    }

    block setS() {
        block s;
        prg.random_block(&s, 1);
		mitccrh.setS(s);

        isSetS = true;
        return s;
    }
    vector<vector<block>> send(std::span<const block> data0, std::span<const block> data1, int64_t length) {
        if (!isSetS) {
            error("s not set\n");
        }
        if (data0.size() != length || data1.size() != length) {
            error("data size error\n");
        }
    
        // 这里要先执行iknp给q赋值,然后才能getQ
        vector<block> data = iknp_sender.getQ();
        
        vector<vector<block>> pad;
        pad.resize(length / ot_bsize + 1);
		for(int64_t i = 0; i < length; i+=ot_bsize) {
            int64_t cnt = i / ot_bsize;
            pad[cnt].resize(2*ot_bsize);

			for(int64_t j = i; j < min(i+ot_bsize, length); ++j) {
				// t_i^(Delta&r_i)
				pad[cnt][2*(j-i)] = data[j];
				// t_i^(Delta&\bar{r_i})
				pad[cnt][2*(j-i)+1] = data[j] ^ iknp_sender.getDelta();
			}
			mitccrh.hash<ot_bsize, 2>(pad[cnt].data());

			for(int64_t j = i; j < min(i+ot_bsize, length); ++j) {
				// x0^H(T^(Delta&r))
				pad[cnt][2*(j-i)] = pad[cnt][2*(j-i)] ^ data0[j];
				// x1^H(T^(Delta&\bar{r}))
				pad[cnt][2*(j-i)+1] = pad[cnt][2*(j-i)+1] ^ data1[j];
			}
		}
        
        return pad;
    }

private:
    bool isSetS = false;
    myIKNPSender iknp_sender;
};
class myNOTReceiver : public myNOT {
public:
    myNOTReceiver(std::shared_ptr<Group> _G) : iknp_receiver(_G), isSetS(false) {}
    myIKNPReceiver& getIKNPReceiver() {
        return iknp_receiver;
    }

    void setS(block s) {
        mitccrh.setS(s);
        isSetS = true;
    }
    vector<block> recv(vector<vector<block>> res, std::span<const bool> r, int64_t length) {
        if (!isSetS) {
            error("s not set\n");
        }
        if (r.size() != length) {
            error("r.size error\n");
        }

        // 这里要先执行iknp给T赋值,然后才能getT
        vector<block> T = iknp_receiver.getT();
        vector<block> data;
        data.resize(length);

		block pad[ot_bsize];
		for(int64_t i = 0; i < length; i+=ot_bsize) {
            int64_t cnt = i / ot_bsize;
			// T接下来的ot_size行
			memcpy(pad, T.data()+i, min(ot_bsize,length-i)*sizeof(block));
			// 哈希 计算H(t)=H(t)
			// 当r=0时就是H(T^(Delta&r))，当r=1时就是H(T^(Delta&\bar{r}))
			mitccrh.hash<ot_bsize, 1>(pad);
            
			for(int64_t j = 0; j < ot_bsize and j < length-i; ++j) {
				data[i+j] = res[cnt][2*j+r[i+j]] ^ pad[j];
			}
		}
        return data;
    }

private:
    bool isSetS = false;
    myIKNPReceiver iknp_receiver;
};
}
#endif