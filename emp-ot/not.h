#ifndef EMP_NOT_H__
#define EMP_NOT_H__
#include "emp-ot/cot.h"
#include "emp-ot/iknp.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <emp-tool/utils/aes.h>
#include <emp-tool/utils/block.h>

namespace emp {

template<typename T>
class NOT {
	T * io = nullptr;
	MITCCRH<ot_bsize> mitccrh;
    IKNP<T> *cot = nullptr;
    
public:
    NOT(T *io) {
        this->io = io;
        this->cot = new IKNP<T>(io);
    }
    ~NOT() = default;

    void send(const block* data, int64_t maxChoice) {
        int64_t maxChoiceBitLength = std::ceil(log(maxChoice));
        block *userkey0 = new block[maxChoiceBitLength];
        block *userkey1 = new block[maxChoiceBitLength];

	    PRG prg(fix_key);
        prg.random_block(userkey0, maxChoiceBitLength);
	    prg.random_block(userkey1, maxChoiceBitLength);
        cot->send(userkey0, userkey1, maxChoiceBitLength);

        block *res = new block[maxChoice];
        int64_t tmpi;
        AES_KEY *aeskey;
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
            AES_set_encrypt_key(res[i], aeskey);

            res[i] = data[i];
            memcpy(res + i, data + i, sizeof(block));
            AES_ecb_encrypt_blks(res + i, 1, aeskey);
        }
        io->flush();
        io->send_data(res, sizeof(block)*maxChoice);

        delete[] userkey0;
        delete[] userkey1;
        delete[] res;
        delete aeskey;
    }

    void recv(block* data, const int64_t r, int64_t maxChoice) {
        int64_t maxChoiceBitLength = std::ceil(log(maxChoice));

        int64_t tmpr = r;
        bool *b = new bool[maxChoiceBitLength];
        for (int64_t i = 0; i < maxChoiceBitLength; ++i) {
            b[i] = tmpr & 1;
            tmpr >>= 1;
        }
        cot->recv(data, b, maxChoiceBitLength);

        block userkey = makeBlock(0, 0);
        AES_KEY *aeskey;
        for (int64_t i = 0; i < maxChoiceBitLength; ++i) {
            userkey ^= data[i];
        }
        AES_set_decrypt_key(userkey, aeskey);
        
        block *res = new block[maxChoice];
        io->recv_data(res, sizeof(block)*maxChoice);
        io->flush();
        AES_ecb_decrypt_blks(res + r, 1, aeskey);
        memcpy(data, res + r, sizeof(block));

        delete[] b; 
    }
};
}

#endif