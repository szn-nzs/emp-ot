#ifndef OT_IDEAL_H__
#define OT_IDEAL_H__
#include "ot.h"
/** @addtogroup OT
    @{
  */
  
class OTIdeal: public OT<OTIdeal> { public:
	int cnt = 0;
	OTIdeal(NetIO * io): OT(io) {
	}

	void send_impl(const block* data0, const block* data1, int length) {
		cnt+=length;
		io->send_block(data0, length);
		io->send_block(data1, length);
	}

	void recv_impl(block* data, const bool* b, int length) {
		cnt+=length;
		block *data1 = new block[length];
		io->recv_block(data, length);
		io->recv_block(data1, length);
		for(int i = 0; i < length; ++i)
			if(b[i])
				data[i] = data1[i];
		delete []data1;
	}
};
/**@}*/
#endif// OT_IDEAL_H__