/*
* Copyright 2008 Free Software Foundation, Inc.
* Copyright 2010 Kestrel Signal Processing, Inc.
* Copyright 2011 Range Networks, Inc.
* Copyright 2012 Fairwaves LLC, Dmitri Soloviev <dmi3sol@gmail.com>
*
* This software is distributed under the terms of the GNU Affero Public License.
* See the COPYING file in the main directory for details.
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


#ifndef SIPENGINE_H
#define SIPENGINE_H

#include <string>
#include <sys/time.h>
#include <sys/types.h>
#include <semaphore.h>


#include <osip2/osip.h>
#include <ortp/ortp.h>

#include <Sockets.h>
#include <Globals.h>


namespace SIP {


class SIPInterface;


enum SIPState  {
	NullState,
	Timeout,
	Starting,
	Proceeding,	
	Ringing,
	Busy,
	Connecting,
	Active,
	MODClearing,
	MODCanceling,
	MTDClearing,
	MTDCanceling,
	Canceled,
	Cleared,
	Fail,
	MessageSubmit,
			//HO state
	HO_Invited,
	HO_Command,
	HO_Proxy
	
//	HO_Initiated,
//	HO_WaitAccess,
//	HO_Active
};


const char* SIPStateString(SIPState s);
std::ostream& operator<<(std::ostream& os, SIPState s);

class SIPEngine 
{

public:

	enum Method { SIPRegister =0, SIPUnregister=1 };

private:


	/**@name General SIP tags and ids. */
	//@{
	std::string mRemoteUsername;
	std::string mRemoteDomain;
	std::string mMyTag;
	std::string mCallID;
	std::string mViaBranch;
	std::string mSIPUsername;	///< our SIP username
	unsigned  mCSeq;
	/**@name Pre-formed headers. These point into mINVITE. */
	//@{
	osip_from_t* mMyToFromHeader;	///< To/From header for us
	osip_from_t* mRemoteToFromHeader;	///< To/From header for the remote party
	osip_call_id_t *mCallIDHeader;		///< the call ID header for this transaction
	//@}
	//@}

	/**@name SIP UDP parameters */
	//@{
	unsigned mSIPPort;			///< our local SIP port
	std::string mSIPIP;			///< our SIP IP address as seen by the proxy
	std::string mProxyIP;			///< IP address of the SIP proxy
	unsigned mProxyPort;			///< UDP port number of the SIP proxy
	struct ::sockaddr_in mProxyAddr;	///< the ready-to-use UDP address
	
	struct ::sockaddr_in mHOtoBTSAddr;	///< target iBTS address

	//@}

	/**@name Saved SIP messages. */
	//@{
	osip_message_t * mINVITE;	///< the INVITE message for this transaction
	osip_message_t * mLastResponse;	///< the last response received for this transaction
	//we should maybe push these together sometime? -kurtis
	osip_message_t * mBYE;		///< the BYE message for this transaction
	osip_message_t * mCANCEL;	///< the CANCEL message for this transaction
	osip_message_t * mERROR;	///< the ERROR message for this transaction
	//@}

	/**@name RTP state and parameters. */
	//@{
	short mRTPPort;
	short mDRTPPort;
	char mDRTPIp[20];		// destination ip (rtp)
	unsigned mCodec;
	RtpSession * mSession;		///< RTP media session
	unsigned int mTxTime;		///< RTP transmission timestamp in 8 kHz samples
	unsigned int mRxTime;		///< RTP receive timestamp in 8 kHz samples
	//@}

	SIPState mState;			///< current SIP call state
	bool mInstigator;               ///< true if this side initiated the call

	/**@name RFC-2833 DTMF state. */
	//@{
	char mDTMF;					///< current DTMF digit, \0 if none
	unsigned mDTMFDuration;		///< duration of DTMF event so far
	unsigned mDTMFStartTime;	///< start time of the DTMF key event
	//@}

public:

	/**
		Default constructor. Initialize the object.
		@param proxy <host>:<port>
	*/
	SIPEngine(const char* proxy, const char* IMSI=NULL);

	/** Destroy held message copies. */
	~SIPEngine();

	osip_message_t* get_message();
	osip_message_t* HOGetResp();
	
	const std::string& callID() const { return mCallID; } 

	const std::string& proxyIP() const { return mProxyIP; }
	unsigned proxyPort() const { return mProxyPort; }

	/** Return the current SIP call state. */
	SIPState state() const { return mState; }

	/** Return the RTP Port being used. */
	short RTPPort() const { return mRTPPort; }
	short destRTPPort() const { return mDRTPPort; }
	char* destRTPIp() { return mDRTPIp; }
	unsigned codec() const { return mCodec; }

	/** Return if the call has successfully finished */
	bool finished() const { return (mState==Cleared || mState==Canceled); }

	/** Return if the communication was started by us (true) or not (false) */
	/* requires an mINVITE be established */
	bool instigator() const { return mInstigator; }
	
	/** Set the user to IMSI<IMSI> and generate a call ID; for mobile-originated transactions. */
	void user( const char * IMSI );

	/** Set the use to IMSI<IMSI> and set the other SIP call parameters; for network-originated transactions. */
	void user( const char * wCallID, const char * IMSI , const char *origID, const char *origHost);

	/** Set generate call ID for outgoing handover */
	SIPEngine(const char* proxy, const char* IMSI, unsigned wL3TI, string wDRTPIp, short wDestRTP, unsigned wCodec);
	
	/** fetch parameters for Handover Command */
	bool handoverTarget(char *cell, char *chan , unsigned *reference);
	/** fetch parameters for re-invite during handover */
	bool reinviteTarget(char *ip, char *port, unsigned *codec);
	/**@name Messages for SIP registration. */
	//@{

	/**
		Send sip register and look at return msg.
		Can throw SIPTimeout().
		@return True on success.
	*/
	bool Register(Method wMethod=SIPRegister);	

	/**
		Send sip unregister and look at return msg.
		Can throw SIPTimeout().
		@return True on success.
	*/
	bool unregister() { return (Register(SIPUnregister)); };

	//@}

	
	/**@name Messages associated with Emegency call (SOS) procedure. */
	//@{

	/**
		Send an invite message.
		@param rtpPort UDP port to use for speech (will use this and next port)
		@param codec Code for codec to be used.
		@return New SIP call state.
	*/
	SIPState SOSSendINVITE(short rtpPort, unsigned codec, const GSM::LogicalChannel *chan = NULL);

	//SIPState SOSResendINVITE();

	//SIPState SOSWaitForOK();

	//SIPState SOSSendACK();

	//@}


	
	/**@name Messages associated with MOC procedure. */
	//@{

	/**
		Send an invite message.
		@param calledUser SIP userid or E.164 address.
		@param calledDomain SIP user's domain.
		@param rtpPort UDP port to use for speech (will use this and next port)
		@param codec Code for codec to be used.
		@return New SIP call state.
	*/
	SIPState MOCSendINVITE(const char * calledUser,
		const char * calledDomain, short rtpPort, unsigned codec,
		const GSM::LogicalChannel *chan = NULL);


	SIPState MOCResendINVITE();

	SIPState MOCCheckForOK(Mutex *lock);

	SIPState MOCSendACK();

	//@}

	/**@name Messages associated with MOSMS procedure. */
	//@{

	/**
		Send an instant message.
		@param calledUsername SIP userid or E.164 address.
		@param calledDomain SIP user's domain.
		@param messageText MESSAGE payload as a C string.
		@return New SIP call state.
	*/
	SIPState MOSMSSendMESSAGE(const char * calledUsername,
		const char * calledDomain, const char *messageText,
		const char *contentType,
		const GSM::LogicalChannel *chan = NULL);

	SIPState MOSMSWaitForSubmit(Mutex *lock=NULL);

	SIPState MTSMSSendOK(const GSM::LogicalChannel *chan = NULL);

	//@}


	/**@name Messages associated with MTC procedure. */
	//@{
	SIPState MTCSendTrying();

	SIPState MTCSendRinging();

	SIPState MTCSendOK(short rtpPort, unsigned codec, const GSM::LogicalChannel *chan = NULL);

	SIPState MTCCheckForACK(Mutex *lock);

	SIPState MTCCheckForCancel();
	//@}

	/**@name Messages associated with MTSMS procedure. */
	//@{

	SIPState MTCSendOK();

	//@}
	
	SIPState HOCSendOK(short rtpPort, unsigned codec);
	
	SIPState HOCSendTemporarilyUnavailable();


	/**@name Messages for MOD procedure. */
	//@{
	SIPState MODSendBYE();

	SIPState MODSendERROR(osip_message_t * cause, int code, const char * reason, bool cancel);

	SIPState MODSendCANCEL();

	SIPState MODResendBYE();

	SIPState MODResendCANCEL();

	SIPState MODResendERROR(bool cancel);

	SIPState MODWaitForBYEOK(Mutex *lock=NULL);

	SIPState MODWaitForCANCELOK(Mutex *lock=NULL);

	SIPState MODWaitForERRORACK(bool cancel, Mutex *lock=NULL);

	SIPState MODWaitFor487(Mutex *lock=NULL);

	SIPState MODWaitForResponse(vector<unsigned> *validResponses, Mutex *lock=NULL);

	//@}


	/**@name Messages for MTD procedure. */
	//@{
	SIPState MTDCheckBYE();	

	SIPState MTDSendBYEOK();

	SIPState MTDSendCANCELOK();
	//@}
	SIPState HOSendINVITE(string whichBTS);
//	SIPState HOWaitForOK();
	SIPState HOSendACK();
	SIPState HOSendREINVITE(char *ip, short port, unsigned codec);
	
	// sip body must contain target cell parameters for handover
	SIPState HOCSendProceeding(const char *body);


	/** Set up to start sending RFC2833 DTMF event frames in the RTP stream. */
	bool startDTMF(char key);

	/** Send a DTMF end frame and turn off the DTMF events. */
	void stopDTMF();

	/** Send a vocoder frame over RTP. */
	void txFrame(unsigned char* frame);

	/**
		Receive a vocoder frame over RTP.
		@param The vocoder frame
		@return new RTP timestamp
	*/
	int  rxFrame(unsigned char* frame);

	void MOCInitRTP();
	void MTCInitRTP();

	/** In-call Signalling */
	//@{

	/**
		Send a SIP INFO message, usually for DTMF.
		Most parameters taken from current SIPEngine state.
		This call blocks for the response.
		@param wInfo The DTMF signalling code.
		@return Success/Fail flag.
	*/
	bool sendINFOAndWaitForOK(unsigned wInfo, Mutex *lock=NULL);

	//@}
        
        /**
		A way to deliver Measurement Results  04.08 10.5.2.20
	*/
	void sendINFO(const char * wInfo);


	/** Save a copy of an INVITE or MESSAGE message in the engine. */
	void saveINVITE(const osip_message_t *INVITE, bool mine);

	/** Save a copy of a response message in the engine. */
	void saveResponse(osip_message_t *respsonse);

	/** Save a copy of a BYE message in the engine. */
	void saveBYE(const osip_message_t *BYE, bool mine);

	/** Save a copy of a CANCEL message in the engine. */
	void saveCANCEL(const osip_message_t *CANCEL, bool mine);

	/** Save a copy of a ERROR message in the engine. */
	void saveERROR(const osip_message_t *ERROR, bool mine);

	
	/** Determine if this invite matches the saved one */
	bool sameINVITE(osip_message_t * msg);

	private:

	/**
		Initialize the RTP session.
		@param msg A SIP INVITE or 200 OK wth RTP parameters
	*/
	void InitRTP(const osip_message_t * msg );

	/**
		Generate a standard set of private headers on initiating messages.
	*/
	void writePrivateHeaders(osip_message_t *msg, const GSM::LogicalChannel *chan);

};


}; 

#endif // SIPENGINE_H
// vim: ts=4 sw=4
