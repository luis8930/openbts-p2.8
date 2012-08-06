/* written by Dmitri Soloviev <dmi3sol@gmail.com> for Fairwaves, Inc
 */

#include <stdio.h>
#include <stdlib.h>
#include <list>

#include "ControlCommon.h"
#include "TransactionTable.h"
#include "HandoverControl.h"

using namespace std;
using namespace GSM;
using namespace Control;




void HandoverEntry::HandoverEntry(const TransactionEntry& wTransaction, const GSM::LogicalChannel wTCH, unsigned wHandoverReference){
	mTransaction(wTransaction); mTCH(wTCH); mHandoverReference(wHandoverReference);
	
	gTRX.ARFCN()->handoverOn(mTCH->TN(),mHandoverReference);
	mGotHA = mGotHComplete = mRegisterPerformed = false;
	
	// HO Ref, Cell Id, ChannelID - all the donor site needs to know now
	mTransaction->HOCSendHandhoverAck(mHandoverReference, 
		gConfig.getNum("GSM.Identity.BSIC.BCC"),
		gConfig.getNum("GSM.Identity.BSIC.NCC"),
		gConfig.getNum("GSM.Radio.C0"),
		TCH->channelDescription());
	
	mNy1 = gConfig.getNum("GSM.Handover.Ny1");
	mT3103 = Z100Timer(gConfig.getNum("GSM.Handover.T3103"));
	mT3103.set();	// Limit transaction lifetime
	
	gHandover.addHandover(this);
}




void HandoverEntry::HandoverAccessDetected(const GSM::L3PhysicalInformation wPhysicalInformation){
	mPhysicalInformation(wPhysicalInformation);
	
	gTRX.ARFCN()->handoverOff(mTCH->TN());
	mGotHA = true;
	
	mPhysicalInfoAttempts = 0;
	T3105Tick();	// just to accelerate a process
}




bool HandoverEntry::T3105Tick(){
	if(mGotHA){
		// FIXME it seems to be nonsense - I'll check it later
		GSM::TCHFACCHLogicalChannel * facch = gBTS.getTCHByTN(mTCH->TN());
		facch->send(mPhysicalInformation,UNIT_DATA,0);
	
		mPhysicalInfoAttempts++;
		return true;
	}
	return false;
}




void HandoverEntry::HandoverCompleteDetected(){
	mGotHA = false;
	mGotHComplete = true;
	mT3103.stop();
}




bool HandoverEntry::SipRegister(){
	char *IMSI;
	IMSI = mTransaction->subscriber(); //HOCgetIMSI();
	
	if(mGotHComplete){
		LOG(INFO) << "performing Sip Register after handover for " << mHandoverReference;
		try {
			SIPEngine engine(gConfig.getStr("SIP.Proxy.Registration").c_str(),IMSI);
			LOG(DEBUG) << "handover: waiting for registration of " << IMSI << " on " << gConfig.getStr("SIP.Proxy.Registration");
			// FIXME is there any reason to check result: extra t
			mRegisterPerformed = engine.Register(SIPEngine::SIPRegister); 
		}
		catch(SIPTimeout) {
			LOG(ALERT) "SIP registration timed out (handover), proxy is " << gConfig.getStr("SIP.Proxy.Registration");
		}
		return true;
	}
	return false;
}




bool HandoverEntry::removeHandoverEntry(){
	if(mGotHA)
		if(mPhysicalInfoAttempts >= mNy1) {
			// FIXME is Ny1 expiration a reason to kill handover
			// might wait till T3103 expires
			gTRX.ARFCN()->handoverOff(mTCH->TN());
			mTransaction->HOCTimeout();
			return true;
		}
	if(mRegisterPerformed) {
		gTRX.ARFCN()->handoverOff(mTCH->TN());	// this will spoil nothing..
		return true;
	}
	if(mT3103.expired() && (!mGotHComplete)){
		gTRX.ARFCN()->handoverOff(mTCH->TN());
		mTransaction->HOCTimeout();
			return true;
	}
	
	return false;
}




void Handover::start()
{
	if (mRunning) return;
	mRunning=true;
	mHandoverThread.start((void* (*)(void*))HandoverServiceLoop, (void*)this);
}




void Handover::addHandover(HandoverEntry he){ 
	LOG(INFO) << "adding Handover with " << he->HandoverReference() <<" to processing thread";
	ScopedLock lock(mLock);
	// add an instance
	mHandovers.put(he);
	// wake up a thread
	mHandoverSignal.signal();
}




void Handover::handoverHandler(){
	bool delaySipRegister = false;
	while(mRunning){

		mLock.lock();
		while (mHandovers.size()==0) mHandoverSignal.wait(mLock);
		mLock.unlock();
		
		HandoverEntryList::iterator lp = mHandovers.begin();
		while (lp != mHandovers.end()) {
			LOG(INFO) << "inscpecting Handover " << lp->HandoverReference();
			if(lp->removeHandover()) {
				LOG(INFO) << "Handover with " << lp->HandoverReference() <<" needs to be removed";
				lp = mHandovers.erase(lp);
			}
			else lp++;
		}

		lp = mHandovers.begin();
		while (lp != mHandovers.end()) {
			LOG(INFO) << "processing Handover " << lp->HandoverReference();
			delaySipRegister |= lp->T3105Tick();
			lp++;
		}
		
		if(delaySipRegister()) {
			usleep(mT3105);
			continue;
		}
		
		// SIP activities pauses a thread, so 
		// --they can be performed when 
		//   no on-line handover activities are required.
		// -- one registration procedure per cycle is enough
		
		lp = mHandovers.begin();
		while (lp != mHandovers.end()) {
			LOG(INFO) << "checking whether Register after Handover needed " << lp->HandoverReference();
			if(lp->SipRegister()) {
				LOG(INFO) << "Sip-Registered after Handover " << lp->HandoverReference();
				lp = mHandovers.erase(lp);
				break;
			}
			lp++;
		}		
	}
}




void* Control::HandoverServiceLoop(Handover * handover)
{
	handover->handoverHandler();
	return NULL;
}
