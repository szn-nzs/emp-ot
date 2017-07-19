#ifndef OT_M_EXTENSION_KOS_H__
#define OT_M_EXTENSION_KOS_H__
#include "ot.h"
#include "co.h"

/** @addtogroup OT
  @{
 */
template<typename IO>
class MOTExtension_KOS: public OTExtension<IO, OTCO, MOTExtension_KOS> { public:
	block *open_data = nullptr;
	bool committing = false;
	char dgst[Hash::DIGEST_SIZE];
	
	using OTExtension<IO, OTCO, MOTExtension_KOS>::send_pre;
	using OTExtension<IO, OTCO, MOTExtension_KOS>::recv_pre;
	using OTExtension<IO, OTCO, MOTExtension_KOS>::block_s;
	using OTExtension<IO, OTCO, MOTExtension_KOS>::io;
	using OTExtension<IO, OTCO, MOTExtension_KOS>::extended_r;
	using OTExtension<IO, OTCO, MOTExtension_KOS>::qT;
	using OTExtension<IO, OTCO, MOTExtension_KOS>::tT;
	using OTExtension<IO, OTCO, MOTExtension_KOS>::prg;
	using OTExtension<IO, OTCO, MOTExtension_KOS>::pi;
	using OTExtension<IO, OTCO, MOTExtension_KOS>::padded_length;
	
	MOTExtension_KOS(IO * io, bool committing = false, int ssp = 40) :
		OTExtension<IO, OTCO, MOTExtension_KOS>(io, ssp) {
		this->committing = committing;
	}

	~MOTExtension_KOS() {
		delete_array_null(open_data);
	}

	bool send_check(int length) {
		if (committing) {
			Hash::hash_once(dgst, &block_s, sizeof(block));
			io->send_data(dgst, Hash::DIGEST_SIZE);
		}

		int extended_length = padded_length(length);
		block seed2, x, t[2], q[2], tmp1, tmp2;
		io->recv_block(&seed2, 1);
		block *chi = new block[extended_length];
		PRG prg2(&seed2);
		prg2.random_block(chi, extended_length);

		q[0] = zero_block();
		q[1] = zero_block();
		for(int i = 0; i < extended_length; ++i) {
			mul128(qT[i], chi[i], &tmp1, &tmp2);
			q[0] = xorBlocks(q[0], tmp1);
			q[1] = xorBlocks(q[1], tmp2);
		}
		io->recv_block(&x, 1);
		io->recv_block(t, 2);

		mul128(x, block_s, &tmp1, &tmp2);
		q[0] = xorBlocks(q[0], tmp1);
		q[1] = xorBlocks(q[1], tmp2);

		delete[] chi;
		return block_cmp(q, t, 2);	
	}
	void recv_check(const bool* r, int length) {
		if (committing) {
			io->recv_data(dgst, Hash::DIGEST_SIZE);
		}

		int extended_length = padded_length(length);
		block *chi = new block[extended_length];
		block seed2, x = zero_block(), t[2], tmp1, tmp2;
		prg.random_block(&seed2,1);
		io->send_block(&seed2, 1);
		PRG prg2(&seed2);
		t[0] = t[1] = zero_block();
		prg2.random_block(chi,extended_length);
		for(int i = 0 ; i < length; ++i) {
			if(r[i])
				x = xorBlocks(x, chi[i]);
		}
		for(int i = 0 ; i < extended_length - length; ++i) {
			if(extended_r[i])
				x = xorBlocks(x, chi[i+length]);
		}

		io->send_block(&x, 1);
		for(int i = 0 ; i < extended_length; ++i) {
			mul128(chi[i], tT[i], &tmp1, &tmp2);
			t[0] = xorBlocks(t[0], tmp1);
			t[1] = xorBlocks(t[1], tmp2);
		}
		io->send_block(t, 2);

		delete[] chi;
	}
	void got_recv_post(block* data, const bool* r, int length) {
		block res[2];
		if(committing) {
			delete_array_null(open_data);
			open_data = new block[length];
			for(int i = 0; i < length; ++i) {
				io->recv_data(res, 2*sizeof(block));
				if(r[i]) {
					data[i] = xorBlocks(res[1], pi.H(tT[i], 2*i+1));
					open_data[i] = res[0];
				} else {
					data[i] = xorBlocks(res[0], pi.H(tT[i], 2*i));
					open_data[i] = res[1];
				}
			}
		} else {
			for(int i = 0; i < length; ++i) {
				io->recv_data(res, 2*sizeof(block));
				if(r[i]) {
					data[i] = xorBlocks(res[1], pi.H(tT[i], 2*i+1));
				} else {
					data[i] = xorBlocks(res[0], pi.H(tT[i], 2*i));
				}
			}
		}
	}

	void send_impl(const block* data0, const block* data1, int length) {
		send_pre(length);
		if(!send_check(length))	error("OT Extension check failed");
		OTExtension<IO, OTCO, MOTExtension_KOS>::got_send_post(data0, data1, length);
	}

	void recv_impl(block* data, const bool* b, int length) {
		recv_pre(b, length);
		recv_check(b, length);
		got_recv_post(data, b, length);
	}

	void send_rot(block * data0, block * data1, int length) {
		send_pre(length);
		if(!send_check(length))
			error("OT Extension check failed");
		OTExtension<IO, OTCO, MOTExtension_KOS>::rot_send_post(data0, data1, length);
	}

	void recv_rot(block* data, const bool* b, int length) {
		recv_pre(b, length);
		recv_check(b, length);
		OTExtension<IO, OTCO, MOTExtension_KOS>::rot_recv_post(data, b, length);
	}


	void open() {
		if (!committing)
			error("Committing not enabled");
		io->send_block(&block_s, 1);		
	}

	void open(block * data, const bool * r, int length) {		
		if (!committing)
			error("Committing not enabled");
		io->recv_block(&block_s, 1);		
		char com_recv[Hash::DIGEST_SIZE];		
		Hash::hash_once(com_recv, &block_s, sizeof(block));		
		if (strncmp(com_recv, dgst, 20)!= 0)
			error("invalid commitment");

		for(int i = 0; i < length; ++i) {	
			tT[i] = xorBlocks(tT[i], block_s);
			if(r[i])
				data[i] = xorBlocks(open_data[i], pi.H(tT[i], 2*i));
			else	
				data[i] = xorBlocks(open_data[i], pi.H(tT[i], 2*i+1));
		}		
	}
};

/**@}*/
#endif// OT_M_EXTENSION_KOS_H__
