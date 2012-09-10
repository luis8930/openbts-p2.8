/**@file Declarations for TransactionTable and related classes. */
/*
* Copyright 2008-2011 Free Software Foundation, Inc.
* Copyright 2012 Jet Wong <icptster@gmail.com>
* Copyright 2012 Fairwaves LLC, Dmitri Soloviev <dmi3sol@gmail.com>
*
* This use of this software may be subject to additional restrictions.
* See the LEGAL file in the main directory for details.

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU Affero General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Affero General Public License for more details.

	You should have received a copy of the GNU Affero General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/



#ifndef TRANSACTIONTABLE_H
#define TRANSACTIONTABLE_H


#include <stdio.h>
#include <list>

#include <Logger.h>
#include <Interthread.h>
#include <Timeval.h>


#include <GSML3CommonElements.h>
#include <GSML3MMElements.h>
#include <GSML3CCElements.h>
#include <GSML3RRElements.h>
#include <SIPEngine.h>



struct sqlite3;


/**@namespace Control This namepace is for use by the control layer. */
namespace Control {

class HandoverEntry;

typedef std::map<std::string, GSM::Z100Timer> TimerTable;




/**
	A TransactionEntry object is used to maintain the state of a transaction
	as it moves from channel to channel.
	The object itself is not thread safe.
*/
class TransactionEntry {

	private:

	mutable Mutex mLock;					///< thread-safe control, shared from gTransactionTable

	/**@name Stable variables, fixed in the constructor or written only once. */
	//@{
	unsigned mID;							///< the internal transaction ID, assigned by a TransactionTable

	GSM::L3MobileIdentity mSubscriber;		///< some kind of subscriber ID, preferably IMSI
	GSM::L3CMServiceType mService;			///< the associated service type
	unsigned mL3TI;							///< the L3 short transaction ID, the version we *send* to the MS

	GSM::L3CalledPartyBCDNumber mCalled;	///< the associated called party number, if known
	GSM::L3CallingPartyBCDNumber mCalling;	///< the associated calling party number, if known

	// TODO -- This should be expaned to deal with long messages.
	//char mMessage[522];						///< text messaging payload
	std::string mMessage;					///< text message payload
	std::string mContentType;				///< text message payload content type
	//@}

	SIP::SIPEngine mSIP;					///< the SIP IETF RFC-3621 protocol engine
	mutable SIP::SIPState mPrevSIPState;	///< previous SIP state, prior to most recent transactions
	GSM::CallState mGSMState;				///< the GSM/ISDN/Q.931 call state
	Timeval mStateTimer;					///< timestamp of last state change.
	TimerTable mTimers;						///< table of Z100-type state timers

	unsigned mNumSQLTries;					///< number of SQL tries for DB operations

	GSM::LogicalChannel *mChannel;			///< current channel of the transaction

	bool mTerminationRequested;
	
	bool mProxyTransaction;
	
	TransactionEntry *mOldTransaction;	// a link to original call, used for outgoing handovers

	public:

	/** This form is used for MTC or MT-SMS with TI generated by the network. */
	TransactionEntry(const char* proxy,
		const GSM::L3MobileIdentity& wSubscriber, 
		GSM::LogicalChannel* wChannel,
		const GSM::L3CMServiceType& wService,
		const GSM::L3CallingPartyBCDNumber& wCalling,
		GSM::CallState wState = GSM::NullState,
		const char *wMessage = NULL);

	/** This form is used for MOC, setting mGSMState to MOCInitiated. */
	TransactionEntry(const char* proxy,
		const GSM::L3MobileIdentity& wSubscriber,
		GSM::LogicalChannel* wChannel,
		const GSM::L3CMServiceType& wService,
		unsigned wL3TI,
		const GSM::L3CalledPartyBCDNumber& wCalled);

	/** This form is used for SOS calls, setting mGSMState to MOCInitiated. */
	TransactionEntry(const char* proxy,
		const GSM::L3MobileIdentity& wSubscriber,
		GSM::LogicalChannel* wChannel,
		const GSM::L3CMServiceType& wService,
		unsigned wL3TI);

	/** Form for MO-SMS; sets yet-unknown TI to 7 and GSM state to SMSSubmitting */
	TransactionEntry(const char* proxy,
		const GSM::L3MobileIdentity& wSubscriber,
		GSM::LogicalChannel* wChannel,
		const GSM::L3CalledPartyBCDNumber& wCalled,
		const char* wMessage);

	/** Form for MO-SMS with a parallel call; sets yet-unknown TI to 7 and GSM state to SMSSubmitting */
	TransactionEntry(const char* proxy,
		const GSM::L3MobileIdentity& wSubscriber,
		GSM::LogicalChannel* wChannel);

	/** Form for "handover-originated" calls */
	TransactionEntry(const char* proxy,
		const GSM::L3MobileIdentity& wSubscriber,
		GSM::LogicalChannel* wChannel,
		unsigned wL3TI,
		const GSM::L3CMServiceType& wService);
//	void addHandoverEntry(HandoverEntry* wHandoverEntry);
	
	/** Form for "temporary" transaction to support outgoing handover*/
	TransactionEntry(TransactionEntry *wOldTransaction, 
		const GSM::L3MobileIdentity& wSubscriber,
		string whichBTS,
		unsigned wL3TI, string wDRTPIp, short wDRTPPort, unsigned wCodec);
	
	/** Delete the database entry upon destruction. */
	~TransactionEntry();
		
	/**@name Accessors. */
	//@{
	unsigned L3TI() const { return mL3TI; }
	void L3TI(unsigned wL3TI);

	const GSM::LogicalChannel* channel() const { return mChannel; }
	GSM::LogicalChannel* channel() { return mChannel; }

	void channel(GSM::LogicalChannel* wChannel);

	const GSM::L3MobileIdentity& subscriber() const { return mSubscriber; }

	const GSM::L3CMServiceType& service() const { return mService; }

	const GSM::L3CalledPartyBCDNumber& called() const { return mCalled; }
	void called(const GSM::L3CalledPartyBCDNumber&);

	const GSM::L3CallingPartyBCDNumber& calling() const { return mCalling; }

	const char* message() const { return mMessage.c_str(); }
	void message(const char *wMessage, size_t length);
	const char* messageType() const { return mContentType.c_str(); }
	void messageType(const char *wContentType);

	unsigned ID() const { return mID; }

	GSM::CallState GSMState() const { ScopedLock lock(mLock); return mGSMState; }
	void GSMState(GSM::CallState wState);

	//const HandoverEntry * handoverEntry() const { return mHandoverEntry; }
//	HandoverEntry * handoverEntry() { return mHandoverEntry; }
	
	// needed to create a temporary transaction for outgoing handover
	short destRTPPort() const { return mSIP.destRTPPort(); }
	char* destRTPIp() { return mSIP.destRTPIp(); }
	short codec() const { return mSIP.codec(); }
	TransactionEntry* callingTransaction() { return mOldTransaction; }
	//@}


	/** Initiate the termination process. */
	void terminate() { ScopedLock lock(mLock); mTerminationRequested=true; }

	bool terminationRequested();

	/**@name SIP-side operations */
	//@{

	SIP::SIPState SIPState() { ScopedLock lock(mLock); return mSIP.state(); } 

	bool SIPFinished() { ScopedLock lock(mLock); return mSIP.finished(); } 

	bool instigator() { ScopedLock lock(mLock); return mSIP.instigator(); }

	SIP::SIPState MOCSendINVITE(const char* calledUser, const char* calledDomain, short rtpPort, unsigned codec);
	SIP::SIPState MOCResendINVITE();
	SIP::SIPState MOCWaitForOK();
	SIP::SIPState MOCSendACK();
	void MOCInitRTP() { ScopedLock lock(mLock); return mSIP.MOCInitRTP(); }

	SIP::SIPState SOSSendINVITE(short rtpPort, unsigned codec);
	SIP::SIPState SOSResendINVITE() { return MOCResendINVITE(); }
	SIP::SIPState SOSWaitForOK() { return MOCWaitForOK(); }
	SIP::SIPState SOSSendACK() { return MOCSendACK(); }
	void SOSInitRTP() { MOCInitRTP(); }

	// few functions below are needed to serve outgoing handover inside a 
	// "temporary" transaction
	SIP::SIPState HOSendINVITE(string whichBTS);
	SIP::SIPState HOWaitForOK();
	SIP::SIPState HOSendACK();
	SIP::SIPState HOSendREINVITE(char *ip, short port, unsigned codec);
	
	SIP::SIPState MTCSendTrying();
	SIP::SIPState MTCSendRinging();
	SIP::SIPState MTCWaitForACK();
	SIP::SIPState MTCCheckForCancel();
	SIP::SIPState MTCSendOK(short rtpPort, unsigned codec);
	void MTCInitRTP() { ScopedLock lock(mLock); mSIP.MTCInitRTP(); }

	SIP::SIPState MODSendBYE();
	SIP::SIPState MODSendUnavail();
	SIP::SIPState MODSendCANCEL();
	SIP::SIPState MODResendBYE();
	SIP::SIPState MODResendCANCEL();
	SIP::SIPState MODWaitForBYEOK();
	SIP::SIPState MODWaitForCANCELOK();
	SIP::SIPState MODWaitFor487();

	SIP::SIPState MTDCheckBYE();
	SIP::SIPState MTDSendBYEOK();
	SIP::SIPState MTDSendCANCELOK();

	// TODO: Remove contentType from here and use the setter above.
	SIP::SIPState MOSMSSendMESSAGE(const char* calledUser, const char* calledDomain, const char* contentType);
	SIP::SIPState MOSMSWaitForSubmit();

	SIP::SIPState MTSMSSendOK();

	bool sendINFOAndWaitForOK(unsigned info);
	
	void sendINFO(const char * measurements);

	void txFrame(unsigned char* frame) { return mSIP.txFrame(frame); }
	int rxFrame(unsigned char* frame) { return mSIP.rxFrame(frame); }
	bool startDTMF(char key) { return mSIP.startDTMF(key); }
	void stopDTMF() { mSIP.stopDTMF(); }

	void SIPUser(const std::string& IMSI) { SIPUser(IMSI.c_str()); }
	void SIPUser(const char* IMSI);
	void SIPUser(const char* callID, const char *IMSI , const char *origID, const char *origHost);

	const std::string SIPCallID() const { ScopedLock lock(mLock); return mSIP.callID(); }

	
	/** acknowledge handover initiation; publish handoverReference + cellId + chanId */
	SIP::SIPState HOCSendHandoverAck(unsigned handoverReference, unsigned BCC, unsigned NCC, unsigned C0, const char *channelDescription);
	/** drop handover-originated "call setup" */
	SIP::SIPState HOCTimeout();
	/** complete handover-originated "call setup" and provide rtp endpoint*/
	SIP::SIPState HOCSendOK(short rtpPort, unsigned codec);
	
	// Send Handover Command to move the current call
	void HOSendHandoverCommand(GSM::L3CellDescription wCell, GSM::L3ChannelDescription wChan, unsigned wHandoverReference);
	
	bool proxyTransaction() { return mProxyTransaction; }
	
	bool handoverTarget(char *cell, char *chan , unsigned *reference) 
		{ return mSIP.handoverTarget(cell, chan , reference);}
	bool reinviteTarget(char *ip, char *port, unsigned *codec)
		{ return mSIP.reinviteTarget(ip, port , codec);}
	
	// These are called by SIPInterface.
	void saveINVITE(const osip_message_t* invite, bool local)
		{ ScopedLock lock(mLock); mSIP.saveINVITE(invite,local); }
	void saveBYE(const osip_message_t* bye, bool local)
		{ ScopedLock lock(mLock); mSIP.saveBYE(bye,local); }

	//@}

	unsigned stateAge() const { ScopedLock lock(mLock); return mStateTimer.elapsed(); }

	/**@name Timer access. */
	//@{

	bool timerExpired(const char* name) const;

	void setTimer(const char* name)
		{ ScopedLock lock(mLock); return mTimers[name].set(); }

	void setTimer(const char* name, long newLimit)
		{ ScopedLock lock(mLock); return mTimers[name].set(newLimit); }

	void resetTimer(const char* name)
		{ ScopedLock lock(mLock); return mTimers[name].reset(); }

	/** Return true if any Q.931 timer is expired. */
	bool anyTimerExpired() const;

	/** Reset all Q.931 timers. */
	void resetTimers();
	
	//@}

	/** Return true if clearing is in progress in the GSM side. */
	bool clearingGSM() const
		{ ScopedLock lock(mLock); return (mGSMState==GSM::ReleaseRequest) || (mGSMState==GSM::DisconnectIndication); }

	/** Retrns true if the transaction is "dead". */
	bool dead() const;

	/** Dump information as text for debugging. */
	void text(std::ostream&) const;

	private:

	friend class TransactionTable;

	/** Create L3 timers from GSM and Q.931 (network side) */
	void initTimers();

	/** Echo latest SIPSTATE to the database. */
	void echoSIPState(SIP::SIPState state) const;
};


std::ostream& operator<<(std::ostream& os, const TransactionEntry&);


/** A map of transactions keyed by ID. */
class TransactionMap : public std::map<unsigned,TransactionEntry*> {};

/**
	A table for tracking the states of active transactions.
*/
class TransactionTable {

	private:

	sqlite3 *mDB;			///< database connection

	TransactionMap mTable;
	mutable Mutex mLock;
	unsigned mIDCounter;

	public:

	/**
		Initialize thetransaction table with a random mIDCounter value.
	*/
	void init();

	~TransactionTable();

	// TransactionTable does not need a destructor.

	/**
		Return a new ID for use in the table.
	*/
	unsigned newID();

	/**
		Insert a new entry into the table; deleted by the table later.
		@param value The entry to insert into the table; will be deleted by the table later.
	*/
	void add(TransactionEntry* value);

	/**
		Find an entry and return a pointer into the table.
		@param wID The transaction ID to search
		@return NULL if ID is not found or was dead
	*/
	TransactionEntry* find(unsigned wID);

	/**
		Find the longest-running non-SOS call.
		@return NULL if there are no calls or if all are SOS.
	*/
	TransactionEntry* findLongestCall();

	/**
		Return the availability of this particular RTP port
		@return True if Port is available, False otherwise
	*/
	bool RTPAvailable(short rtpPort);

	/**
		Remove an entry from the table and from gSIPMessageMap.
		@param wID The transaction ID to search.
		@return True if the ID was really in the table and deleted.
	*/
	bool remove(unsigned wID);

	bool remove(TransactionEntry* transaction) { return remove(transaction->ID()); }

	/**
		Remove an entry from the table and from gSIPMessageMap,
		if it is in the Paging state.
		@param wID The transaction ID to search.
		@return True if the ID was really in the table and deleted.
	*/
	bool removePaging(unsigned wID);


	/**
		Find an entry by its channel pointer.
		Also clears dead entries during search.
		@param chan The channel pointer to the first record found.
		@return pointer to entry or NULL if no active match
	*/
	TransactionEntry* find(const GSM::LogicalChannel *chan);

	/**
		Find an entry in the given state by its mobile ID.
		Also clears dead entries during search.
		@param mobileID The mobile to search for.
		@return pointer to entry or NULL if no match
	*/
	TransactionEntry* find(const GSM::L3MobileIdentity& mobileID, GSM::CallState state);


	/** Find by subscriber and SIP call ID. */
	TransactionEntry* find(const GSM::L3MobileIdentity& mobileID, const char* callID);

	/**
		Find an entry in the Paging state by its mobile ID, change state to AnsweredPaging and reset T3113.
		Also clears dead entries during search.
		@param mobileID The mobile to search for.
		@return pointer to entry or NULL if no match
	*/
	TransactionEntry* answeredPaging(const GSM::L3MobileIdentity& mobileID);


	/**
		Find the channel, if any, used for current transactions by this mobile ID.
		@param mobileID The target mobile subscriber.
		@return pointer to TCH/FACCH, SDCCH or NULL.
	*/
	GSM::LogicalChannel* findChannel(const GSM::L3MobileIdentity& mobileID);

	/** Count the number of transactions using a particular channel. */
	unsigned countChan(const GSM::LogicalChannel*);

	size_t size() { ScopedLock lock(mLock); return mTable.size(); }

	size_t dump(std::ostream& os) const;


	private:

	friend class TransactionEntry;

	/** Accessor to database connection. */
	sqlite3* DB() { return mDB; }

	/**
		Remove "dead" entries from the table.
		A "dead" entry is a transaction that is no longer active.
		The caller should hold mLock.
	*/
	void clearDeadEntries();

	/**
		Remove and entry from the table and from gSIPInterface.
	*/
	void innerRemove(TransactionMap::iterator);

};





}	//Control



/**@addtogroup Globals */
//@{
/** A single global transaction table in the global namespace. */
extern Control::TransactionTable gTransactionTable;
//@}



#endif

// vim: ts=4 sw=4
