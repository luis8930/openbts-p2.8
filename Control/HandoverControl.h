/* written by Dmitri Soloviev <dmi3sol@gmail.com> for Fairwaves, Inc
 */
#include <list>

namespace GSM{
class LogicalChannel;
class L3PhysicalInformation;
}

namespace Control{

class TransactionEntry;

/** handover entry, placed when handover is requested from outside and removed when handoved complete message got at DCCH*/
class HandoverEntry{
	private:
	
	
	GSM::L3PhysicalInformation mPhysicalInformation;
	
	/** number of attempts to check against Ny1*/
	unsigned mPhysicalInfoAttempts;
	
	unsigned mNy1;
	
	/** traffic channel allocated to accept handover */
	GSM::TCH mTCH;
	
	/** ether waiting for Handover Access or sending PhysicalInformation */
	bool mGotHA;
	
	/** SIP Register needed */
	bool mGotHComplete;
	
	bool mRegisterPerformed;
	
	/** what handover reference was assigned for this attempt */
	unsigned mHandoverReference;
	
	/** in this content acts as an attempt timeout*/
	Z100Timer mT3103;
	
	/** transaction stores SIP session, mobileID etc*/
	TransactionEntry mTransaction;
	
	
	
	public:
	
	unsigned handoverReference() const { return mHandoverReference; }
	
	/** acknowledge via SIP, set timeout, change convolution in Transceiver */
	HandoverEntry(const TransactionEntry& wTransaction, const GSM::LogicalChannel wTCH, unsigned wHandoverReference);
	
	/** must be called from
	 * TCHFACCHL1Decoder::processBurst( const RxBurst& inBurst)
	 * file GSML1FEC.cpp
	 * 
	 * turn RACH decoding off, start sending Physical Info */
	void HandoverAccessDetected(const GSM::L3PhysicalInformation wPhysicalInformation);
	
	/** called from gHandover thread;
	 * returns TRUE if Physical Information sent */
	bool T3105Tick();
	
	/** must be called from
	 * file DCCHDispatch.cpp
	 * stop sending Physical Info, T3103 timer, try to perform SIP Register */
	void HandoverCompleteDetected();
	
	/** true if performed (or attempted) */
	bool SipRegister();
	
	bool removeHandoverEntry();
};


typedef std::list<HandoverEntry> HandoverEntryList;


class Handover{
	public:
	
	Handover()
		:mRunning(false), mT3105(gConfig.getNum("GSM.Handover.T3105"))
	{}
	
	void start();


	private:

	HandoverEntryList mHandovers;				///< List of ID's to be paged.
	mutable Mutex mLock;					///< Lock for thread-safe access.
	Signal mHandoverSignal;						///< signal to wake the paging loop
	Thread mHandoverThread;					///< Thread for the paging loop.
	volatile bool mRunning;
	
	/** !! Attention: in usec needed here */
	unsigned mT3105;	
	
	friend void *HandoverServiceLoop(Handover *);
	
	void handoverHandler();
	
	void addHandover(HandoverEntry *he);
};

void *HandoverServiceLoop(Handover *);
}