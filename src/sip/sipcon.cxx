/*
 * sipcon.cxx
 *
 * Session Initiation Protocol connection.
 *
 * Open Phone Abstraction Library (OPAL)
 *
 * Copyright (c) 2000 Equivalence Pty. Ltd.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Open Phone Abstraction Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#include <ptlib.h>
#include <opal/buildopts.h>

#if OPAL_SIP

#ifdef __GNUC__
#pragma implementation "sipcon.h"
#endif

#include <sip/sipcon.h>

#include <sip/sipep.h>
#include <codec/rfc2833.h>
#include <opal/manager.h>
#include <opal/call.h>
#include <opal/patch.h>
#include <opal/transcoders.h>
#include <ptclib/random.h>              // for local dialog tag
#include <ptclib/pdns.h>
#include <ptclib/pxml.h>
#include <h323/q931.h>

#if OPAL_HAS_H224
#include <h224/h224.h>
#endif

#if OPAL_HAS_IM
#include <im/msrp.h>
#endif

#define new PNEW

//
//  uncomment this to force pause ReINVITES to have c=0.0.0.0
//
//#define PAUSE_WITH_EMPTY_ADDRESS  1

typedef void (SIPConnection::* SIPMethodFunction)(SIP_PDU & pdu);

static const PConstCaselessString HeaderPrefix(SIP_HEADER_PREFIX);
static const PConstCaselessString RemotePartyID("Remote-Party-ID");
static const PConstCaselessString ApplicationDTMFRelayKey("application/dtmf-relay");
static const PConstCaselessString ApplicationDTMFKey("application/dtmf");

#if OPAL_VIDEO
static const char ApplicationMediaControlXMLKey[] = "application/media_control+xml";
#endif


static SIP_PDU::StatusCodes GetStatusCodeFromReason(OpalConnection::CallEndReason reason)
{
  static const struct {
    unsigned             q931Code;
    SIP_PDU::StatusCodes sipCode;
  }
  //
  // This table comes from RFC 3398 para 7.2.4.1
  //
  Q931ToSIPCode[] = {
    {   1, SIP_PDU::Failure_NotFound               }, // Unallocated number
    {   2, SIP_PDU::Failure_NotFound               }, // no route to network
    {   3, SIP_PDU::Failure_NotFound               }, // no route to destination
    {  17, SIP_PDU::Failure_BusyHere               }, // user busy                            
    {  18, SIP_PDU::Failure_RequestTimeout         }, // no user responding                   
    {  19, SIP_PDU::Failure_TemporarilyUnavailable }, // no answer from the user              
    {  20, SIP_PDU::Failure_TemporarilyUnavailable }, // subscriber absent                    
    {  21, SIP_PDU::Failure_Forbidden              }, // call rejected                        
    {  22, SIP_PDU::Failure_Gone                   }, // number changed (w/o diagnostic)      
    {  22, SIP_PDU::Redirection_MovedPermanently   }, // number changed (w/ diagnostic)       
    {  23, SIP_PDU::Failure_Gone                   }, // redirection to new destination       
    {  26, SIP_PDU::Failure_NotFound               }, // non-selected user clearing           
    {  27, SIP_PDU::Failure_BadGateway             }, // destination out of order             
    {  28, SIP_PDU::Failure_AddressIncomplete      }, // address incomplete                   
    {  29, SIP_PDU::Failure_NotImplemented         }, // facility rejected                    
    {  31, SIP_PDU::Failure_TemporarilyUnavailable }, // normal unspecified                   
    {  34, SIP_PDU::Failure_ServiceUnavailable     }, // no circuit available                 
    {  38, SIP_PDU::Failure_ServiceUnavailable     }, // network out of order                 
    {  41, SIP_PDU::Failure_ServiceUnavailable     }, // temporary failure                    
    {  42, SIP_PDU::Failure_ServiceUnavailable     }, // switching equipment congestion       
    {  47, SIP_PDU::Failure_ServiceUnavailable     }, // resource unavailable                 
    {  55, SIP_PDU::Failure_Forbidden              }, // incoming calls barred within CUG     
    {  57, SIP_PDU::Failure_Forbidden              }, // bearer capability not authorized     
    {  58, SIP_PDU::Failure_ServiceUnavailable     }, // bearer capability not presently available
    {  65, SIP_PDU::Failure_NotAcceptableHere      }, // bearer capability not implemented
    {  70, SIP_PDU::Failure_NotAcceptableHere      }, // only restricted digital avail    
    {  79, SIP_PDU::Failure_NotImplemented         }, // service or option not implemented
    {  87, SIP_PDU::Failure_Forbidden              }, // user not member of CUG           
    {  88, SIP_PDU::Failure_ServiceUnavailable     }, // incompatible destination         
    { 102, SIP_PDU::Failure_ServerTimeout          }, // recovery of timer expiry         
    { 111, SIP_PDU::Failure_InternalServerError    }, // protocol error                   
    { 127, SIP_PDU::Failure_InternalServerError    }, // interworking unspecified         
  };
  for (PINDEX i = 0; i < PARRAYSIZE(Q931ToSIPCode); i++) {
    if (Q931ToSIPCode[i].q931Code == reason.q931)
      return Q931ToSIPCode[i].sipCode;
  }

  static const struct {
    OpalConnection::CallEndReasonCodes reasonCode;
    SIP_PDU::StatusCodes               sipCode;
  }
  ReasonToSIPCode[] = {
    { OpalConnection::EndedByNoUser            , SIP_PDU::Failure_NotFound               }, // Unallocated number
    { OpalConnection::EndedByLocalBusy         , SIP_PDU::Failure_BusyHere               }, // user busy                            
    { OpalConnection::EndedByLocalUser         , SIP_PDU::Failure_BusyHere               }, // user busy                            
    { OpalConnection::EndedByNoAnswer          , SIP_PDU::Failure_RequestTimeout         }, // no user responding                   
    { OpalConnection::EndedByNoUser            , SIP_PDU::Failure_TemporarilyUnavailable }, // subscriber absent                    
    { OpalConnection::EndedByCapabilityExchange, SIP_PDU::Failure_NotAcceptableHere      }, // bearer capability not implemented
    { OpalConnection::EndedByCallerAbort       , SIP_PDU::Failure_RequestTerminated      },
    { OpalConnection::EndedByCallForwarded     , SIP_PDU::Redirection_MovedTemporarily   },
    { OpalConnection::EndedByAnswerDenied      , SIP_PDU::GlobalFailure_Decline          },
    { OpalConnection::EndedByRefusal           , SIP_PDU::GlobalFailure_Decline          }, // TODO - SGW - add for call reject from H323 side.
    { OpalConnection::EndedByHostOffline       , SIP_PDU::Failure_NotFound               }, // TODO - SGW - add for no ip from H323 side.
    { OpalConnection::EndedByNoEndPoint        , SIP_PDU::Failure_NotFound               }, // TODO - SGW - add for endpoints not running on a ip from H323 side.
    { OpalConnection::EndedByUnreachable       , SIP_PDU::Failure_Forbidden              }, // TODO - SGW - add for avoid sip calls to SGW IP.
    { OpalConnection::EndedByNoBandwidth       , SIP_PDU::GlobalFailure_NotAcceptable    }, // TODO - SGW - added to reject call when no bandwidth 
    { OpalConnection::EndedByInvalidConferenceID,SIP_PDU::Failure_TransactionDoesNotExist}
  };

  for (PINDEX i = 0; i < PARRAYSIZE(ReasonToSIPCode); i++) {
    if (ReasonToSIPCode[i].reasonCode == reason.code)
      return ReasonToSIPCode[i].sipCode;
  }

  return SIP_PDU::Failure_BadGateway;
}

static OpalConnection::CallEndReason GetCallEndReasonFromResponse(SIP_PDU & response)
{
  //
  // This table comes from RFC 3398 para 8.2.6.1
  //
  static const struct {
    SIP_PDU::StatusCodes               sipCode;
    OpalConnection::CallEndReasonCodes reasonCode;
    unsigned                           q931Code;
  } SIPCodeToReason[] = {
    { SIP_PDU::Local_Timeout                      , OpalConnection::EndedByHostOffline       ,  27 }, // Destination out of order
    { SIP_PDU::Failure_RequestTerminated          , OpalConnection::EndedByNoAnswer          ,  19 }, // No answer
    { SIP_PDU::Failure_BadRequest                 , OpalConnection::EndedByQ931Cause         ,  41 }, // Temporary Failure
    { SIP_PDU::Failure_UnAuthorised               , OpalConnection::EndedBySecurityDenial    ,  21 }, // Call rejected (*)
    { SIP_PDU::Failure_PaymentRequired            , OpalConnection::EndedByRefusal           ,  21 }, // Call rejected
    { SIP_PDU::Failure_Forbidden                  , OpalConnection::EndedBySecurityDenial    ,  21 }, // Call rejected
    { SIP_PDU::Failure_NotFound                   , OpalConnection::EndedByNoUser            ,   1 }, // Unallocated number
    { SIP_PDU::Failure_MethodNotAllowed           , OpalConnection::EndedByQ931Cause         ,  63 }, // Service or option unavailable
    { SIP_PDU::Failure_NotAcceptable              , OpalConnection::EndedByQ931Cause         ,  79 }, // Service/option not implemented (+)
    { SIP_PDU::Failure_ProxyAuthenticationRequired, OpalConnection::EndedBySecurityDenial    ,  21 }, // Call rejected (*)
    { SIP_PDU::Failure_RequestTimeout             , OpalConnection::EndedByTemporaryFailure  , 102 }, // Recovery on timer expiry
    { SIP_PDU::Failure_Gone                       , OpalConnection::EndedByQ931Cause         ,  22 }, // Number changed (w/o diagnostic)
    { SIP_PDU::Failure_RequestEntityTooLarge      , OpalConnection::EndedByQ931Cause         , 127 }, // Interworking (+)
    { SIP_PDU::Failure_RequestURITooLong          , OpalConnection::EndedByQ931Cause         , 127 }, // Interworking (+)
    { SIP_PDU::Failure_UnsupportedMediaType       , OpalConnection::EndedByCapabilityExchange,  79 }, // Service/option not implemented (+)
    { SIP_PDU::Failure_NotAcceptableHere          , OpalConnection::EndedByCapabilityExchange,  79 }, // Service/option not implemented (+)
    { SIP_PDU::Failure_UnsupportedURIScheme       , OpalConnection::EndedByQ931Cause         , 127 }, // Interworking (+)
    { SIP_PDU::Failure_BadExtension               , OpalConnection::EndedByNoUser            , 127 }, // Interworking (+)
    { SIP_PDU::Failure_ExtensionRequired          , OpalConnection::EndedByNoUser            , 127 }, // Interworking (+)
    { SIP_PDU::Failure_IntervalTooBrief           , OpalConnection::EndedByQ931Cause         , 127 }, // Interworking (+)
    { SIP_PDU::Failure_TemporarilyUnavailable     , OpalConnection::EndedByTemporaryFailure  ,  18 }, // No user responding
    { SIP_PDU::Failure_TransactionDoesNotExist    , OpalConnection::EndedByQ931Cause         ,  41 }, // Temporary Failure
    { SIP_PDU::Failure_LoopDetected               , OpalConnection::EndedByQ931Cause         ,  25 }, // Exchange - routing error
    { SIP_PDU::Failure_TooManyHops                , OpalConnection::EndedByQ931Cause         ,  25 }, // Exchange - routing error
    { SIP_PDU::Failure_AddressIncomplete          , OpalConnection::EndedByQ931Cause         ,  28 }, // Invalid Number Format (+)
    { SIP_PDU::Failure_Ambiguous                  , OpalConnection::EndedByNoUser            ,   1 }, // Unallocated number
    { SIP_PDU::Failure_BusyHere                   , OpalConnection::EndedByRemoteBusy        ,  17 }, // User busy
    { SIP_PDU::Failure_InternalServerError        , OpalConnection::EndedByOutOfService      ,  41 }, // Temporary failure
    { SIP_PDU::Failure_NotImplemented             , OpalConnection::EndedByQ931Cause         ,  79 }, // Not implemented, unspecified
    { SIP_PDU::Failure_BadGateway                 , OpalConnection::EndedByOutOfService      ,  38 }, // Network out of order
    { SIP_PDU::Failure_ServiceUnavailable         , OpalConnection::EndedByOutOfService      ,  41 }, // Temporary failure
    { SIP_PDU::Failure_ServerTimeout              , OpalConnection::EndedByOutOfService      , 102 }, // Recovery on timer expiry
    { SIP_PDU::Failure_SIPVersionNotSupported     , OpalConnection::EndedByQ931Cause         , 127 }, // Interworking (+)
    { SIP_PDU::Failure_MessageTooLarge            , OpalConnection::EndedByQ931Cause         , 127 }, // Interworking (+)
    { SIP_PDU::GlobalFailure_BusyEverywhere       , OpalConnection::EndedByRemoteBusy        ,  17 }, // User busy
    { SIP_PDU::GlobalFailure_Decline              , OpalConnection::EndedByRefusal           ,  21 }, // Call rejected
    { SIP_PDU::GlobalFailure_DoesNotExistAnywhere , OpalConnection::EndedByNoUser            ,   1 }, // Unallocated number
  };

  for (PINDEX i = 0; i < PARRAYSIZE(SIPCodeToReason); i++) {
    if (response.GetStatusCode() == SIPCodeToReason[i].sipCode)
      return OpalConnection::CallEndReason(SIPCodeToReason[i].reasonCode, SIPCodeToReason[i].q931Code);
  }

  // default Q.931 code is 31 Normal, unspecified
  return OpalConnection::CallEndReason(OpalConnection::EndedByQ931Cause, Q931::NormalUnspecified);
}


////////////////////////////////////////////////////////////////////////////

SIPConnection::SIPConnection(OpalCall & call,
                             SIPEndPoint & ep,
                             const PString & token,
                             const SIPURL & destination,
                             OpalTransport * newTransport,
                             unsigned int options,
                             OpalConnection::StringOptions * stringOptions)
  : OpalRTPConnection(call, ep, token, options, stringOptions)
  , endpoint(ep)
  , transport(newTransport)
  , deleteTransport(newTransport == NULL || !newTransport->IsReliable())
  , m_allowedMethods((1<<SIP_PDU::Method_INVITE)|
                     (1<<SIP_PDU::Method_ACK   )|
                     (1<<SIP_PDU::Method_CANCEL)|
                     (1<<SIP_PDU::Method_BYE   )) // Minimum set
  , m_holdToRemote(eHoldOff)
  , m_holdFromRemote(false)
  , originalInvite(NULL)
  , originalInviteTime(0)
  , m_sdpSessionId(PTime().GetTimeInSeconds())
  , m_sdpVersion(0)
  , m_needReINVITE(false)
  , m_handlingINVITE(false)
  , m_resolveMultipleFormatReINVITE(true)
  , m_symmetricOpenStream(false)
  , m_appearanceCode(ep.GetDefaultAppearanceCode())
  , m_authentication(NULL)
  , m_authenticateErrors(0)
  , m_prackMode((PRACKMode)m_stringOptions.GetInteger(OPAL_OPT_PRACK_MODE, ep.GetDefaultPRACKMode()))
  , m_prackEnabled(false)
  , m_prackSequenceNumber(0)
  , m_responseRetryCount(0)
  , m_referInProgress(false)
#if OPAL_FAX
  , m_switchedToT38(false)
#endif
  , releaseMethod(ReleaseWithNothing)
  , m_receivedUserInputMethod(UserInputMethodUnknown)
//  , m_messageContext(NULL)
{
  SIPURL adjustedDestination = destination;

  // Look for a "proxy" parameter to override default proxy
  PStringToString params = adjustedDestination.GetParamVars();
  SIPURL proxy;
  if (params.Contains(OPAL_PROXY_PARAM)) {
    proxy.Parse(params[OPAL_PROXY_PARAM]);
    adjustedDestination.SetParamVar(OPAL_PROXY_PARAM, PString::Empty());
  }

  if (params.Contains("x-line-id")) {
    m_appearanceCode = params["x-line-id"].AsUnsigned();
    adjustedDestination.SetParamVar("x-line-id", PString::Empty());
  }

  if (params.Contains("appearance")) {
    m_appearanceCode = params["appearance"].AsUnsigned();
    adjustedDestination.SetParamVar("appearance", PString::Empty());
  }

  const PStringToString & query = adjustedDestination.GetQueryVars();
  for (PINDEX i = 0; i < query.GetSize(); ++i)
    m_stringOptions.SetAt(HeaderPrefix+query.GetKeyAt(i), query.GetDataAt(i));
  adjustedDestination.SetQuery(PString::Empty());

  m_stringOptions.ExtractFromURL(adjustedDestination);

  m_dialog.SetRequestURI(adjustedDestination);
  m_dialog.SetRemoteURI(adjustedDestination);

  // Update remote party parameters
  UpdateRemoteAddresses();

  if (proxy.IsEmpty())
    proxy = endpoint.GetProxy();

  if (proxy.IsEmpty())
    proxy = endpoint.GetRegisteredProxy(adjustedDestination);

  m_dialog.SetProxy(proxy, false); // No default routeSet if there is a proxy for INVITE

  forkedInvitations.DisallowDeleteObjects();
  pendingInvitations.DisallowDeleteObjects();
  m_pendingTransactions.DisallowDeleteObjects();

  m_responseFailTimer.SetNotifier(PCREATE_NOTIFIER(OnInviteResponseTimeout));
  m_responseRetryTimer.SetNotifier(PCREATE_NOTIFIER(OnInviteResponseRetry));

  sessionTimer.SetNotifier(PCREATE_NOTIFIER(OnSessionTimeout));

  PTRACE(4, "SIP\tCreated connection.");
}


SIPConnection::~SIPConnection()
{
  PTRACE(4, "SIP\tDeleting connection.");

  // Delete the transport now we are finished with it
  SetTransport(SIPURL());

  delete m_authentication;
  delete originalInvite;
}


void SIPConnection::OnApplyStringOptions()
{
  m_prackMode = (PRACKMode)m_stringOptions.GetInteger(OPAL_OPT_PRACK_MODE, m_prackMode);
  OpalRTPConnection::OnApplyStringOptions();
}


bool SIPConnection::GarbageCollection()
{
  /* Note we wait for various transactions to complete as the transport they
     rely on may be owned by the connection, and would be deleted once we exit
     from OnRelease() causing a crash in the transaction processing. */
  PSafePtr<SIPTransaction> transaction;
  while ((transaction = m_pendingTransactions.GetAt(0, PSafeReference)) != NULL) {
    if (!transaction->IsTerminated())
      return false;

    PTRACE(4, "SIP\tRemoved terminated transaction, id=" << transaction->GetTransactionID());
    m_pendingTransactions.Remove(transaction);
  }

  // Remove all the references to the transactions so garbage can be collected
  pendingInvitations.RemoveAll();
  forkedInvitations.RemoveAll();

  return OpalConnection::GarbageCollection();
}


bool SIPConnection::SetTransport(const SIPURL & destination)
{
  PTRACE(4, "SIP\tSetting new transport for destination \"" << destination << '"');

  OpalTransport * newTransport = NULL;
  if (!destination.IsEmpty()) {
    newTransport = endpoint.CreateTransport(destination, m_stringOptions(OPAL_OPT_INTERFACE));
    if (newTransport == NULL)
      return false;
  }

  if (deleteTransport && transport != NULL) {
    transport->CloseWait();
    delete transport;
  }

  transport = newTransport;
  deleteTransport = true;

  return transport != NULL;
}


void SIPConnection::OnReleased()
{
  PTRACE(3, "SIP\tOnReleased: " << *this);

  if (m_referInProgress) {
    m_referInProgress = false;

    PStringToString info;
    info.SetAt("result", "blind");
    info.SetAt("party", "B");
    OnTransferNotify(info, this);
  }

  SIPDialogNotification::Events notifyDialogEvent = SIPDialogNotification::NoEvent;
  SIP_PDU::StatusCodes sipCode = SIP_PDU::IllegalStatusCode;

  PSafePtr<SIPBye> bye;

  switch (releaseMethod) {
    case ReleaseWithNothing :
      for (PSafePtr<SIPTransaction> invitation(forkedInvitations, PSafeReference); invitation != NULL; ++invitation) {
        /* If we never even received a "100 Trying" from a remote, then just abort
           the transaction, do not wait, it is probably on an interface that the
           remote is not physically on. */
        if (!invitation->IsCompleted())
          invitation->Abort();
        notifyDialogEvent = SIPDialogNotification::Timeout;
      }
      break;

    case ReleaseWithResponse :
      // Try find best match for return code
      sipCode = GetStatusCodeFromReason(callEndReason);

      // EndedByCallForwarded is a special case because it needs extra paramater
      if (callEndReason != EndedByCallForwarded)
        SendInviteResponse(sipCode);
      else {
        SIP_PDU response(*originalInvite, sipCode);
        AdjustInviteResponse(response);
        response.GetMIME().SetContact(m_forwardParty);
        originalInvite->SendResponse(*transport, response); 
      }

      /* Wait for ACK from remote before destroying object. Note that we either
         get the ACK, or OnAckTimeout() fires and sets this flag anyway. We are
         outside of a connection mutex lock at this point, so no deadlock
         should occur. Really should be a PSyncPoint. */
      while (!m_responsePackets.empty())
        PThread::Sleep(100);

      notifyDialogEvent = SIPDialogNotification::Rejected;
      break;

    case ReleaseWithBYE :
      // create BYE now & delete it later to prevent memory access errors
      bye = new SIPBye(*this);
      if (!bye->Start()) {
        delete bye;
        bye.SetNULL();
      }

      for (PSafePtr<SIPTransaction> invitation(forkedInvitations, PSafeReference); invitation != NULL; ++invitation) {
        /* If we never even received a "100 Trying" from a remote, then just abort
            the transaction, do not wait, it is probably on an interface that the
            remote is not physically on. */
        if (!invitation->IsCompleted())
          invitation->Abort();
      }
      break;

    case ReleaseWithCANCEL :
      PTRACE(3, "SIP\tCancelling " << forkedInvitations.GetSize() << " transactions.");
      for (PSafePtr<SIPTransaction> invitation(forkedInvitations, PSafeReference); invitation != NULL; ++invitation) {
        /* If we never even received a "100 Trying" from a remote, then just abort
           the transaction, do not wait, it is probably on an interface that the
           remote is not physically on, otherwise we have to CANCEL and wait. */
        if (invitation->IsTrying())
          invitation->Abort();
        else
          invitation->Cancel();
      }
      notifyDialogEvent = SIPDialogNotification::Cancelled;
  }

  // Abort the queued up re-INVITEs we never got a chance to send.
  for (PSafePtr<SIPTransaction> invitation(pendingInvitations, PSafeReference); invitation != NULL; ++invitation)
    invitation->Abort();

  // No termination event set yet, get it from the call end reason
  if (notifyDialogEvent == SIPDialogNotification::NoEvent) {
    switch (GetCallEndReason()) {
      case EndedByRemoteUser :
        notifyDialogEvent = SIPDialogNotification::RemoteBye;
        break;

      case OpalConnection::EndedByCallForwarded :
        notifyDialogEvent = SIPDialogNotification::Replaced;
        break;

      default :
        notifyDialogEvent = SIPDialogNotification::LocalBye;
    }
  }

  NotifyDialogState(SIPDialogNotification::Terminated, notifyDialogEvent, sipCode);

  if (bye != NULL) {
    bye->WaitForCompletion();
    bye.SetNULL();
  }

  // Close media and indicate call ended, even though we have a little bit more
  // to go in clean up, don't let other bits wait for it.
  OpalRTPConnection::OnReleased();

  // If forwardParty is a connection token, then must be INVITE with replaces scenario
  PSafePtr<OpalConnection> replacerConnection = GetEndPoint().GetConnectionWithLock(m_forwardParty);
  if (replacerConnection != NULL) {
    /* According to RFC 3891 we now send a 200 OK in both the early and confirmed
       dialog cases. OnReleased() is responsible for if the replaced connection is
       sent a BYE or a CANCEL. */
    replacerConnection->SetConnected();
  }
}


bool SIPConnection::TransferConnection(const PString & remoteParty)
{
  // There is still an ongoing REFER transaction 
  if (m_referInProgress) {
    PTRACE(2, "SIP\tTransfer already in progress for " << *this);
    return false;
  }

  if ((m_allowedMethods&(1<<SIP_PDU::Method_REFER)) == 0) {
    PTRACE(2, "SIP\tRemote does not allow REFER message.");
    return false;
  }

  PTRACE(3, "SIP\tTransferring " << *this << " to " << remoteParty);

  PURL url(remoteParty, "sip");
  StringOptions extra;
  extra.ExtractFromURL(url);

  // Tell the REFER processing UA if it should suppress NOTIFYs about the REFER processing.
  // If we want to get NOTIFYs we have to clear the old connection on the progress message
  // where the connection is transfered. See OnTransferNotify().
  const char * referSub = extra.GetBoolean(OPAL_OPT_REFER_SUB,
                m_stringOptions.GetBoolean(OPAL_OPT_REFER_SUB, true)) ? "true" : "false";

  PSafePtr<OpalCall> call = endpoint.GetManager().FindCallWithLock(url.GetHostName(), PSafeReadOnly);
  if (call == NULL) {
    SIPRefer * referTransaction = new SIPRefer(*this, remoteParty, m_dialog.GetLocalURI(), referSub);
    m_referInProgress = referTransaction->Start();
    return m_referInProgress;
  }

  if (call == &ownerCall) {
    PTRACE(2, "SIP\tCannot transfer connection to itself: " << *this);
    return false;
  }

  for (PSafePtr<OpalConnection> connection = call->GetConnection(0); connection != NULL; ++connection) {
    PSafePtr<SIPConnection> sip = PSafePtrCast<OpalConnection, SIPConnection>(connection);
    if (sip != NULL) {
      /* Note that the order of to-tag and remote-tag is counter intuitive. This is because
        the call being referred to by the call token in remoteParty is not the A party in
        the consultation transfer, but the B party. */
      PTRACE(4, "SIP\tTransferring " << *this << " to remote of " << *sip);

      /* The following is to compensate for Avaya who send a Contact without a
         username in the URL and then get upset later in th REFER when we use
         what they told us to use. They can't do the REFER without a username
         part, but they never gave us a username to give them. Give me a break!
       */
      SIPURL referTo = sip->GetRemotePartyURL();
      if (remoteProductInfo.name == "Avaya" && referTo.GetUserName().IsEmpty())
        referTo.SetUserName("anonymous");

      PStringStream id;
      id <<                 sip->GetDialog().GetCallID()
         << ";to-tag="   << sip->GetDialog().GetRemoteTag()
         << ";from-tag=" << sip->GetDialog().GetLocalTag();
      referTo.SetQueryVar("Replaces", id);

      SIPRefer * referTransaction = new SIPRefer(*this, referTo, m_dialog.GetLocalURI(), referSub);
      referTransaction->GetMIME().AddSupported("replaces");
      m_referInProgress = referTransaction->Start();
      return m_referInProgress;
    }
  }

  PTRACE(2, "SIP\tConsultation transfer requires other party to be SIP.");
  return false;
}


PBoolean SIPConnection::SetAlerting(const PString & /*calleeName*/, PBoolean withMedia)
{
  if (IsOriginating() || originalInvite == NULL) {
    PTRACE(2, "SIP\tSetAlerting ignored on call we originated.");
    return PTrue;
  }

  PSafeLockReadWrite safeLock(*this);
  if (!safeLock.IsLocked())
    return PFalse;
  
  PTRACE(3, "SIP\tSetAlerting");

  if (GetPhase() >= AlertingPhase) 
    return PFalse;

  if (!withMedia && (!m_prackEnabled || originalInvite->GetSDP(m_localMediaFormats) != NULL))
    SendInviteResponse(SIP_PDU::Information_Ringing);
  else {
    SDPSessionDescription sdpOut(m_sdpSessionId, ++m_sdpVersion, GetDefaultSDPConnectAddress());
    if (!OnSendAnswerSDP(m_rtpSessions, sdpOut)) {
      Release(EndedByCapabilityExchange);
      return PFalse;
    }
    if (!SendInviteResponse(SIP_PDU::Information_Session_Progress, &sdpOut))
      return PFalse;
  }

  SetPhase(AlertingPhase);
  NotifyDialogState(SIPDialogNotification::Early);

  return PTrue;
}


PBoolean SIPConnection::SetConnected()
{
  if (transport == NULL) {
    Release(EndedByTransportFail);
    return PFalse;
  }

  if (IsOriginating()) {
    PTRACE(2, "SIP\tSetConnected ignored on call we originated " << *this);
    return PTrue;
  }

  PSafeLockReadWrite safeLock(*this);
  if (!safeLock.IsLocked())
    return PFalse;
  
  if (GetPhase() >= ConnectedPhase) {
    PTRACE(2, "SIP\tSetConnected ignored on already connected call " << *this);
    return PFalse;
  }
  
  PTRACE(3, "SIP\tSetConnected " << *this);

  // send the 200 OK response
  if (!SendInviteOK()) {
    Release(EndedByCapabilityExchange);
    return false;
  }

  releaseMethod = ReleaseWithBYE;
  sessionTimer = 10000;

  NotifyDialogState(SIPDialogNotification::Confirmed);

  // switch phase and if media was previously set up, then move to Established
  return OpalConnection::SetConnected();
}


OpalMediaSession * SIPConnection::SetUpMediaSession(const unsigned rtpSessionId,
                                                    const OpalMediaType & mediaType,
                                                    const SDPMediaDescription & mediaDescription,
                                                    OpalTransportAddress & localAddress,
                                                    bool & remoteChanged)
{
  if (mediaDescription.GetPort() == 0) {
    PTRACE(2, "SIP\tReceived disabled/missing media description for " << mediaType);

    /* Some remotes return all of the media detail (a= lines) in SDP even though
       port is zero indicating the media is not to be used. So don't return these
       bogus media formats from SDP to the "remote media format list". */
    m_remoteFormatList.Remove(PString('@')+mediaType);
    return NULL;
  }

  OpalTransportAddress remoteMediaAddress = mediaDescription.GetTransportAddress();
  if (remoteMediaAddress.IsEmpty()) {
    PTRACE(2, "SIP\tReceived media description with no address for " << mediaType);
    remoteChanged = true;
  }

  if (ownerCall.IsMediaBypassPossible(*this, rtpSessionId)) {
    PSafePtr<OpalRTPConnection> otherParty = GetOtherPartyConnectionAs<OpalRTPConnection>();
    if (otherParty != NULL) {
      MediaInformation gatewayInfo;
      if (otherParty->GetMediaInformation(rtpSessionId, gatewayInfo)) {
        PTRACE(1, "SIP\tMedia bypass unimplemented for media type " << mediaType << " in session " << rtpSessionId);
        return NULL;
      }
    }
    else {
      PTRACE(2, "SIP\tCowardly refusing to media bypass with only one RTP connection");
    }
  }

  OpalMediaTypeDefinition * mediaDefinition = mediaType.GetDefinition();
  if (mediaDefinition == NULL) {
    PTRACE(1, "SIP\tUnknown media type " << mediaType << " in session " << rtpSessionId);
    return NULL;
  }

  if (!mediaDefinition->UsesRTP()) {
    OpalMediaSession * mediaSession = GetMediaSession(rtpSessionId);
    if (mediaSession == NULL) {
      mediaSession = mediaDefinition->CreateMediaSession(*this, rtpSessionId);
      if (mediaSession == NULL) {
        PTRACE(1, "SIP\tMedia definition cannot create session for " << mediaType);
        return NULL;
      }
      m_rtpSessions.AddMediaSession(mediaSession, mediaType);
    }

    if (!remoteMediaAddress.IsEmpty())
      mediaSession->SetRemoteMediaAddress(remoteMediaAddress, mediaDescription.GetMediaFormats());
    localAddress = mediaSession->GetLocalMediaAddress();
    return mediaSession;
  }

  // Create the RTPSession if required
  RTP_UDP *rtpSession = dynamic_cast<RTP_UDP *>(UseSession(GetTransport(), rtpSessionId, mediaType));
  if (rtpSession == NULL) {
    PTRACE(1, "SIP\tCannot create RTP session on non-bypassed connection");
    return NULL;
  }

  // Set user data
  rtpSession->SetUserData(new SIP_RTP_Session(*this));

  // Local Address of the session
  localAddress = GetDefaultSDPConnectAddress(rtpSession->GetLocalDataPort());

  if (!remoteMediaAddress.IsEmpty()) {
    PIPSocket::Address ip;
    WORD port = 0;
    if (!remoteMediaAddress.GetIpAndPort(ip, port)) {
      PTRACE(1, "SIP\tCannot get remote address/port for RTP session " << rtpSessionId);
      return NULL;
    }

    // see if remote socket information has changed
    bool remoteSet = rtpSession->GetRemoteDataPort() != 0 && rtpSession->GetRemoteAddress().IsValid();
    remoteChanged = remoteSet && ((rtpSession->GetRemoteAddress() != ip) ||
                                  (rtpSession->GetRemoteDataPort() != port));

    if (remoteChanged) {
      PTRACE(3, "SIP\tRemote changed IP address: "
             << rtpSession->GetRemoteAddress() << "!=" << ip
             << " || " << rtpSession->GetRemoteDataPort() << "!=" << port);
      ((OpalRTPEndPoint &)endpoint).CheckEndLocalRTP(*this, rtpSession);
    }

    if (remoteChanged || !remoteSet) {
      if (!rtpSession->SetRemoteSocketInfo(ip, port, true)) {
        PTRACE(1, "SIP\tCannot set remote ports on RTP session");
        return NULL;
      }
    }
  }

  return m_rtpSessions.GetMediaSession(rtpSessionId);
}


static bool SetNxECapabilities(OpalRFC2833Proto * handler,
                      const OpalMediaFormatList & localMediaFormats,
                      const OpalMediaFormatList & remoteMediaFormats,
                          const OpalMediaFormat & baseMediaFormat,
                            SDPMediaDescription * localMedia = NULL,
                    RTP_DataFrame::PayloadTypes   nxePayloadCode = RTP_DataFrame::IllegalPayloadType)
{
  OpalMediaFormatList::const_iterator remFmt = remoteMediaFormats.FindFormat(baseMediaFormat);
  if (remFmt == remoteMediaFormats.end()) {
    // Not in remote list, disable transmitter
    handler->SetTxMediaFormat(OpalMediaFormat());
    return false;
  }

  OpalMediaFormatList::const_iterator localFmt = localMediaFormats.FindFormat(baseMediaFormat);
  if (localFmt == localMediaFormats.end()) {
    // Not in our local list, disable transmitter
    handler->SetTxMediaFormat(OpalMediaFormat());
    return true;
  }

  // Merge remotes format into ours.
  // Note if this is our initial offer remote is the same as local.
  OpalMediaFormat adjustedFormat = *localFmt;
  adjustedFormat.Update(*remFmt);

  if (nxePayloadCode != RTP_DataFrame::IllegalPayloadType) {
    PTRACE(3, "SIP\tUsing bypass RTP payload " << nxePayloadCode << " for " << *localFmt);
    adjustedFormat.SetPayloadType(nxePayloadCode);
  }

  handler->SetTxMediaFormat(adjustedFormat);

  if (localMedia != NULL) {
    // Set the receive handler to what we are sending to remote in our SDP
    handler->SetRxMediaFormat(adjustedFormat);
    localMedia->AddSDPMediaFormat(new SDPMediaFormat(*localMedia, adjustedFormat));
  }

  return true;
}


PBoolean SIPConnection::OnSendOfferSDP(OpalRTPSessionManager & rtpSessions, SDPSessionDescription & sdpOut, bool offerCurrentOnly)
{
  bool sdpOK = false;

  if (offerCurrentOnly && !mediaStreams.IsEmpty()) {
    PTRACE(4, "SIP\tOffering only current media streams in Re-INVITE");
    std::vector<bool> sessions;
    for (OpalMediaStreamPtr stream(mediaStreams, PSafeReference); stream != NULL; ++stream) {
      std::vector<bool>::size_type session = stream->GetSessionID();
      sessions.resize(std::max(sessions.size(),session+1));
      if (!sessions[session]) {
        sessions[session] = true;
        if (OnSendOfferSDPSession(stream->GetMediaFormat().GetMediaType(), session, rtpSessions, sdpOut, true))
          sdpOK = true;
      }
    }
  }
  else {
    PTRACE(4, "SIP\tOffering all configured media:\n    " << setfill(',') << m_localMediaFormats << setfill(' '));

    // always offer audio first
    sdpOK = OnSendOfferSDPSession(OpalMediaType::Audio(), 0, rtpSessions, sdpOut, false);

#if OPAL_VIDEO
    // always offer video second (if enabled)
    if (OnSendOfferSDPSession(OpalMediaType::Video(), 0, rtpSessions, sdpOut, false))
      sdpOK = true;
#endif

    // offer other formats
    OpalMediaTypeFactory::KeyList_T mediaTypes = OpalMediaType::GetList();
    for (OpalMediaTypeFactory::KeyList_T::iterator iter = mediaTypes.begin(); iter != mediaTypes.end(); ++iter) {
      OpalMediaType mediaType = *iter;
      if (mediaType != OpalMediaType::Video() && mediaType != OpalMediaType::Audio()) {
        if (OnSendOfferSDPSession(mediaType, 0, rtpSessions, sdpOut, false))
          sdpOK = true;
      }
    }
  }

  return sdpOK && !sdpOut.GetMediaDescriptions().IsEmpty();
}


bool SIPConnection::OnSendOfferSDPSession(const OpalMediaType & mediaType,
                                                     unsigned   rtpSessionId,
                                        OpalRTPSessionManager & rtpSessions,
                                        SDPSessionDescription & sdp,
                                                         bool   offerOpenMediaStreamOnly)
{
  OpalMediaType::AutoStartMode autoStart = GetAutoStart(mediaType);
  if (rtpSessionId == 0 && autoStart == OpalMediaType::DontOffer)
    return false;

  // See if any media formats of this session id, so don't create unused RTP session
  if (!m_localMediaFormats.HasType(mediaType)) {
    PTRACE(3, "SIP\tNo media formats of type " << mediaType << ", not adding SDP");
    return PFalse;
  }

  PTRACE(3, "SIP\tOffering media type " << mediaType << " in SDP");

  if (rtpSessionId == 0)
    rtpSessionId = sdp.GetMediaDescriptions().GetSize()+1;

  MediaInformation gatewayInfo;
  if (ownerCall.IsMediaBypassPossible(*this, rtpSessionId)) {
    PSafePtr<OpalRTPConnection> otherParty = GetOtherPartyConnectionAs<OpalRTPConnection>();
    if (otherParty != NULL)
      otherParty->GetMediaInformation(rtpSessionId, gatewayInfo);
  }

  OpalMediaSession * mediaSession = rtpSessions.GetMediaSession(rtpSessionId);

  OpalTransportAddress sdpContactAddress;

  if (!gatewayInfo.data.IsEmpty())
    sdpContactAddress = gatewayInfo.data;
  else {

    /* We are not doing media bypass, so must have some media session
       Due to the possibility of several INVITEs going out, all with different
       transport requirements, we actually need to use an rtpSession dictionary
       for each INVITE and not the one for the connection. Once an INVITE is
       accepted the rtpSessions for that INVITE is put into the connection. */
    // need different handling for RTP and non-RTP sessions
    if (!mediaType.GetDefinition()->UsesRTP()) {
      if (mediaSession == NULL) {
        mediaSession = mediaType.GetDefinition()->CreateMediaSession(*this, rtpSessionId);
        if (mediaSession != NULL)
          rtpSessions.AddMediaSession(mediaSession, mediaType);
      }
      if (mediaSession != NULL)
        sdpContactAddress = mediaSession->GetLocalMediaAddress();
    }

    else {
      RTP_Session * rtpSession = rtpSessions.GetSession(rtpSessionId);
      if (rtpSession == NULL) {

        // Not already there, so create one
        rtpSession = CreateSession(GetTransport(), rtpSessionId, mediaType, NULL);
        if (rtpSession == NULL) {
          PTRACE(1, "SIP\tCould not create RTP session " << rtpSessionId << " for media type " << mediaType << ", released " << *this);
          Release(OpalConnection::EndedByTransportFail);
          return PFalse;
        }

        rtpSession->SetUserData(new SIP_RTP_Session(*this));

        // add the RTP session to the RTP session manager in INVITE
        rtpSessions.AddSession(rtpSession, mediaType);

        mediaSession = rtpSessions.GetMediaSession(rtpSessionId);
        PAssert(mediaSession != NULL, "cannot retrieve newly added RTP session");
      }
      sdpContactAddress = GetDefaultSDPConnectAddress(((RTP_UDP *)rtpSession)->GetLocalDataPort());
    }
  }

  if (sdpContactAddress.IsEmpty()) {
    PTRACE(2, "SIP\tRefusing to add SDP media description for session id " << rtpSessionId << " with no transport address");
    return false;
  }

  if (mediaSession == NULL) {
    PTRACE(1, "SIP\tCould not create media session " << rtpSessionId << " for media type " << mediaType << ", released " << *this);
    Release(OpalConnection::EndedByTransportFail);
    return PFalse;
  }

  SDPMediaDescription * localMedia = mediaSession->CreateSDPMediaDescription(sdpContactAddress);
  if (localMedia == NULL) {
    PTRACE(2, "SIP\tCan't create SDP media description for media type " << mediaType);
    return false;
  }

  if (sdp.GetDefaultConnectAddress().IsEmpty())
    sdp.SetDefaultConnectAddress(sdpContactAddress);

  if (offerOpenMediaStreamOnly) {
    OpalMediaStreamPtr recvStream = GetMediaStream(rtpSessionId, true);
    OpalMediaStreamPtr sendStream = GetMediaStream(rtpSessionId, false);
    if (recvStream != NULL)
      localMedia->AddMediaFormat(*m_localMediaFormats.FindFormat(recvStream->GetMediaFormat()));
    else if (sendStream != NULL)
      localMedia->AddMediaFormat(sendStream->GetMediaFormat());
    else
      localMedia->AddMediaFormats(m_localMediaFormats, mediaType);

    bool sending = sendStream != NULL && sendStream->IsOpen() && !sendStream->IsPaused();
    if (sending && m_holdFromRemote) {
      // OK we have (possibly) asymmetric hold, check if remote supports it.
      PString regex = m_stringOptions(OPAL_OPT_SYMMETRIC_HOLD_PRODUCT);
      if (regex.IsEmpty() || remoteProductInfo.AsString().FindRegEx(regex) == P_MAX_INDEX)
        sending = false;
    }
    bool recving = m_holdToRemote < eHoldOn && recvStream != NULL && recvStream->IsOpen();

    if (sending && recving)
      localMedia->SetDirection(SDPMediaDescription::SendRecv);
    else if (recving)
      localMedia->SetDirection(SDPMediaDescription::RecvOnly);
    else if (sending)
      localMedia->SetDirection(SDPMediaDescription::SendOnly);
    else
      localMedia->SetDirection(SDPMediaDescription::Inactive);

#if PAUSE_WITH_EMPTY_ADDRESS
    if (m_holdToRemote >= eHoldOn) {
      OpalTransportAddress addr = localMedia->GetTransportAddress();
      PIPSocket::Address dummy;
      WORD port;
      addr.GetIpAndPort(dummy, port);
      OpalTransportAddress newAddr("0.0.0.0", port, addr.GetProto());
      localMedia->SetTransportAddress(newAddr);
      localMedia->SetDirection(SDPMediaDescription::Undefined);
      sdp.SetDefaultConnectAddress(newAddr);
    }
#endif
  }
  else {
    localMedia->AddMediaFormats(m_localMediaFormats, mediaType);
    localMedia->SetDirection((SDPMediaDescription::Direction)autoStart);
  }

  if (mediaType == OpalMediaType::Audio()) {
    SDPAudioMediaDescription * audioMedia = dynamic_cast<SDPAudioMediaDescription *>(localMedia);
    if (audioMedia != NULL)
      audioMedia->SetOfferPTime(m_stringOptions.GetBoolean(OPAL_OPT_OFFER_SDP_PTIME));

    // Set format if we have an RTP payload type for RFC2833 and/or NSE
    // Must be after other codecs, as Mediatrix gateways barf if RFC2833 is first
    SetNxECapabilities(rfc2833Handler, m_localMediaFormats, m_remoteFormatList, OpalRFC2833, localMedia, gatewayInfo.rfc2833);
#if OPAL_T38_CAPABILITY
    SetNxECapabilities(ciscoNSEHandler, m_localMediaFormats, m_remoteFormatList, OpalCiscoNSE, localMedia, gatewayInfo.ciscoNSE);
#endif
  }

  sdp.AddMediaDescription(localMedia);

  return true;
}


static bool PauseOrCloseMediaStream(OpalMediaStreamPtr & stream,
                                    const OpalMediaFormatList & answerFormats,
                                    bool remoteChanged,
                                    bool paused)
{
  if (stream == NULL)
    return false;

  if (!stream->IsOpen())
    return false;

  if (!remoteChanged) {
    OpalMediaFormatList::const_iterator fmt = answerFormats.FindFormat(stream->GetMediaFormat());
    if (fmt != answerFormats.end() && stream->UpdateMediaFormat(*fmt)) {
      PTRACE(4, "SIP\tINVITE change needs to " << (paused ? "pause" : "resume") << " stream " << *stream);
      stream->SetPaused(paused);
      return !paused;
    }
  }

  PTRACE(4, "SIP\tRe-INVITE needs to close stream " << *stream);
  stream->GetPatch()->GetSource().Close();
  stream.SetNULL();
  return false;
}


bool SIPConnection::OnSendAnswerSDP(OpalRTPSessionManager & rtpSessions, SDPSessionDescription & sdpOut)
{
  if (!PAssert(originalInvite != NULL, PLogicError))
    return false;

  SDPSessionDescription * sdp = originalInvite->GetSDP(m_localMediaFormats);

  /* If we had SDP but no media could not be decoded from it, then we should return
     Not Acceptable Here error and not do an offer. Only offer if there was no body
     at all or there was a valid SDP with no m lines. */
  if (sdp == NULL && !originalInvite->GetEntityBody().IsEmpty())
    return false;

  if (sdp == NULL || sdp->GetMediaDescriptions().IsEmpty()) {
    // They did not offer anything, so it behooves us to do so: RFC 3261, para 14.2
    PTRACE(3, "SIP\tRemote did not offer media, so we will.");
    return OnSendOfferSDP(rtpSessions, sdpOut, false);
  }

  // The Re-INVITE can be sent to change the RTP Session parameters,
  // the current codecs, or to put the call on hold
  bool holdFromRemote = sdp->IsHold();
  if (m_holdFromRemote != holdFromRemote) {
    PTRACE(3, "SIP\tRemote " << (holdFromRemote ? "" : "retrieve from ") << "hold detected");
    m_holdFromRemote = holdFromRemote;
    OnHold(true, holdFromRemote);
  }

  // get the remote media formats
  m_answerFormatList = sdp->GetMediaFormats();

  // Remove anything we never offerred
  while (!m_answerFormatList.IsEmpty() && m_localMediaFormats.FindFormat(m_answerFormatList.front()) == m_localMediaFormats.end())
    m_answerFormatList.RemoveAt(0);

  AdjustMediaFormats(false, NULL, m_answerFormatList);
  if (m_answerFormatList.IsEmpty()) {
    PTRACE(3, "SIP\tAll media formats offered by remote have been removed.");
    return false;
  }

  bool sdpOK = false;
  unsigned sessionCount = sdp->GetMediaDescriptions().GetSize();

  vector<bool> goodSession(sessionCount+1);
  for (unsigned session = 1; session <= sessionCount; ++session) {
    if (OnSendAnswerSDPSession(*sdp, session, sdpOut)) {
      sdpOK = true;
      goodSession[session] = true;
    }
    else {
      SDPMediaDescription * incomingMedia = sdp->GetMediaDescriptionByIndex(session);
      if (PAssert(incomingMedia != NULL, "SDP Media description list changed")) {
        SDPMediaDescription * outgoingMedia = incomingMedia->CreateEmpty();
        if (PAssert(outgoingMedia != NULL, "SDP Media description clone failed")) {
          if (incomingMedia->GetSDPMediaFormats().IsEmpty())
            outgoingMedia->AddSDPMediaFormat(new SDPMediaFormat(*incomingMedia, OpalG711_ULAW_64K));
          else
            outgoingMedia->AddSDPMediaFormat(new SDPMediaFormat(incomingMedia->GetSDPMediaFormats().front()));
          sdpOut.AddMediaDescription(outgoingMedia);
        }
      }
    }
  }

  if (sdpOK) {
    /* Shut down any media that is in a session not mentioned in a re-INVITE.
       While the SIP/SDP specification says this shouldn't happen, it does
       anyway so we need to deal. */
    for (OpalMediaStreamPtr stream(mediaStreams, PSafeReference); stream != NULL; ++stream) {
      unsigned session = stream->GetSessionID();
      if (session > sessionCount || !goodSession[session])
        stream->Close();
    }

    // In case some new streams got created.
    ownerCall.StartMediaStreams();
  }

  return sdpOK;
}


bool SIPConnection::OnSendAnswerSDPSession(const SDPSessionDescription & sdpIn,
                                                              unsigned   rtpSessionId,
                                                 SDPSessionDescription & sdpOut)
{
  PSafePtr<OpalConnection> otherParty = GetOtherPartyConnection();
  if (otherParty == NULL)
    return false;

  SDPMediaDescription * incomingMedia = sdpIn.GetMediaDescriptionByIndex(rtpSessionId);
  if (!PAssert(incomingMedia != NULL, "SDP Media description list changed"))
    return false;

  OpalMediaType mediaType = incomingMedia->GetMediaType();

  // See if any media formats of this session id, so don't create unused RTP session
  if (!m_localMediaFormats.HasType(mediaType)) {
    PTRACE(3, "SIP\tNo media formats of type " << mediaType << ", not adding SDP");
    return false;
  }

  OpalTransportAddress localAddress;
  bool remoteChanged = false;
  OpalMediaSession * mediaSession = SetUpMediaSession(rtpSessionId, mediaType, *incomingMedia, localAddress, remoteChanged);
  if (mediaSession == NULL)
    return false;

#if OPAL_T38_CAPABILITY
  if (mediaType == OpalMediaType::Fax()) {
    if (!otherParty->OnSwitchingFaxMediaStreams(true)) {
      PTRACE(2, "SIP\tSwitch to T.38 refused for " << *this);
      return false;
    }
  }
  else if (mediaSession->mediaType == OpalMediaType::Fax()) {
    if (!otherParty->OnSwitchingFaxMediaStreams(false)) {
      PTRACE(2, "SIP\tSwitch from T.38 refused for " << *this);
      return false;
    }
  }
#endif // OPAL_T38_CAPABILITY

  // For fax we have to translate the media type
  mediaSession->mediaType = mediaType;

  SDPMediaDescription * localMedia = NULL;

  if (!m_answerFormatList.HasType(mediaType)) {
    PTRACE(1, "SIP\tNo available media formats in SDP media description for session " << rtpSessionId);
    // Send back a m= line with port value zero and the first entry of the offer payload types as per RFC3264
    localMedia = mediaSession->CreateSDPMediaDescription(OpalTransportAddress());
    if (localMedia  == NULL) {
      PTRACE(1, "SIP\tCould not create SDP media description for media type " << mediaType);
      return false;
    }
    if (!incomingMedia->GetSDPMediaFormats().IsEmpty())
      localMedia->AddSDPMediaFormat(new SDPMediaFormat(incomingMedia->GetSDPMediaFormats().front()));
    sdpOut.AddMediaDescription(localMedia);
    return false;
  }
  
  // construct a new media session list 
  if ((localMedia = mediaSession->CreateSDPMediaDescription(localAddress)) == NULL) {
    PTRACE(1, "SIP\tCould not create SDP media description for media type " << mediaType);
    return false;
  }

  PTRACE(4, "SIP\tAnswering offer for media type " << mediaType);

  SDPMediaDescription::Direction otherSidesDir = sdpIn.GetDirection(rtpSessionId);
  if (GetPhase() < ConnectedPhase) {
    // If processing initial INVITE and video, obey the auto-start flags
    OpalMediaType::AutoStartMode autoStart = GetAutoStart(mediaType);
    if ((autoStart&OpalMediaType::Transmit) == 0)
      otherSidesDir = (otherSidesDir&SDPMediaDescription::SendOnly) != 0 ? SDPMediaDescription::SendOnly : SDPMediaDescription::Inactive;
    if ((autoStart&OpalMediaType::Receive) == 0)
      otherSidesDir = (otherSidesDir&SDPMediaDescription::RecvOnly) != 0 ? SDPMediaDescription::RecvOnly : SDPMediaDescription::Inactive;
  }

  SDPMediaDescription::Direction newDirection = SDPMediaDescription::Inactive;

  // Check if we had a stream and the remote has either changed the codec or
  // changed the direction of the stream
  OpalMediaStreamPtr sendStream = GetMediaStream(rtpSessionId, false);
  if (PauseOrCloseMediaStream(sendStream, m_answerFormatList, remoteChanged, (otherSidesDir&SDPMediaDescription::RecvOnly) == 0))
    newDirection = SDPMediaDescription::SendOnly;

  OpalMediaStreamPtr recvStream = GetMediaStream(rtpSessionId, true);
  if (PauseOrCloseMediaStream(recvStream, m_answerFormatList, remoteChanged,
                              m_holdToRemote >= eHoldOn && (otherSidesDir&SDPMediaDescription::SendOnly) == 0))
    newDirection = newDirection != SDPMediaDescription::Inactive ? SDPMediaDescription::SendRecv : SDPMediaDescription::RecvOnly;

  /* After (possibly) closing streams, we now open them again if necessary,
     OpenSourceMediaStreams will just return true if they are already open.
     We open tx (other party source) side first so we follow the remote
     endpoints preferences. */
  if (!incomingMedia->GetTransportAddress().IsEmpty()) {
    if (sendStream == NULL) {
      PTRACE(5, "SIP\tOpening tx " << mediaType << " stream from SDP");
      if (ownerCall.OpenSourceMediaStreams(*otherParty, mediaType, rtpSessionId)) {
        sendStream = GetMediaStream(rtpSessionId, false);

        if (sendStream != NULL && (otherSidesDir&SDPMediaDescription::RecvOnly) != 0)
          newDirection = newDirection != SDPMediaDescription::Inactive ? SDPMediaDescription::SendRecv
                                                                       : SDPMediaDescription::SendOnly;
      }
    }

    if (sendStream != NULL) {
      sendStream->UpdateMediaFormat(*m_answerFormatList.FindFormat(sendStream->GetMediaFormat()));
      sendStream->SetPaused((otherSidesDir&SDPMediaDescription::RecvOnly) == 0);
    }

    if (recvStream == NULL) {
      PTRACE(5, "SIP\tOpening rx " << mediaType << " stream from SDP");
      if (ownerCall.OpenSourceMediaStreams(*this, mediaType, rtpSessionId)) {
        recvStream = GetMediaStream(rtpSessionId, true);
        if (recvStream != NULL && (otherSidesDir&SDPMediaDescription::SendOnly) != 0)
          newDirection = newDirection != SDPMediaDescription::Inactive ? SDPMediaDescription::SendRecv
                                                                       : SDPMediaDescription::RecvOnly;
      }
    }

    if (recvStream != NULL) {
      OpalMediaFormat adjustedMediaFormat = *m_answerFormatList.FindFormat(recvStream->GetMediaFormat());

      // If we are sendrecv we will receive the same payload type as we transmit.
      if (newDirection == SDPMediaDescription::SendRecv)
        adjustedMediaFormat.SetPayloadType(sendStream->GetMediaFormat().GetPayloadType());

      recvStream->UpdateMediaFormat(adjustedMediaFormat);
      recvStream->SetPaused((otherSidesDir&SDPMediaDescription::SendOnly) == 0);
    }
  }

  // Now we build the reply, setting "direction" as appropriate for what we opened.
  localMedia->SetDirection(newDirection);
  if (sendStream != NULL)
    localMedia->AddMediaFormat(sendStream->GetMediaFormat());
  else if (recvStream != NULL)
    localMedia->AddMediaFormat(recvStream->GetMediaFormat());
  else {
    // Add all possible formats
    bool empty = true;
    for (OpalMediaFormatList::iterator remoteFormat = m_remoteFormatList.begin(); remoteFormat != m_remoteFormatList.end(); ++remoteFormat) {
      if (remoteFormat->GetMediaType() == mediaType) {
        for (OpalMediaFormatList::iterator localFormat = m_localMediaFormats.begin(); localFormat != m_localMediaFormats.end(); ++localFormat) {
          if (localFormat->GetMediaType() == mediaType) {
            OpalMediaFormat intermediateFormat;
            if (OpalTranscoder::FindIntermediateFormat(*localFormat, *remoteFormat, intermediateFormat)) {
              localMedia->AddMediaFormat(*remoteFormat);
              empty = false;
              break;
            }
          }
        }
      }
    }

    // RFC3264 says we MUST have an entry, but it should have port zero
    if (empty) {
      localMedia->AddMediaFormat(m_answerFormatList.front());
      localMedia->SetTransportAddress(OpalTransportAddress());
    }
    else {
      // We can do the media type but choose not to at this time
      localMedia->SetDirection(SDPMediaDescription::Inactive);
    }
  }

  if (mediaType == OpalMediaType::Audio()) {
    // Set format if we have an RTP payload type for RFC2833 and/or NSE
    SetNxECapabilities(rfc2833Handler, m_localMediaFormats, m_answerFormatList, OpalRFC2833, localMedia);
#if OPAL_T38_CAPABILITY
    SetNxECapabilities(ciscoNSEHandler, m_localMediaFormats, m_answerFormatList, OpalCiscoNSE, localMedia);
#endif
  }

  sdpOut.AddMediaDescription(localMedia);

  return true;
}


OpalTransportAddress SIPConnection::GetDefaultSDPConnectAddress(WORD port) const
{
  PIPSocket::Address localIP;
  if (!transport->GetLocalAddress().GetIpAddress(localIP)) {
    PTRACE(1, "SIP\tNot using IP transport");
    return OpalTransportAddress();
  }

  PIPSocket::Address remoteIP;
  if (!transport->GetRemoteAddress().GetIpAddress(remoteIP)) {
    PTRACE(1, "SIP\tNot using IP transport");
    return OpalTransportAddress();
  }

  endpoint.GetManager().TranslateIPAddress(localIP, remoteIP);
  return OpalTransportAddress(localIP, port, transport->GetProtoPrefix());
}


OpalMediaFormatList SIPConnection::GetMediaFormats() const
{
  // Need to limit the media formats to what the other side provided in a re-INVITE
  if (m_answerFormatList.IsEmpty()) {
    PTRACE(4, "SIP\tUsing remote media format list");
    return m_remoteFormatList;
  }
  else {
    PTRACE(4, "SIP\tUsing offered media format list");
    return m_answerFormatList;
  }
}


bool SIPConnection::SetRemoteMediaFormats(SDPSessionDescription * sdp)
{
  /* As SIP does not really do capability exchange, if we don't have an initial
     INVITE from the remote (indicated by sdp == NULL) then all we can do is
     assume the the remote can at do what we can do. We could assume it does
     everything we know about, but there is no point in assuming it can do any
     more than we can, really.
     */
  if (sdp == NULL) {
    m_remoteFormatList = GetLocalMediaFormats();
    m_remoteFormatList.MakeUnique();
#if OPAL_FAX
    m_remoteFormatList += OpalT38; // Assume remote can do it.
#endif
  }
  else {
    m_remoteFormatList = sdp->GetMediaFormats();
    AdjustMediaFormats(false, NULL, m_remoteFormatList);
  }

  if (m_remoteFormatList.IsEmpty()) {
    PTRACE(2, "SIP\tAll possible media formats to offer were removed.");
   return false;
  }

  PTRACE(4, "SIP\tRemote media formats set:\n    " << setfill(',') << m_remoteFormatList << setfill(' '));
  return true;
}


OpalMediaStreamPtr SIPConnection::OpenMediaStream(const OpalMediaFormat & mediaFormat, unsigned sessionID, bool isSource)
{
  if (m_holdFromRemote && !isSource && !m_handlingINVITE) {
    PTRACE(3, "SIP\tCannot start media stream as are currently in HOLD by remote.");
    return NULL;
  }

  // Make sure stream is symmetrical, if codec changed, close and re-open it
  OpalMediaStreamPtr otherStream = GetMediaStream(sessionID, !isSource);
  bool makesymmetrical = !m_symmetricOpenStream &&
                          otherStream != NULL &&
                          otherStream->IsOpen() &&
                          otherStream->GetMediaFormat() != mediaFormat;
  if (makesymmetrical) {
    m_symmetricOpenStream = true;
    // We must make sure reverse stream is closed before opening the
    // new forward one or can really confuse the RTP stack, especially
    // if switching to udptl in fax mode
    if (isSource) {
      OpalMediaPatch * patch = otherStream->GetPatch();
      if (patch != NULL)
        patch->GetSource().Close();
    }
    else
      otherStream->Close();
    m_symmetricOpenStream = false;
  }

  OpalMediaStreamPtr oldStream = GetMediaStream(sessionID, isSource);

  // Open forward side
  OpalMediaStreamPtr newStream = OpalRTPConnection::OpenMediaStream(mediaFormat, sessionID, isSource);
  if (newStream == NULL)
    return newStream;

  // Open other direction, if needed (must be after above open)
  if (makesymmetrical) {
    m_symmetricOpenStream = true;

    PSafePtr<OpalConnection> otherConnection = isSource ? GetCall().GetOtherPartyConnection(*this) : this;
    bool ok = false;
    if (otherConnection != NULL)
      ok = GetCall().OpenSourceMediaStreams(*otherConnection, mediaFormat.GetMediaType(), sessionID, mediaFormat);

    m_symmetricOpenStream = false;

    if (!ok) {
      newStream->Close();
      return NULL;
    }
  }

  if (!m_symmetricOpenStream && !m_handlingINVITE && GetPhase() == EstablishedPhase &&
              (newStream != oldStream || GetMediaStream(sessionID, !isSource) != otherStream))
    SendReINVITE(PTRACE_PARAM("open channel"));

  return newStream;
}


bool SIPConnection::CloseMediaStream(OpalMediaStream & stream)
{
  bool closed = OpalConnection::CloseMediaStream(stream);

  if (!m_symmetricOpenStream && !m_handlingINVITE && GetPhase() == EstablishedPhase)
    closed = SendReINVITE(PTRACE_PARAM("close channel")) && closed;

  return closed;
}


void SIPConnection::OnPauseMediaStream(OpalMediaStream & strm, bool paused)
{
  /* If we have pasued the transmit RTP, we really need to tell the other side
     via a re-INVITE or some systems disconnect the call becuase they have not
     received any RTP for too long. */
  if (!m_symmetricOpenStream && !m_handlingINVITE && strm.IsSink())
    SendReINVITE(PTRACE_PARAM(paused ? "pausing channel" : "resume channel"));
  OpalConnection::OnPauseMediaStream(strm, paused);
}


bool SIPConnection::SendReINVITE(PTRACE_PARAM(const char * msg))
{
  bool startImmediate = !m_handlingINVITE && pendingInvitations.IsEmpty();

  PTRACE(3, "SIP\t" << (startImmediate ? "Start" : "Queue") << "ing re-INVITE to " << msg);

  m_needReINVITE = true;

  SIPTransaction * invite = new SIPInvite(*this, m_rtpSessions);

  // To avoid overlapping INVITE transactions, we place the new transaction
  // in a queue, if queue is empty we can start immediately, otherwise
  // it waits till we get a response.
  if (startImmediate) {
    if (!invite->Start())
      return false;
    m_handlingINVITE = true;
  }

  pendingInvitations.Append(invite);
  return true;
}


bool SIPConnection::StartPendingReINVITE()
{
  while (!pendingInvitations.IsEmpty()) {
    PSafePtr<SIPTransaction> reInvite = pendingInvitations.GetAt(0, PSafeReadWrite);
    if (reInvite->IsInProgress())
      break;

    if (!reInvite->IsCompleted()) {
      if (reInvite->Start()) {
        m_handlingINVITE = true;
        return true;
      }
    }

    pendingInvitations.RemoveAt(0);
  }

  return false;
}


PBoolean SIPConnection::WriteINVITE(OpalTransport &, void * param)
{
  return ((SIPConnection *)param)->WriteINVITE();
}


bool SIPConnection::WriteINVITE()
{
  const SIPURL & requestURI = m_dialog.GetRequestURI();
  SIPURL myAddress = m_stringOptions(OPAL_OPT_CALLING_PARTY_URL);
  if (myAddress.IsEmpty())
    myAddress = endpoint.GetRegisteredPartyName(requestURI, *transport);

  PString transportProtocol = requestURI.GetParamVars()("transport");
  if (!transportProtocol.IsEmpty())
    myAddress.SetParamVar("transport", transportProtocol);

  bool changedUserName = false;
  if (IsOriginating()) {
    // only allow override of calling party number if the local party
    // name hasn't been first specified by a register handler. i.e a
    // register handler's target number is always used
    changedUserName = m_stringOptions.Contains(OPAL_OPT_CALLING_PARTY_NUMBER);
    if (changedUserName)
      myAddress.SetUserName(m_stringOptions[OPAL_OPT_CALLING_PARTY_NUMBER]);
    else {
      changedUserName = m_stringOptions.Contains(OPAL_OPT_CALLING_PARTY_NAME);
      if (changedUserName)
        myAddress.SetUserName(m_stringOptions[OPAL_OPT_CALLING_PARTY_NAME]);
    }
  }
  else {
    changedUserName = m_stringOptions.Contains(OPAL_OPT_CALLED_PARTY_NAME);
    if (changedUserName)
      myAddress.SetUserName(m_stringOptions[OPAL_OPT_CALLED_PARTY_NAME]);
  }

  bool changedDisplayName = myAddress.GetDisplayName() != GetDisplayName();
  if (changedDisplayName)
    myAddress.SetDisplayName(GetDisplayName());

  // Domain cannot be an empty string so do not set if override is empty
  {
    PString domain(m_stringOptions(OPAL_OPT_CALLING_PARTY_DOMAIN));
    if (!domain.IsEmpty())
      myAddress.SetHostName(domain);
  }

  // Tag must be set to token or the whole house of cards falls down
  myAddress.SetTag(GetToken(), true);
  m_dialog.SetLocalURI(myAddress);

  NotifyDialogState(SIPDialogNotification::Trying);

  m_needReINVITE = false;
  SIPTransaction * invite = new SIPInvite(*this, OpalRTPSessionManager(*this));

  if (!m_stringOptions.Contains(SIP_HEADER_CONTACT) && (changedUserName || changedDisplayName)) {
    SIPMIMEInfo & mime = invite->GetMIME();
    SIPURL contact = mime.GetContact();
    if (changedUserName)
      contact.SetUserName(myAddress.GetUserName());
    if (changedDisplayName)
      contact.SetDisplayName(myAddress.GetDisplayName());
    mime.SetContact(contact.AsQuotedString());
  }

  SIPURL redir(m_stringOptions(OPAL_OPT_REDIRECTING_PARTY, m_redirectingParty));
  if (!redir.IsEmpty())
    invite->GetMIME().SetReferredBy(redir.AsQuotedString());

  invite->GetMIME().SetAlertInfo(m_alertInfo, m_appearanceCode);

  // It may happen that constructing the INVITE causes the connection
  // to be released (e.g. there are no UDP ports available for the RTP sessions)
  // Since the connection is released immediately, a INVITE must not be
  // sent out.
  if (IsReleased()) {
    PTRACE(2, "SIP\tAborting INVITE transaction since connection is in releasing phase");
    delete invite; // Before Start() is called we are responsible for deletion
    return PFalse;
  }

  if (invite->Start()) {
    forkedInvitations.Append(invite);
    return PTrue;
  }

  PTRACE(2, "SIP\tDid not start INVITE transaction on " << transport);
  return PFalse;
}


PBoolean SIPConnection::SetUpConnection()
{
  PTRACE(3, "SIP\tSetUpConnection: " << m_dialog.GetRequestURI());

  originating = true;

  OnApplyStringOptions();

  if (m_stringOptions.Contains(SIP_HEADER_PREFIX"Route")) {
    SIPMIMEInfo mime;
    mime.SetRoute(m_stringOptions[SIP_HEADER_PREFIX"Route"]);
    m_dialog.SetRouteSet(mime.GetRoute());
  }

  SIPURL transportAddress;

  if (!m_dialog.GetRouteSet().empty()) 
    transportAddress = m_dialog.GetRouteSet().front();
  else if (!m_dialog.GetProxy().IsEmpty()) {
    SIPURL proxy_url;
    proxy_url = m_dialog.GetProxy();
    proxy_url.AdjustToDNS(endpoint);
    //transportAddress = m_dialog.GetProxy().GetHostAddress();
    transportAddress = proxy_url.GetHostAddress();
    PTRACE(4, "SIP\tConnecting to (with proxy)" << m_dialog.GetRequestURI() << " via " << transportAddress);
  }
  else {
    transportAddress = m_dialog.GetRequestURI();
    transportAddress.AdjustToDNS(endpoint); // Do a DNS SRV lookup
    PTRACE(4, "SIP\tConnecting to " << m_dialog.GetRequestURI() << " via " << transportAddress);
  }

  if (!SetTransport(transportAddress)) {
    Release(EndedByUnreachable);
    return false;
  }

  ++m_sdpVersion;

  if (!SetRemoteMediaFormats(NULL))
    return false;

  bool ok;
  if (!transport->GetInterface().IsEmpty())
    ok = WriteINVITE();
  else {
    PWaitAndSignal mutex(transport->GetWriteMutex());
    m_dialog.SetForking(true);
    ok = transport->WriteConnect(WriteINVITE, this);
    m_dialog.SetForking(false);
  }

  SetPhase(SetUpPhase);

  if (ok) {
    releaseMethod = ReleaseWithCANCEL;
    m_handlingINVITE = true;
    return true;
  }

  PTRACE(1, "SIP\tCould not write to " << transportAddress << " - " << transport->GetErrorText());
  Release(EndedByTransportFail);
  return false;
}


PString SIPConnection::GetDestinationAddress()
{
  return originalInvite != NULL ? originalInvite->GetURI().AsString() : OpalConnection::GetDestinationAddress();
}


PString SIPConnection::GetCalledPartyURL()
{
  if (!originating && originalInvite != NULL)
    return originalInvite->GetURI().AsString();

  SIPURL calledParty = m_dialog.GetRequestURI();
  calledParty.Sanitise(SIPURL::ExternalURI);
  return calledParty.AsString();
}


PString SIPConnection::GetAlertingType() const
{
  return m_alertInfo;
}


bool SIPConnection::SetAlertingType(const PString & info)
{
  m_alertInfo = info;
  return true;
}


PString SIPConnection::GetCallInfo() const
{
  return originalInvite != NULL ? originalInvite->GetMIME().GetCallInfo() : PString::Empty();
}


bool SIPConnection::Hold(bool fromRemote, bool placeOnHold)
{
  if (transport == NULL)
    return false;

#if PTRACING
  const char * holdStr = placeOnHold ? "on" : "off";
#endif

  if (fromRemote) {
    if (m_holdFromRemote == placeOnHold) {
      PTRACE(4, "SIP\tHold " << holdStr << " request ignored as already set on " << *this);
      return true;
    }
    m_holdFromRemote = placeOnHold;
    if (SendReINVITE(PTRACE_PARAM(placeOnHold ? "break remote hold" : "request remote hold")))
      return true;
    m_holdFromRemote = !placeOnHold;
    return false;
  }

  switch (m_holdToRemote) {
    case eHoldOff :
      if (!placeOnHold) {
        PTRACE(4, "SIP\tHold off request ignored as not in hold on " << *this);
        return true;
      }
      break;

    case eHoldOn :
      if (placeOnHold) {
        PTRACE(4, "SIP\tHold on request ignored as already in hold on " << *this);
        return true;
      }
      break;

    default :
      PTRACE(4, "SIP\tHold " << holdStr << " request ignored as in progress on " << *this);
      return false;
  }


  HoldState origState = m_holdToRemote;
  m_holdToRemote = placeOnHold ? eHoldInProgress : eRetrieveInProgress;

  if (SendReINVITE(PTRACE_PARAM(placeOnHold ? "put connection on hold" : "retrieve connection from hold")))
    return true;

  m_holdToRemote = origState;
  return false;
}


PBoolean SIPConnection::IsOnHold(bool fromRemote)
{
  return fromRemote ? m_holdFromRemote : (m_holdToRemote >= eHoldOn);
}


PString SIPConnection::GetPrefixName() const
{
  return m_dialog.GetRequestURI().GetScheme();
}


PString SIPConnection::GetIdentifier() const
{
  return m_dialog.GetCallID();
}


void SIPConnection::OnTransactionFailed(SIPTransaction & transaction)
{
  PTRACE(4, "SIP\tOnTransactionFailed for transaction id=" << transaction.GetTransactionID());

  std::map<std::string, SIP_PDU *>::iterator it = m_responses.find(transaction.GetTransactionID());
  if (it != m_responses.end()) {
    it->second->SetStatusCode(transaction.GetStatusCode());
    m_responses.erase(it);
  }

  switch (transaction.GetMethod()) {
    case SIP_PDU::Method_INVITE :
      break;

    case SIP_PDU::Method_REFER :
      m_referInProgress = false;
      // Do next case

    default :
      return;
  }

  m_handlingINVITE = false;

  // If we are releasing then I can safely ignore failed
  // transactions - otherwise I'll deadlock.
  if (IsReleased())
    return;

  PTRACE(4, "SIP\tChecking for all forked INVITEs failing.");
  bool allFailed = true;
  {
    // The connection stays alive unless all INVITEs have failed
    PSafePtr<SIPTransaction> invitation(forkedInvitations, PSafeReference);
    while (invitation != NULL) {
      if (invitation == &transaction)
        forkedInvitations.Remove(invitation++);
      else {
        if (!invitation->IsFailed())
          allFailed = false;
        ++invitation;
      }
    }
  }

  // All invitations failed, die now, with correct code
  if (allFailed && GetPhase() < ConnectedPhase)
    Release(GetCallEndReasonFromResponse(transaction));
}


void SIPConnection::OnReceivedPDU(SIP_PDU & pdu)
{
  SIP_PDU::Methods method = pdu.GetMethod();

  PSafeLockReadWrite lock(*this);
  if (!lock.IsLocked())
    return;

  // Prevent retries from getting through to processing
  unsigned sequenceNumber = pdu.GetMIME().GetCSeqIndex();
  if (m_lastRxCSeq.find(method) != m_lastRxCSeq.end() && sequenceNumber <= m_lastRxCSeq[method]) {
    PTRACE(3, "SIP\tIgnoring duplicate PDU " << pdu);
    return;
  }
  m_lastRxCSeq[method] = sequenceNumber;

  m_allowedMethods |= pdu.GetMIME().GetAllowBitMask();

  switch (method) {
    case SIP_PDU::Method_INVITE :
      OnReceivedINVITE(pdu);
      break;
    case SIP_PDU::Method_ACK :
      OnReceivedACK(pdu);
      break;
    case SIP_PDU::Method_CANCEL :
      OnReceivedCANCEL(pdu);
      break;
    case SIP_PDU::Method_BYE :
      OnReceivedBYE(pdu);
      break;
    case SIP_PDU::Method_OPTIONS :
      OnReceivedOPTIONS(pdu);
      break;
    case SIP_PDU::Method_NOTIFY :
      OnReceivedNOTIFY(pdu);
      break;
    case SIP_PDU::Method_REFER :
      OnReceivedREFER(pdu);
      break;
    case SIP_PDU::Method_INFO :
      OnReceivedINFO(pdu);
      break;
    case SIP_PDU::Method_PING :
      OnReceivedPING(pdu);
      break;
    case SIP_PDU::Method_PRACK :
      OnReceivedPRACK(pdu);
      break;
    case SIP_PDU::Method_MESSAGE :
      OnReceivedMESSAGE(pdu);
      break;
    default :
      // Shouldn't have got this!
      PTRACE(2, "SIP\tUnhandled PDU " << pdu);
      break;
  }
}


void SIPConnection::OnReceivedResponseToINVITE(SIPTransaction & transaction, SIP_PDU & response)
{
  unsigned statusCode = response.GetStatusCode();
  unsigned statusClass = statusCode/100;
  if (statusClass > 2)
    return;

  PSafeLockReadWrite lock(*this);
  if (!lock.IsLocked())
    return;

  // See if this is an initial INVITE or a re-INVITE
  bool reInvite = true;
  for (PSafePtr<SIPTransaction> invitation(forkedInvitations, PSafeReference); invitation != NULL; ++invitation) {
    if (invitation == &transaction) {
      reInvite = false;
      break;
    }
  }

  // If we are in a dialog, then m_dialog needs to be updated in the 2xx/1xx
  // response for a target refresh request
  m_dialog.Update(*transport, response);

  const SIPMIMEInfo & responseMIME = response.GetMIME();

  {
    SIPURL newRemotePartyID(responseMIME, RemotePartyID);
    if (!newRemotePartyID.IsEmpty()) {
      if (m_ciscoRemotePartyID.IsEmpty() && newRemotePartyID.GetUserName() == m_dialog.GetRemoteURI().GetUserName()) {
        PTRACE(3, "SIP\tOld style Remote-Party-ID set to \"" << newRemotePartyID << '"');
        m_ciscoRemotePartyID = newRemotePartyID;
      }
      else if (m_ciscoRemotePartyID != newRemotePartyID) {
        PTRACE(3, "SIP\tOld style Remote-Party-ID used for forwarding indication to \"" << newRemotePartyID << '"');

        m_ciscoRemotePartyID = newRemotePartyID;
        newRemotePartyID.SetParameters(PString::Empty());

        PStringToString info = m_ciscoRemotePartyID.GetParamVars();
        info.SetAt("result", "forwarded");
        info.SetAt("party", "A");
        info.SetAt("code", psprintf("%u", statusCode));
        info.SetAt("Referred-By", m_dialog.GetRemoteURI().AsString());
        info.SetAt("Remote-Party", newRemotePartyID.AsString());
        OnTransferNotify(info, this);
      }
    }
  }

  // Update internal variables on remote part names/number/address
  UpdateRemoteAddresses();

  if (reInvite)
    return;

  if (statusClass == 2) {
    // Have a final response to the INVITE, so cancel all the other invitations sent.
    for (PSafePtr<SIPTransaction> invitation(forkedInvitations, PSafeReference); invitation != NULL; ++invitation) {
      if (invitation != &transaction)
        invitation->Cancel();
    }

    // And end connect mode on the transport
    transport->SetInterface(transaction.GetInterface());
    m_contactAddress = transaction.GetMIME().GetContact();
  }

  responseMIME.GetProductInfo(remoteProductInfo);

  // Save the sessions etc we are actually using of all the forked INVITES sent
  SDPSessionDescription * sdp = response.GetSDP(m_localMediaFormats);
  if (sdp != NULL) {
    m_rtpSessions = ((SIPInvite &)transaction).GetSessionManager();
    if (remoteProductInfo.vendor.IsEmpty() && remoteProductInfo.name.IsEmpty()) {
      if (sdp->GetSessionName() != "-")
        remoteProductInfo.name = sdp->GetSessionName();
      if (sdp->GetUserName() != "-")
        remoteProductInfo.vendor = sdp->GetUserName();
    }
  }

  // Do PRACK after all the dialog completion parts above.
  if (statusCode > 100 && statusCode < 200 && responseMIME.GetRequire().Contains("100rel")) {
    PString rseq = responseMIME.GetString("RSeq");
    if (rseq.IsEmpty()) {
      PTRACE(2, "SIP\tReliable (100rel) response has no RSeq field.");
    }
    else if (rseq.AsUnsigned() <= m_prackSequenceNumber) {
      PTRACE(3, "SIP\tDuplicate response " << response.GetStatusCode() << ", already PRACK'ed");
    }
    else {
      transport->SetInterface(transaction.GetInterface()); // Make sure same as response
      SIPTransaction * prack = new SIPPrack(*this, rseq & transaction.GetMIME().GetCSeq());
      prack->Start();
    }
  }
}


void SIPConnection::UpdateRemoteAddresses()
{
  SIPURL remote = m_ciscoRemotePartyID;
  if (remote.IsEmpty()) {
    remote = m_dialog.GetRemoteURI();
    remote.Sanitise(SIPURL::ExternalURI);
  }
  remotePartyName = remote.GetDisplayName();

  remotePartyNumber = remote.GetUserName();
  if (!OpalIsE164(remotePartyNumber))
    remotePartyNumber.MakeEmpty();

  remotePartyAddress = remote.AsString();
  remotePartyName = remote.GetDisplayName();
  if (remotePartyName.IsEmpty())
    remotePartyName = remotePartyNumber.IsEmpty() ? remote.GetUserName() : remote.AsString();

  SIPURL request = m_dialog.GetRequestURI();
  request.Sanitise(SIPURL::ExternalURI);
  remotePartyURL = request.AsString();

  // If no local name, then use what the remote thinks we are
  if (localPartyName.IsEmpty())
    localPartyName = m_dialog.GetLocalURI().GetUserName();

  ownerCall.SetPartyNames();
}


void SIPConnection::NotifyDialogState(SIPDialogNotification::States state, SIPDialogNotification::Events eventType, unsigned eventCode)
{
  SIPURL url = m_dialog.GetLocalURI();
  url.Sanitise(SIPURL::ExternalURI);

  SIPDialogNotification info(url.AsString());

  info.m_dialogId = m_dialogNotifyId.AsString();
  info.m_callId = m_dialog.GetCallID();

  info.m_local.m_URI = url.AsString();
  info.m_local.m_dialogTag  = m_dialog.GetLocalTag();
  info.m_local.m_identity = url.AsString();
  info.m_local.m_display = url.GetDisplayName();
  info.m_local.m_appearance = m_appearanceCode;

  url = m_dialog.GetRemoteURI();
  url.Sanitise(SIPURL::ExternalURI);

  info.m_remote.m_URI = m_dialog.GetRequestURI().AsString();
  info.m_remote.m_dialogTag = m_dialog.GetRemoteTag();
  info.m_remote.m_identity = url.AsString();
  info.m_remote.m_display = url.GetDisplayName();

  if (!info.m_remote.m_dialogTag.IsEmpty() && state == SIPDialogNotification::Proceeding)
    state = SIPDialogNotification::Early;

  info.m_initiator = IsOriginating();
  info.m_state = state;
  info.m_eventType = eventType;
  info.m_eventCode = eventCode;

  if (GetPhase() == EstablishedPhase)
    info.m_local.m_rendering = info.m_remote.m_rendering = SIPDialogNotification::NotRenderingMedia;

  for (OpalMediaStreamPtr mediaStream(mediaStreams, PSafeReference); mediaStream != NULL; ++mediaStream) {
    if (mediaStream->IsSource())
      info.m_remote.m_rendering = SIPDialogNotification::RenderingMedia;
    else
      info.m_local.m_rendering = SIPDialogNotification::RenderingMedia;
  }

  endpoint.SendNotifyDialogInfo(info);
}


void SIPConnection::OnReceivedResponse(SIPTransaction & transaction, SIP_PDU & response)
{
  unsigned responseClass = response.GetStatusCode()/100;

  PSafeLockReadWrite lock(*this);
  if (!lock.IsLocked())
    return;

  m_allowedMethods |= response.GetMIME().GetAllowBitMask();

  switch (response.GetStatusCode()) {
    case SIP_PDU::Failure_UnAuthorised :
    case SIP_PDU::Failure_ProxyAuthenticationRequired :
      if (OnReceivedAuthenticationRequired(transaction, response))
        return;
      break;

    default :
      m_authenticateErrors = 0;
  }

  if (transaction.GetMethod() != SIP_PDU::Method_INVITE) {
    switch (responseClass) {
      case 1 : // Treat all other provisional responses like a Trying.
        OnReceivedTrying(transaction, response);
        return;

      case 2 : // Successful response - there really is only 200 OK
        OnReceivedOK(transaction, response);
        break;

      default :
        if (transaction.GetMethod() == SIP_PDU::Method_REFER) {
          m_referInProgress = false;

          PStringToString info;
          info.SetAt("result", "error");
          info.SetAt("party", "B");
          info.SetAt("code", psprintf("%u", response.GetStatusCode()));
          OnTransferNotify(info, this);
        }
    }

    std::map<std::string, SIP_PDU *>::iterator it = m_responses.find(transaction.GetTransactionID());
    if (it != m_responses.end()) {
      *it->second = response;
      m_responses.erase(it);
    }

    return;
  }

  const SIPMIMEInfo & responseMIME = response.GetMIME();

  // If they ignore our "Require" header, we have to kill the call ourselves
  if (m_prackMode == e_prackRequired &&
      responseClass == 1 &&
      response.GetStatusCode() != SIP_PDU::Information_Trying &&
      !responseMIME.GetRequire().Contains("100rel")) {
    Release(EndedBySecurityDenial);
    return;
  }

  // Don't pass on retries of reliable provisional responses.
  {
    PString snStr = responseMIME.GetString("RSeq");
    if (!snStr.IsEmpty()) {
      unsigned sn = snStr.AsUnsigned();
      if (sn <= m_prackSequenceNumber)
        return;
      m_prackSequenceNumber = sn;
    }
  }

  if (GetPhase() < EstablishedPhase) {
    PString referToken = m_stringOptions(OPAL_SIP_REFERRED_CONNECTION);
    if (!referToken.IsEmpty()) {
      PSafePtr<SIPConnection> referred = endpoint.GetSIPConnectionWithLock(referToken, PSafeReadOnly);
      if (referred != NULL) {
        (new SIPReferNotify(*referred, response.GetStatusCode()))->Start();

        if (response.GetStatusCode() >= 300) {
          PTRACE(3, "SIP\tFailed to transfer " << *referred);
          referred->SetPhase(EstablishedPhase); // Go back to established

          PStringToString info;
          info.SetAt("result", "failed");
          info.SetAt("party", "A");
          info.SetAt("code", psprintf("%u", response.GetStatusCode()));
          OnTransferNotify(info, this);
        }
        else if (response.GetStatusCode() >= 200) {
          PTRACE(3, "SIP\tCompleted transfer of " << *referred);
          referred->Release(OpalConnection::EndedByCallForwarded);

          PStringToString info;
          info.SetAt("result", "success");
          info.SetAt("party", "A");
          OnTransferNotify(info, this);
        }
      }
    }
  }

  bool handled = false;

  // Break out to virtual functions for some special cases.
  switch (response.GetStatusCode()) {
    case SIP_PDU::Information_Ringing :
      OnReceivedRinging(response);
      return;

    case SIP_PDU::Information_Session_Progress :
      OnReceivedSessionProgress(response);
      return;

    case SIP_PDU::Failure_MessageTooLarge :
    {
      SIPURL newTransportAddress(transport->GetRemoteAddress());
      newTransportAddress.SetParamVar("transport", "tcp");
      if (!SetTransport(newTransportAddress))
        break;

      SIPInvite * newInvite = new SIPInvite(*this, ((SIPInvite &)transaction).GetSessionManager());
      if (!newInvite->Start()) {
        PTRACE(2, "SIP\tCould not restart INVITE for switch to TCP");
        break;
      }

      forkedInvitations.Append(newInvite);
      return;
    }

    case SIP_PDU::Failure_RequestPending :
      m_handlingINVITE = false;
      if (StartPendingReINVITE())
        return;
      // Is real error then
      break;

    default :
      switch (responseClass) {
        case 1 : // Treat all other provisional responses like a Trying.
          OnReceivedTrying(transaction, response);
          return;

        case 2 : // Successful response - there really is only 200 OK
          OnReceivedOK(transaction, response);
          handled = true;
          break;

        case 3 : // Redirection response
          OnReceivedRedirection(response);
          handled = true;
          break;
      }
  }

  m_handlingINVITE = false;

  // To avoid overlapping INVITE transactions, wait till here before
  // starting the next one.
  if (responseClass != 1) {
    pendingInvitations.Remove(&transaction);
    StartPendingReINVITE();
  }

  if (handled)
    return;

  // If we are doing a local hold, and it failed, we do not release the connection
  switch (m_holdToRemote) {
    case eHoldInProgress :
      PTRACE(4, "SIP\tHold request failed on " << *this);
      m_holdToRemote = eHoldOff;  // Did not go into hold
      OnHold(false, false);   // Signal the manager that there is no more hold
      break;

    case eRetrieveInProgress :
      PTRACE(4, "SIP\tRetrieve request failed on " << *this);
      m_holdToRemote = eHoldOn;  // Did not go out of hold
      OnHold(false, true);   // Signal the manager that hold is still active
      break;

    default :
      break;
  }

  if (GetPhase() == EstablishedPhase) {
    // Is a re-INVITE if in here, so don't kill the call becuase it failed.
#if OPAL_FAX
    if (m_faxMediaStreamsSwitchState != e_NotSwitchingFaxMediaStreams) {
      bool switchingToT38 = (m_faxMediaStreamsSwitchState == e_SwitchingToFaxMediaStreams);
      if (m_switchedToT38 != switchingToT38)
        OnSwitchedFaxMediaStreams(switchingToT38, false);
    }
#endif
    return;
  }

  // We don't always release the connection, eg not till all forked invites have completed
  // This INVITE is from a different "dialog", any errors do not cause a release

  if (GetPhase() < ConnectedPhase) {
    // Final check to see if we have forked INVITEs still running, don't
    // release connection until all of them have failed.
    for (PSafePtr<SIPTransaction> invitation(forkedInvitations, PSafeReference); invitation != NULL; ++invitation) {
      if (invitation->IsProceeding())
        return;
      // If we have not even got a 1xx from the remote for this forked INVITE,
      // don't keep waiting, cancel it and take the error we got
      if (invitation->IsTrying())
        invitation->Cancel();
    }
  }

  // All other responses are errors, set Q931 code if available
  releaseMethod = ReleaseWithNothing;
  Release(GetCallEndReasonFromResponse(response));
}


SIPConnection::TypeOfINVITE SIPConnection::CheckINVITE(const SIP_PDU & request) const
{
  const SIPMIMEInfo & requestMIME = request.GetMIME();
  PString requestFromTag = requestMIME.GetFieldParameter("From", "tag");
  PString requestToTag   = requestMIME.GetFieldParameter("To",   "tag");

  // Criteria for our existing dialog.
  if (!requestToTag.IsEmpty() &&
       m_dialog.GetCallID() == requestMIME.GetCallID() &&
       m_dialog.GetRemoteTag() == requestFromTag &&
       m_dialog.GetLocalTag() == requestToTag)
    return IsReINVITE;

  if (IsOriginating()) {
    /* Weird, got incoming INVITE for a call we originated, however it is not in
       the same dialog. Send back a refusal as this just isn't handled. */
    PTRACE(2, "SIP\tIgnoring INVITE from " << request.GetURI() << " when originated call.");
    return IsLoopedINVITE;
  }

  // No original INVITE, so we are in a race condition at start up of the
  // connection, assume a duplicate so it is ignored. If it does turn out
  // to be new INVITE, then it should be retried and next time the race
  // will be passed.
  if (originalInvite == NULL) {
    PTRACE(3, "SIP\tIgnoring INVITE from " << request.GetURI() << " as we are originator.");
    return IsDuplicateINVITE;
  }

  /* If we have same transaction ID, it means it is a retransmission
     of the original INVITE, probably should re-transmit last sent response
     but we just ignore it. Still should work. */
  if (originalInvite->GetTransactionID() == request.GetTransactionID()) {
    PTimeInterval timeSinceInvite = PTime() - originalInviteTime;
    PTRACE(3, "SIP\tIgnoring duplicate INVITE from " << request.GetURI() << " after " << timeSinceInvite);
    return IsDuplicateINVITE;
  }

  // Check if is RFC3261/8.2.2.2 case relating to merged requests.
  if (!requestToTag.IsEmpty()) {
    PTRACE(3, "SIP\tIgnoring INVITE from " << request.GetURI() << " as has invalid to-tag.");
    return IsDuplicateINVITE;
  }

  // More checks for RFC3261/8.2.2.2 case relating to merged requests.
  if (m_dialog.GetRemoteTag() != requestFromTag ||
      m_dialog.GetCallID() != requestMIME.GetCallID() ||
      originalInvite->GetMIME().GetCSeq() != requestMIME.GetCSeq() ||
      request.GetTransactionID().NumCompare("z9hG4bK") != EqualTo) // Or RFC2543
    return IsNewINVITE; // No it isn't

  /* This is either a merged request or a brand new "dialog" to the same call.
     Probably indicates forking request or request arriving via multiple IP
     paths. In both cases we refuse the INVITE. The first because we don't
     support multiple dialogs on one call, the second because the RFC says
     we should! */
  PTRACE(3, "SIP\tIgnoring forked INVITE from " << request.GetURI());
  return IsLoopedINVITE;
}


void SIPConnection::OnReceivedINVITE(SIP_PDU & request)
{
  bool isReinvite = IsOriginating() || originalInvite != NULL;
  PTRACE_IF(4, !isReinvite, "SIP\tInitial INVITE to " << request.GetURI());

  // originalInvite should contain the first received INVITE for
  // this connection
  delete originalInvite;
  originalInvite     = new SIP_PDU(request);
  originalInviteTime = PTime();

  SIPMIMEInfo & mime = originalInvite->GetMIME();

  // update the dialog context
  m_dialog.SetLocalTag(GetToken());
  m_dialog.Update(*transport, request);
  UpdateRemoteAddresses();

  // We received a Re-INVITE for a current connection
  if (isReinvite) { 
    OnReceivedReINVITE(request);
    return;
  }

  SetPhase(SetUpPhase);

  OnApplyStringOptions();

  NotifyDialogState(SIPDialogNotification::Trying);
  mime.GetAlertInfo(m_alertInfo, m_appearanceCode);

  // Fill in all the various connection info, note our to/from is their from/to
  mime.GetProductInfo(remoteProductInfo);

  m_ciscoRemotePartyID = SIPURL(mime, RemotePartyID);
  PTRACE_IF(4, !m_ciscoRemotePartyID.IsEmpty(),
            "SIP\tOld style Remote-Party-ID set to \"" << m_ciscoRemotePartyID << '"');

  m_contactAddress = request.GetURI();

  mime.SetTo(m_dialog.GetLocalURI().AsQuotedString());

  // get the called destination number and name
  m_calledPartyName = request.GetURI().GetUserName();
  if (!m_calledPartyName.IsEmpty() && m_calledPartyName.FindSpan("0123456789*#") == P_MAX_INDEX) {
    m_calledPartyNumber = m_calledPartyName;
    m_calledPartyName = request.GetURI().GetDisplayName(false);
  }

  m_redirectingParty = mime.GetReferredBy();
  PTRACE_IF(4, !m_redirectingParty.IsEmpty(),
            "SIP\tRedirecting party (Referred-By/Diversion) set to \"" << m_redirectingParty << '"');

  // get the address that remote end *thinks* it is using from the Contact field
  PIPSocket::Address sigAddr;
  if (!PIPSocket::GetHostAddress(m_dialog.GetRequestURI().GetHostName(), sigAddr)) {
    PString via = mime.GetFirstVia();
    if (!via.IsEmpty())
      sigAddr = via(via.Find(' ')+1, via.Find(':')-1);
  }

  // get the local and peer transport addresses
  PIPSocket::Address peerAddr, localAddr;
  transport->GetRemoteAddress().GetIpAddress(peerAddr);
  transport->GetLocalAddress().GetIpAddress(localAddr);

  // allow the application to determine if RTP NAT is enabled or not
  remoteIsNAT = IsRTPNATEnabled(localAddr, peerAddr, sigAddr, PTrue);

  bool prackSupported = mime.GetSupported().Contains("100rel");
  bool prackRequired = mime.GetRequire().Contains("100rel");
  switch (m_prackMode) {
    case e_prackDisabled :
      if (prackRequired) {
        SIP_PDU response(request, SIP_PDU::Failure_BadExtension);
        response.GetMIME().SetUnsupported("100rel");
        request.SendResponse(*transport, response);
        return;
      }
      // Ignore, if just supported
      break;

    case e_prackSupported :
      m_prackEnabled = prackSupported || prackRequired;
      break;

    case e_prackRequired :
      m_prackEnabled = prackSupported || prackRequired;
      if (!m_prackEnabled) {
        SIP_PDU response(request, SIP_PDU::Failure_ExtensionRequired);
        response.GetMIME().SetRequire("100rel");
        request.SendResponse(*transport, response);
        return;
      }
  }

  releaseMethod = ReleaseWithResponse;
  m_handlingINVITE = true;

  // See if we have a replaces header, if not is normal call
  PString replaces = mime("Replaces");
  if (replaces.IsEmpty()) {
    // indicate the other is to start ringing (but look out for clear calls)
    if (!OnIncomingConnection(0, NULL)) {
      PTRACE(1, "SIP\tOnIncomingConnection failed for INVITE from " << request.GetURI() << " for " << *this);
      Release();
      return;
    }

    PTRACE(3, "SIP\tOnIncomingConnection succeeded for INVITE from " << request.GetURI() << " for " << *this);

    if (!SetRemoteMediaFormats(originalInvite->GetSDP(GetLocalMediaFormats()))) {
      Release(EndedByCapabilityExchange);
      return;
    }

    if (ownerCall.OnSetUp(*this)) {
      if (GetPhase() < ProceedingPhase) {
        SetPhase(ProceedingPhase);
        OnProceeding();
      }
      AnsweringCall(OnAnswerCall(GetRemotePartyURL()));
      return;
    }

    PTRACE(1, "SIP\tOnSetUp failed for INVITE from " << request.GetURI() << " for " << *this);
    Release();
    return;
  }

  // Replaces header string has already been validated in SIPEndPoint::OnReceivedINVITE
  PSafePtr<SIPConnection> replacedConnection = endpoint.GetSIPConnectionWithLock(replaces, PSafeReference);
  if (replacedConnection == NULL) {
    /* Allow for a race condition where between when SIPEndPoint::OnReceivedINVITE()
       is executed and here, the call to be replaced was released. */
    Release(EndedByInvalidConferenceID);
    return;
  }

  if (replacedConnection->GetPhase() < ConnectedPhase) {
    if (!replacedConnection->IsOriginating()) {
      PTRACE(3, "SIP\tEarly connection " << *replacedConnection << " cannot be replaced by " << *this);
      Release(EndedByInvalidConferenceID);
      return;
    }
  }
  else {
    if (replaces.Find(";early-only") != P_MAX_INDEX) {
      PTRACE(3, "SIP\tReplaces has early-only on early connection " << *this);
      Release(EndedByLocalBusy);
      return;
    }
  }

  if (!SetRemoteMediaFormats(originalInvite->GetSDP(GetLocalMediaFormats()))) {
    Release(EndedByCapabilityExchange);
    return;
  }

  PTRACE(3, "SIP\tEstablished connection " << *replacedConnection << " replaced by " << *this);

  // Set forward party to call token so the SetConnected() that completes the
  // operation happens on the OnReleases() thread.
  replacedConnection->m_forwardParty = GetToken();
  replacedConnection->Release(OpalConnection::EndedByCallForwarded);

  // Check if we are the target of an attended transfer, indicated by a referred-by header
  if (!m_redirectingParty.IsEmpty()) {
    /* Indicate to application we are party C in a consultation transfer.
       The calls are A->B (first call), B->C (consultation call) => A->C
       (final call after the transfer) */
    PStringToString info = PURL(m_redirectingParty).GetParamVars();
    info.SetAt("result", "incoming");
    info.SetAt("party", "C");
    info.SetAt("Referred-By", m_redirectingParty);
    info.SetAt("Remote-Party", GetRemotePartyURL());
    OnTransferNotify(info, this);
  }
}


void SIPConnection::OnReceivedReINVITE(SIP_PDU & request)
{
  if (m_handlingINVITE || GetPhase() < ConnectedPhase) {
    PTRACE(2, "SIP\tRe-INVITE from " << request.GetURI() << " received while INVITE in progress on " << *this);
    request.SendResponse(*transport, SIP_PDU::Failure_RequestPending);
    return;
  }

  PTRACE(3, "SIP\tReceived re-INVITE from " << request.GetURI() << " for " << *this);

  m_needReINVITE = true;
  m_handlingINVITE = true;

  // send the 200 OK response
  if (SendInviteOK())
    ownerCall.StartMediaStreams();
  else
    SendInviteResponse(SIP_PDU::Failure_NotAcceptableHere);

  m_answerFormatList.RemoveAll();

  SIPURL newRemotePartyID(request.GetMIME(), RemotePartyID);
  if (newRemotePartyID.IsEmpty() || m_ciscoRemotePartyID == newRemotePartyID)
    UpdateRemoteAddresses();
  else {
    PTRACE(3, "SIP\tOld style Remote-Party-ID used for transfer indication to \"" << newRemotePartyID << '"');

    m_ciscoRemotePartyID = newRemotePartyID;
    newRemotePartyID.SetParameters(PString::Empty());
    UpdateRemoteAddresses();

    PStringToString info = m_ciscoRemotePartyID.GetParamVars();
    info.SetAt("result", "incoming");
    info.SetAt("party", "C");
    info.SetAt("Referred-By", m_dialog.GetRemoteURI().AsString());
    info.SetAt("Remote-Party", newRemotePartyID.AsString());
    OnTransferNotify(info, this);
  }
}


void SIPConnection::OnReceivedACK(SIP_PDU & response)
{
  if (originalInvite == NULL) {
    PTRACE(2, "SIP\tACK from " << response.GetURI() << " received before INVITE!");
    return;
  }

  // Forked request
  PString origFromTag = originalInvite->GetMIME().GetFieldParameter("From", "tag");
  PString origToTag   = originalInvite->GetMIME().GetFieldParameter("To",   "tag");
  PString fromTag     = response.GetMIME().GetFieldParameter("From", "tag");
  PString toTag       = response.GetMIME().GetFieldParameter("To",   "tag");
  if (fromTag != origFromTag || (!toTag.IsEmpty() && (toTag != origToTag))) {
    PTRACE(3, "SIP\tACK received for forked INVITE from " << response.GetURI());
    return;
  }

  PTRACE(3, "SIP\tACK received: " << GetPhase());

  m_responseFailTimer.Stop(false); // Asynchronous stop to avoid deadlock
  m_responseRetryTimer.Stop(false);

  // If got ACK, any pending responses are done, even if they are not acknowledged
  while (!m_responsePackets.empty())
    m_responsePackets.pop();

  OnReceivedAnswerSDP(response);

  m_handlingINVITE = false;

  if (GetPhase() == ConnectedPhase) {
    SetPhase(EstablishedPhase);
    OnEstablished();
  }

  StartPendingReINVITE();
}


void SIPConnection::OnReceivedOPTIONS(SIP_PDU & request)
{
  if (request.GetMIME().GetAccept().Find("application/sdp") == P_MAX_INDEX)
    request.SendResponse(*transport, SIP_PDU::Failure_UnsupportedMediaType);
  else {
    SDPSessionDescription sdp(m_sdpSessionId, m_sdpVersion, transport->GetLocalAddress());
    SIP_PDU response(request, SIP_PDU::Successful_OK);
    response.SetAllow(GetAllowedMethods());
    response.SetEntityBody(sdp.Encode());
    request.SendResponse(*transport, response, &endpoint);
  }
}


void SIPConnection::OnAllowedEventNotify(const PString & /* eventStr */)
{
}


void SIPConnection::OnReceivedNOTIFY(SIP_PDU & request)
{
  /* Transfering a Call
     We have sent a REFER to the UA in this connection.
     Now we get Progress Indication of that REFER Request by NOTIFY Requests.
     Handle the response coded of "message/sipfrag" as follows:

     Use code >= 180  &&  code < 300 to Release this dialog.
     Use Subscription State = terminated to Release this dialog.
  */

  const SIPMIMEInfo & mime = request.GetMIME();

  SIPSubscribe::EventPackage package(mime.GetEvent());
  if (m_allowedEvents.GetStringsIndex(package) != P_MAX_INDEX) {
    PTRACE(2, "SIP\tReceived Notify for allowed event " << package);
    request.SendResponse(*transport, SIP_PDU::Successful_OK);
    OnAllowedEventNotify(package);
    return;
  }

  // Do not include the id parameter in this comparison, may need to
  // do it later if we ever support multiple simultaneous REFERs
  if (package.Find("refer") == P_MAX_INDEX) {
    PTRACE(2, "SIP\tNOTIFY in a connection only supported for REFER requests");
    request.SendResponse(*transport, SIP_PDU::Failure_BadEvent);
    return;
  }

  if (!m_referInProgress) {
    PTRACE(2, "SIP\tNOTIFY for REFER we never sent.");
    request.SendResponse(*transport, SIP_PDU::Failure_TransactionDoesNotExist);
    return;
  }

  if (mime.GetContentType() != "message/sipfrag") {
    PTRACE(2, "SIP\tNOTIFY for REFER has incorrect Content-Type");
    request.SendResponse(*transport, SIP_PDU::Failure_BadRequest);
    return;
  }

  PCaselessString body = request.GetEntityBody();
  unsigned code = body.Mid(body.Find(' ')).AsUnsigned();
  if (body.NumCompare("SIP/") != EqualTo || code < 100) {
    PTRACE(2, "SIP\tNOTIFY for REFER has incorrect body");
    request.SendResponse(*transport, SIP_PDU::Failure_BadRequest);
    return;
  }

  request.SendResponse(*transport, SIP_PDU::Successful_OK);

  PStringToString info;
  PCaselessString state = mime.GetSubscriptionState(info);
  m_referInProgress = state != "terminated";
  info.SetAt("party", "B"); // We are B party in consultation transfer
  info.SetAt("state", state);
  info.SetAt("code", psprintf("%u", code));
  info.SetAt("result", m_referInProgress ? "progress" : (code < 300 ? "success" : "failed"));

  if (OnTransferNotify(info, this))
    return;

  // Release the connection
  if (IsReleased())
    return;

  releaseMethod = ReleaseWithBYE;
  Release(OpalConnection::EndedByCallForwarded);
}


void SIPConnection::OnReceivedREFER(SIP_PDU & request)
{
  const SIPMIMEInfo & requestMIME = request.GetMIME();

  PString referTo = requestMIME.GetReferTo();
  if (referTo.IsEmpty()) {
    SIP_PDU response(request, SIP_PDU::Failure_BadRequest);
    response.SetInfo("Missing refer-to header");
    request.SendResponse(*transport, response);
    return;
  }    

  SIP_PDU response(request, SIP_PDU::Successful_Accepted);

  // Comply to RFC4488
  bool referSub = true;
  static PConstCaselessString const ReferSubHeader("Refer-Sub");
  if (requestMIME.Contains(ReferSubHeader)) {
    referSub = requestMIME.GetBoolean(ReferSubHeader, true);
    response.GetMIME().SetBoolean(ReferSubHeader, referSub);
  }

  // send response before attempting the transfer
  if (!request.SendResponse(*transport, response))
    return;

  m_redirectingParty = requestMIME.GetReferredBy();
  if (m_redirectingParty.IsEmpty()) {
    SIPURL from = requestMIME.GetFrom();
    from.Sanitise(SIPURL::ExternalURI);
    m_redirectingParty = from.AsString();
  }

  PStringToString info = PURL(m_redirectingParty).GetParamVars();
  info.SetAt("result", "started");
  info.SetAt("party", "A");
  info.SetAt("Referred-By", m_redirectingParty);
  OnTransferNotify(info, this);

  SIPURL to = referTo;
  PString replaces = to.GetQueryVars()("Replaces");
  to.SetQuery(PString::Empty());

  if (referSub)
    to.SetParamVar(OPAL_URL_PARAM_PREFIX OPAL_SIP_REFERRED_CONNECTION, GetToken());

  // send NOTIFY if transfer failed, but only if allowed by RFC4488
  if (!endpoint.SetupTransfer(GetToken(), replaces, to.AsString(), NULL) && referSub)
    (new SIPReferNotify(*this, SIP_PDU::GlobalFailure_Decline))->Start();
}


void SIPConnection::OnReceivedBYE(SIP_PDU & request)
{
  PTRACE(3, "SIP\tBYE received for call " << request.GetMIME().GetCallID());
  request.SendResponse(*transport, SIP_PDU::Successful_OK);
  
  if (IsReleased()) {
    PTRACE(2, "SIP\tAlready released " << *this);
    return;
  }
  releaseMethod = ReleaseWithNothing;

  m_dialog.Update(*transport, request);
  UpdateRemoteAddresses();
  request.GetMIME().GetProductInfo(remoteProductInfo);

  Release(EndedByRemoteUser);
}


void SIPConnection::OnReceivedCANCEL(SIP_PDU & request)
{
  // Currently only handle CANCEL requests for the original INVITE that
  // created this connection, all else ignored

  if (originalInvite == NULL || originalInvite->GetTransactionID() != request.GetTransactionID()) {
    PTRACE(2, "SIP\tUnattached " << request << " received for " << *this);
    request.SendResponse(*transport, SIP_PDU::Failure_TransactionDoesNotExist);
    return;
  }

  PTRACE(3, "SIP\tCancel received for " << *this);

  SIP_PDU response(request, SIP_PDU::Successful_OK);
  response.GetMIME().SetTo(m_dialog.GetLocalURI().AsQuotedString());
  request.SendResponse(*transport, response);
  
  if (!IsOriginating())
    Release(EndedByCallerAbort);
}


void SIPConnection::OnReceivedTrying(SIPTransaction & transaction, SIP_PDU & /*response*/)
{
  if (transaction.GetMethod() != SIP_PDU::Method_INVITE)
    return;

  PTRACE(3, "SIP\tReceived Trying response");
  NotifyDialogState(SIPDialogNotification::Proceeding);

  if (GetPhase() < ProceedingPhase) {
    SetPhase(ProceedingPhase);
    OnProceeding();
  }
}


void SIPConnection::OnStartTransaction(SIPTransaction & transaction)
{
  endpoint.OnStartTransaction(*this, transaction);
}


void SIPConnection::OnReceivedRinging(SIP_PDU & response)
{
  PTRACE(3, "SIP\tReceived Ringing response");

  OnReceivedAnswerSDP(response);

  response.GetMIME().GetAlertInfo(m_alertInfo, m_appearanceCode);

  if (GetPhase() < AlertingPhase) {
    SetPhase(AlertingPhase);
    OnAlerting();
    NotifyDialogState(SIPDialogNotification::Early);
  }

  PTRACE_IF(4, response.GetSDP(m_localMediaFormats) != NULL,
            "SIP\tStarting receive media to annunciate remote alerting tone");
  ownerCall.StartMediaStreams();
}


void SIPConnection::OnReceivedSessionProgress(SIP_PDU & response)
{
  PTRACE(3, "SIP\tReceived Session Progress response");

  OnReceivedAnswerSDP(response);

  if (GetPhase() < AlertingPhase) {
    SetPhase(AlertingPhase);
    OnAlerting();
    NotifyDialogState(SIPDialogNotification::Early);
  }

  PTRACE(4, "SIP\tStarting receive media to annunciate remote progress tones");
  ownerCall.StartMediaStreams();
}


void SIPConnection::OnReceivedRedirection(SIP_PDU & response)
{
  SIPURL whereTo = response.GetMIME().GetContact();
  for (PINDEX i = 0; i < m_stringOptions.GetSize(); ++i)
    whereTo.SetParamVar(OPAL_URL_PARAM_PREFIX + m_stringOptions.GetKeyAt(i), m_stringOptions.GetDataAt(i));
  PTRACE(3, "SIP\tReceived redirect to " << whereTo);
  endpoint.ForwardConnection(*this, whereTo.AsString());
}


PBoolean SIPConnection::OnReceivedAuthenticationRequired(SIPTransaction & transaction, SIP_PDU & response)
{
  // Try to find authentication parameters for the given realm,
  // if not, use the proxy authentication parameters (if any)
  SIP_PDU::StatusCodes status = endpoint.HandleAuthentication(m_authentication,
                                                              m_authenticateErrors,
                                                              response,
                                                              m_dialog.GetProxy(),
                                                              m_dialog.GetLocalURI().GetUserName(),
                                                              PString::Empty());
  if (status != SIP_PDU::Successful_OK)
    return false;

  transport->SetInterface(transaction.GetInterface());

  SIPTransaction * newTransaction = transaction.CreateDuplicate();
  if (newTransaction == NULL) {
    PTRACE(1, "SIP\tCannot create duplicate transaction for " << transaction);
    return false;
  }

  if (!newTransaction->Start()) {
    PTRACE(2, "SIP\tCould not restart " << transaction);
    return false;
  }

  if (transaction.GetMethod() == SIP_PDU::Method_INVITE)
    forkedInvitations.Append(newTransaction);
  else {
    std::map<std::string, SIP_PDU *>::iterator it = m_responses.find(transaction.GetTransactionID());
    if (it != m_responses.end()) {
      m_responses[newTransaction->GetTransactionID()] = it->second;
      m_responses.erase(it);
    }
  }

  return true;
}


void SIPConnection::OnReceivedOK(SIPTransaction & transaction, SIP_PDU & response)
{
  switch (transaction.GetMethod()) {
    case SIP_PDU::Method_INVITE :
      break;

    case SIP_PDU::Method_REFER :
      if (!response.GetMIME().GetBoolean("Refer-Sub", true)) {
        // Used RFC4488 to indicate we are NOT doing NOTIFYs, release now
        PTRACE(3, "SIP\tBlind transfer accepted, without NOTIFY so ending local call.");
        m_referInProgress = false;

        PStringToString info;
        info.SetAt("result", "blind");
        info.SetAt("party", "B");
        OnTransferNotify(info, this);

        Release(OpalConnection::EndedByCallForwarded);
      }
      // Do next case

    default :
      return;
  }

  PTRACE(3, "SIP\tReceived INVITE OK response for " << transaction.GetMethod());
  releaseMethod = ReleaseWithBYE;
  sessionTimer = 10000;

  NotifyDialogState(SIPDialogNotification::Confirmed);

  OnReceivedAnswerSDP(response);

#if OPAL_FAX
  if (m_faxMediaStreamsSwitchState != e_NotSwitchingFaxMediaStreams) {
    bool switchingToFax = (m_faxMediaStreamsSwitchState == e_SwitchingToFaxMediaStreams);

    SDPSessionDescription * sdp = response.GetSDP(m_localMediaFormats);
    bool switchedToT38 = sdp != NULL && sdp->GetMediaDescriptionByType(OpalMediaType::Fax()) != NULL;

    // Attempted to change fax state, but the remote rudely ignored it!
    if (switchingToFax != switchedToT38)
      OnSwitchedFaxMediaStreams(switchingToFax, false); // Indicate failed
    else {
      // We asked for fax/audio, we got fax/audio
      if (m_switchedToT38 != switchedToT38) {
        // And wasn't a repeat ...
        m_switchedToT38 = switchedToT38;
        OnSwitchedFaxMediaStreams(switchingToFax, true);
      }
    }
  }
#endif

  switch (m_holdToRemote) {
    case eHoldInProgress :
      m_holdToRemote = eHoldOn;
      OnHold(false, true);   // Signal the manager that they are on hold
      break;

    case eRetrieveInProgress :
      m_holdToRemote = eHoldOff;
      OnHold(false, false);   // Signal the manager that there is no more hold
      break;

    default :
      break;
  }

  OnConnectedInternal();
}


void SIPConnection::OnReceivedAnswerSDP(SIP_PDU & response)
{
  SDPSessionDescription * sdp = response.GetSDP(m_localMediaFormats);
  if (sdp == NULL)
    return;

  m_answerFormatList = sdp->GetMediaFormats();
  AdjustMediaFormats(false, NULL, m_answerFormatList);

  bool holdFromRemote = sdp->IsHold();
  if (m_holdFromRemote != holdFromRemote) {
    PTRACE(3, "SIP\tRemote " << (holdFromRemote ? "" : "retrieve from ") << "hold detected");
    m_holdFromRemote = holdFromRemote;
    OnHold(true, holdFromRemote);
  }

  unsigned sessionCount = sdp->GetMediaDescriptions().GetSize();

  bool multipleFormats = false;
  bool ok = false;
  for (unsigned session = 1; session <= sessionCount; ++session) {
    if (OnReceivedAnswerSDPSession(*sdp, session, multipleFormats))
      ok = true;
    else {
      OpalMediaStreamPtr stream;
      if ((stream = GetMediaStream(session, false)) != NULL)
        stream->Close();
      if ((stream = GetMediaStream(session, true)) != NULL)
        stream->Close();
    }
  }

  m_answerFormatList.RemoveAll();

  /* Shut down any media that is in a session not mentioned in a re-INVITE.
     While the SIP/SDP specification says this shouldn't happen, it does
     anyway so we need to deal. */
  for (OpalMediaStreamPtr stream(mediaStreams, PSafeReference); stream != NULL; ++stream) {
    if (stream->GetSessionID() > sessionCount)
      stream->Close();
  }

  /* See if remote has answered our offer with multiple possible codecs.
     While this is perfectly legal, and we are supposed to wait for the first
     RTP packet to arrive before setting up codecs etc, our architecture
     cannot deal with that. So what we do is immediately, send a re-INVITE
     nailing the codec down to the first reply. */
  if (multipleFormats && m_resolveMultipleFormatReINVITE && response.GetStatusCode()/100 == 2) {
    m_resolveMultipleFormatReINVITE= false;
    SendReINVITE(PTRACE_PARAM("resolve multiple codecs in answer"));
  }

  if (GetPhase() == EstablishedPhase)
    ownerCall.StartMediaStreams(); // re-INVITE
  else {
    if (!ok)
      Release(EndedByCapabilityExchange);
  }
}


bool SIPConnection::OnReceivedAnswerSDPSession(SDPSessionDescription & sdp, unsigned rtpSessionId, bool & multipleFormats)
{
  SDPMediaDescription * mediaDescription = sdp.GetMediaDescriptionByIndex(rtpSessionId);
  if (!PAssert(mediaDescription != NULL, "SDP Media description list changed"))
    return false;

  OpalMediaType mediaType = mediaDescription->GetMediaType();
  
  PTRACE(4, "SIP\tProcessing received SDP media description for " << mediaType);

  /* Get the media the remote has answered to our offer. Remove the media
     formats we do not support, in case the remote is insane and replied
     with something we did not actually offer. */
  if (!m_answerFormatList.HasType(mediaType)) {
    PTRACE(2, "SIP\tCould not find supported media formats in SDP media description for session " << rtpSessionId);
    return false;
  }

  // Set up the media session, e.g. RTP
  bool remoteChanged = false;
  OpalTransportAddress localAddress;
  if (SetUpMediaSession(rtpSessionId, mediaType, *mediaDescription, localAddress, remoteChanged) == NULL)
    return false;

  SDPMediaDescription::Direction otherSidesDir = sdp.GetDirection(rtpSessionId);

  // Check if we had a stream and the remote has either changed the codec or
  // changed the direction of the stream
  OpalMediaStreamPtr sendStream = GetMediaStream(rtpSessionId, false);
  PauseOrCloseMediaStream(sendStream, m_answerFormatList, remoteChanged, (otherSidesDir&SDPMediaDescription::RecvOnly) == 0);

  OpalMediaStreamPtr recvStream = GetMediaStream(rtpSessionId, true);
  PauseOrCloseMediaStream(recvStream, m_answerFormatList, remoteChanged, (otherSidesDir&SDPMediaDescription::SendOnly) == 0);

  // Then open the streams if the direction allows and if needed
  // If already open then update to new parameters/payload type

  if (recvStream == NULL &&
      ownerCall.OpenSourceMediaStreams(*this, mediaType, rtpSessionId) &&
      (recvStream = GetMediaStream(rtpSessionId, true)) != NULL) {
    recvStream->UpdateMediaFormat(*m_localMediaFormats.FindFormat(recvStream->GetMediaFormat()));
    recvStream->SetPaused((otherSidesDir&SDPMediaDescription::SendOnly) == 0);
  }

  if (sendStream == NULL) {
    PSafePtr<OpalConnection> otherParty = GetOtherPartyConnection();
    if (otherParty != NULL &&
        ownerCall.OpenSourceMediaStreams(*otherParty, mediaType, rtpSessionId) &&
        (sendStream = GetMediaStream(rtpSessionId, false)) != NULL)
      sendStream->SetPaused((otherSidesDir&SDPMediaDescription::RecvOnly) == 0);
  }

  PINDEX maxFormats = 1;
  if (mediaType == OpalMediaType::Audio()) {
    if (SetNxECapabilities(rfc2833Handler, m_localMediaFormats, m_answerFormatList, OpalRFC2833))
      ++maxFormats;
#if OPAL_T38_CAPABILITY
    if (SetNxECapabilities(ciscoNSEHandler, m_localMediaFormats, m_answerFormatList, OpalCiscoNSE))
      ++maxFormats;
#endif
  }

  if (mediaDescription->GetSDPMediaFormats().GetSize() > maxFormats)
    multipleFormats = true;

  PTRACE_IF(3, otherSidesDir == SDPMediaDescription::Inactive, "SIP\tNo streams opened as " << mediaType << " inactive");
  return true;
}


void SIPConnection::OnCreatingINVITE(SIPInvite & request)
{
  PTRACE(3, "SIP\tCreating INVITE request");

  SIPMIMEInfo & mime = request.GetMIME();

  switch (m_prackMode) {
    case e_prackDisabled :
      break;

    case e_prackRequired :
      mime.AddRequire("100rel");
      // Then add supported as well

    case e_prackSupported :
      mime.AddSupported("100rel");
  }

  mime.AddSupported("replaces");
  for (PINDEX i = 0; i < m_stringOptions.GetSize(); ++i) {
    PCaselessString key = m_stringOptions.GetKeyAt(i);
    if (key.NumCompare(HeaderPrefix) == EqualTo) {
      PString data = m_stringOptions.GetDataAt(i);
      if (!data.IsEmpty()) {
        mime.SetAt(key.Mid(HeaderPrefix.GetLength()), m_stringOptions.GetDataAt(i));
        if (key == SIP_HEADER_REPLACES)
          mime.AddRequire("replaces");
      }
    }
  }

  if (IsPresentationBlocked()) {
    // Should do more as per RFC3323, but this is all for now
    SIPURL from = mime.GetFrom();
    if (!from.GetDisplayName(false).IsEmpty())
      from.SetDisplayName("Anonymous");
    mime.SetFrom(from.AsQuotedString());
  }

  PString externalSDP = m_stringOptions(OPAL_OPT_EXTERNAL_SDP);
  if (!externalSDP.IsEmpty())
    request.SetEntityBody(externalSDP);
  else if (m_stringOptions.GetBoolean(OPAL_OPT_INITIAL_OFFER, true)) {
    if (m_needReINVITE)
      ++m_sdpVersion;

    SDPSessionDescription * sdp = new SDPSessionDescription(m_sdpSessionId, m_sdpVersion, OpalTransportAddress());
    if (OnSendOfferSDP(request.GetSessionManager(), *sdp, m_needReINVITE))
      request.SetSDP(sdp);
    else {
      delete sdp;
      Release(EndedByCapabilityExchange);
    }
  }
}


PBoolean SIPConnection::ForwardCall (const PString & fwdParty)
{
  if (fwdParty.IsEmpty ())
    return false;
  
  m_forwardParty = fwdParty;
  PTRACE(2, "SIP\tIncoming SIP connection will be forwarded to " << m_forwardParty);
  Release(EndedByCallForwarded);

  return true;
}


bool SIPConnection::SendInviteOK()
{
  PString externalSDP = m_stringOptions(OPAL_OPT_EXTERNAL_SDP);
  if (externalSDP.IsEmpty()) {
    SDPSessionDescription sdpOut(m_sdpSessionId, ++m_sdpVersion, GetDefaultSDPConnectAddress());
    if (!OnSendAnswerSDP(m_rtpSessions, sdpOut))
      return false;
    return SendInviteResponse(SIP_PDU::Successful_OK, &sdpOut);
  }

  SIP_PDU response(*originalInvite, SIP_PDU::Successful_OK);
  AdjustInviteResponse(response);

  response.SetEntityBody(externalSDP);
  return originalInvite->SendResponse(*transport, response); 
}


PBoolean SIPConnection::SendInviteResponse(SIP_PDU::StatusCodes code,
                                           const SDPSessionDescription * sdp)
{
  if (originalInvite == NULL)
    return true;

  SIP_PDU response(*originalInvite, code, sdp);
  AdjustInviteResponse(response);

  if (sdp != NULL)
    response.GetSDP(m_localMediaFormats)->SetSessionName(response.GetMIME().GetUserAgent());

  return originalInvite->SendResponse(*transport, response); 
}


void SIPConnection::AdjustInviteResponse(SIP_PDU & response)
{
  SIPMIMEInfo & mime = response.GetMIME();
  mime.SetProductInfo(endpoint.GetUserAgent(), GetProductInfo());
  response.SetAllow(GetAllowedMethods());

  endpoint.AdjustToRegistration(response, *transport, this);

  if (!m_ciscoRemotePartyID.IsEmpty()) {
    SIPURL party(mime.GetContact());
    party.GetFieldParameters().RemoveAll();
    mime.Set(RemotePartyID, party.AsQuotedString());
  }

  // this can be used to promote any incoming calls to TCP. Not quite there yet, but it *almost* works
  bool promoteToTCP = false;    // disable code for now
  if (promoteToTCP && (transport == NULL || strcmp(transport->GetProtoPrefix(), "tcp$") != 0)) {
    // see if endpoint contains a TCP listener we can use
    OpalTransportAddress newAddr;
    if (endpoint.FindListenerForProtocol("tcp", newAddr)) {
      response.GetMIME().SetContact(SIPURL(PString::Empty(), newAddr, 0).AsQuotedString());
      PTRACE(3, "SIP\tPromoting connection to TCP");
    }
  }

  if (response.GetStatusCode() == SIP_PDU::Information_Ringing) {
    if (m_allowedEvents.GetSize() > 0) {
      PStringStream strm; strm << setfill(',') << m_allowedEvents;
      mime.SetAllowEvents(strm);
    }
    mime.SetAlertInfo(m_alertInfo, m_appearanceCode);
  }

  if (response.GetStatusCode() >= 200) {
    // If sending final response, any pending unsent responses no longer need to be sent.
    // But the last sent response still needs to be acknowledged, so leave it in the queue.
    while (m_responsePackets.size() > 1)
      m_responsePackets.pop();

    m_responsePackets.push(response);
  }
  else if (m_prackEnabled) {
    mime.AddRequire("100rel");

    if (m_prackSequenceNumber == 0)
      m_prackSequenceNumber = PRandom::Number(0x40000000); // as per RFC 3262
    mime.SetAt("RSeq", PString(PString::Unsigned, ++m_prackSequenceNumber));

    m_responsePackets.push(response);
  }

  switch (m_responsePackets.size()) {
    case 0 :
      break;

    case 1 :
      m_responseRetryCount = 0;
      m_responseRetryTimer = endpoint.GetRetryTimeoutMin();
      m_responseFailTimer = endpoint.GetAckTimeout();
      break;
  }
}


void SIPConnection::OnInviteResponseRetry(PTimer &, INT)
{
  PSafeLockReadWrite safeLock(*this);
  if (safeLock.IsLocked() && originalInvite != NULL && !m_responsePackets.empty()) {
    PTRACE(3, "SIP\t" << (m_responsePackets.front().GetStatusCode() < 200 ? "PRACK" : "ACK")
           << " not received yet, retry " << m_responseRetryCount << " sending response for " << *this);

    PTimeInterval timeout = endpoint.GetRetryTimeoutMin()*(1 << ++m_responseRetryCount);
    if (timeout > endpoint.GetRetryTimeoutMax())
      timeout = endpoint.GetRetryTimeoutMax();
    m_responseRetryTimer = timeout;

    originalInvite->SendResponse(*transport, m_responsePackets.front()); // Not really a resonse but the function will work  }
  }
}


void SIPConnection::OnInviteResponseTimeout(PTimer &, INT)
{
  PSafeLockReadWrite safeLock(*this);
  if (safeLock.IsLocked() && !m_responsePackets.empty()) {
    PTRACE(1, "SIP\tFailed to receive "
           << (m_responsePackets.front().GetStatusCode() < 200 ? "PRACK" : "ACK")
           << " for " << *this);

    m_responseRetryTimer.Stop(false);

    if (IsReleased()) {
      // Clear out pending responses if we are releasing, just die now.
      while (!m_responsePackets.empty())
        m_responsePackets.pop();
    }
    else {
      if (m_responsePackets.front().GetStatusCode() < 200)
        SendInviteResponse(SIP_PDU::Failure_ServerTimeout);
      else {
        releaseMethod = ReleaseWithBYE;
        Release(EndedByTemporaryFailure);
      }
    }
  }
}


void SIPConnection::OnReceivedPRACK(SIP_PDU & request)
{
  PStringArray rack = request.GetMIME().GetString("RAck").Tokenise(" \r\n\t", false);
  if (rack.GetSize() != 3) {
    request.SendResponse(*transport, SIP_PDU::Failure_BadRequest);
    return;
  }

  if (originalInvite == NULL ||
      originalInvite->GetMIME().GetCSeqIndex() != rack[1].AsUnsigned() ||
      !(rack[2] *= "INVITE") ||
      m_responsePackets.empty() ||
      m_responsePackets.front().GetMIME().GetString("RSeq").AsUnsigned() != rack[0].AsUnsigned()) {
    request.SendResponse(*transport, SIP_PDU::Failure_TransactionDoesNotExist);
    return;
  }

  m_responseFailTimer.Stop(false); // Asynchronous stop to avoid deadlock
  m_responseRetryTimer.Stop(false);

  request.SendResponse(*transport, SIP_PDU::Successful_OK);

  // Got PRACK for our response, pop it off and send next, if on
  m_responsePackets.pop();

  if (!m_responsePackets.empty()) {
    m_responseRetryCount = 0;
    m_responseRetryTimer = endpoint.GetRetryTimeoutMin();
    m_responseFailTimer = endpoint.GetAckTimeout();
    originalInvite->SendResponse(*transport, m_responsePackets.front());
  }

  OnReceivedAnswerSDP(request);
}


void SIPConnection::OnRTPStatistics(const RTP_Session & session) const
{
  endpoint.OnRTPStatistics(*this, session);
}


void SIPConnection::OnUserInputInlineRFC2833(OpalRFC2833Info & info, INT type)
{
  switch (m_receivedUserInputMethod) {
    case ReceivedINFO :
      PTRACE(3, "OpalCon\tUsing INFO, ignoring RFC2833 on " << *this);
      break;

    case UserInputMethodUnknown :
      m_receivedUserInputMethod = ReceivedRFC2833;
      // Do default case

    default:
      OpalRTPConnection::OnUserInputInlineRFC2833(info, type);
  }
}


void SIPConnection::OnReceivedINFO(SIP_PDU & request)
{
  SIP_PDU::StatusCodes status = SIP_PDU::Failure_UnsupportedMediaType;
  SIPMIMEInfo & mimeInfo = request.GetMIME();
  PCaselessString contentType = mimeInfo.GetContentType();

  if (contentType.NumCompare(ApplicationDTMFRelayKey) == EqualTo) {
    switch (m_receivedUserInputMethod) {
      case ReceivedRFC2833 :
        PTRACE(3, "OpalCon\tUsing RFC2833, ignoring INFO " << ApplicationDTMFRelayKey << " on " << *this);
        break;

      case UserInputMethodUnknown :
        m_receivedUserInputMethod = ReceivedINFO;
        // Do default case

      default:
        PStringArray lines = request.GetEntityBody().Lines();
        PINDEX i;
        char tone = -1;
        int duration = -1;
        for (i = 0; i < lines.GetSize(); ++i) {
          PStringArray tokens = lines[i].Tokenise('=', PFalse);
          PString val;
          if (tokens.GetSize() > 1)
            val = tokens[1].Trim();
          if (tokens.GetSize() > 0) {
            if (tokens[0] *= "signal")
              tone = val[0];   // DTMF relay does not use RFC2833 encoding
            else if (tokens[0] *= "duration")
              duration = val.AsInteger();
          }
        }
        if (tone != -1)
          OnUserInputTone(tone, duration == 0 ? 100 : duration);
        status = SIP_PDU::Successful_OK;
        break;
    }
  }

  else if (contentType.NumCompare(ApplicationDTMFKey) == EqualTo) {
    switch (m_receivedUserInputMethod) {
      case ReceivedRFC2833 :
        PTRACE(3, "OpalCon\tUsing RFC2833, ignoring INFO " << ApplicationDTMFKey << " on " << *this);
        break;

      case UserInputMethodUnknown :
        m_receivedUserInputMethod = ReceivedINFO;
        // Do default case

      default:
        PString tones = request.GetEntityBody().Trim();
        if (tones.GetLength() == 1)
          OnUserInputTone(tones[0], 100);
        else
          OnUserInputString(tones);
        status = SIP_PDU::Successful_OK;
    }
  }

#if OPAL_VIDEO
  else if (contentType.NumCompare(ApplicationMediaControlXMLKey) == EqualTo) {
    if (OnMediaControlXML(request))
      return;
    status = SIP_PDU::Failure_UnsupportedMediaType;
  }
#endif

  request.SendResponse(*transport, status);

  if (status == SIP_PDU::Successful_OK) {
    // Have INFO user input, disable the in-band tone detcetor to avoid double detection
    m_detectInBandDTMF = false;

    OpalMediaStreamPtr stream = GetMediaStream(OpalMediaType::Audio(), true);
    if (stream != NULL && stream->RemoveFilter(m_dtmfDetectNotifier, OPAL_PCM16)) {
      PTRACE(4, "OpalCon\tRemoved detect DTMF filter on connection " << *this);
    }
  }
}


void SIPConnection::OnReceivedPING(SIP_PDU & request)
{
  PTRACE(3, "SIP\tReceived PING");
  request.SendResponse(*transport, SIP_PDU::Successful_OK);
}


void SIPConnection::OnReceivedMESSAGE(SIP_PDU & pdu)
{
  PTRACE(3, "SIP\tReceived MESSAGE in the context of a call");

// TODO: debug when we need connection based MESSAGE
#if 0
  OpalIM * im = new OpalIM;
  const SIPMIMEInfo & mime = pdu.GetMIME();

  im->m_from  = mime.GetFrom();
  im->m_to    = mime.GetTo();

  im->m_conversationId = PString("sip_") + mime.GetCallID();

  // populate the message type
  im->m_mimeType = mime.GetContentType();
  if (im->m_mimeType.IsEmpty())
    im->m_mimeType = PMIMEInfo::TextPlain();

  // dispatch the message
  OpalIMContext::SentStatus stat = endpoint.GetManager().GetIMManager().OnIncomingMessage(im, conversationId, PSafePtr<OpalConnection>(this));

  pdu.SendResponse(*transport, stat ? SIP_PDU::Successful_OK : SIP_PDU::Failure_BadRequest);

#endif

  pdu.SendResponse(*transport, SIP_PDU::Failure_BadRequest);
}


OpalConnection::SendUserInputModes SIPConnection::GetRealSendUserInputMode() const
{
  switch (sendUserInputMode) {
    case SendUserInputAsProtocolDefault :
    case SendUserInputAsRFC2833 :
      if (m_remoteFormatList.HasFormat(OpalRFC2833))
        return SendUserInputAsRFC2833;
      PTRACE(3, "SIP\tSendUserInputMode for RFC2833 requested, but unavailable at remote.");
      return SendUserInputAsString;

    case NumSendUserInputModes :
    case SendUserInputAsQ931 :
      return SendUserInputAsTone;

    case SendUserInputAsString :
    case SendUserInputAsTone :
    case SendUserInputInBand :
      break;
  }

  return sendUserInputMode;
}


PBoolean SIPConnection::SendUserInputString(const PString & value)
{
  if (GetRealSendUserInputMode() == SendUserInputAsString) {
    SIPInfo::Params params;
    params.m_contentType = ApplicationDTMFKey;
    params.m_body = value;
    if (SendINFO(params))
      return true;
  }

  return OpalRTPConnection::SendUserInputString(value);
}


PBoolean SIPConnection::SendUserInputTone(char tone, unsigned duration)
{
  if (m_holdFromRemote || m_holdToRemote >= eHoldOn)
    return false;

  SendUserInputModes mode = GetRealSendUserInputMode();

  PTRACE(3, "SIP\tSendUserInputTone('" << tone << "', " << duration << "), using mode " << mode);

  SIPInfo::Params params;

  switch (mode) {
    case SendUserInputAsTone :
      {
        params.m_contentType = ApplicationDTMFRelayKey;
        PStringStream strm;
        strm << "Signal= " << tone << "\r\n" << "Duration= " << duration << "\r\n";  // spaces are important. Who can guess why?
        params.m_body = strm;
      }
      break;

    case SendUserInputAsString :
      params.m_contentType = ApplicationDTMFKey;
      params.m_body = tone;
      break;

    default :
      return OpalRTPConnection::SendUserInputTone(tone, duration);
  }

  if (SendINFO(params))
    return true;

  PTRACE(2, "SIP\tCould not send tone '" << tone << "' via INFO.");
  return OpalRTPConnection::SendUserInputTone(tone, duration);
}


bool SIPConnection::SendOPTIONS(const SIPOptions::Params & params, SIP_PDU * reply)
{
  if ((m_allowedMethods&(1<<SIP_PDU::Method_OPTIONS)) == 0) {
    PTRACE(2, "SIP\tRemote does not allow OPTIONS message.");
    return false;
  }

  PSafePtr<SIPTransaction> transaction = new SIPOptions(*this, params);
  if (reply == NULL)
    return transaction->Start();

  m_responses[transaction->GetTransactionID()] = reply;
  transaction->WaitForCompletion();
  return !transaction->IsFailed();
}


bool SIPConnection::SendINFO(const SIPInfo::Params & params, SIP_PDU * reply)
{
  if ((m_allowedMethods&(1<<SIP_PDU::Method_INFO)) == 0) {
    PTRACE(2, "SIP\tRemote does not allow INFO message.");
    return false;
  }

  PSafePtr<SIPTransaction> transaction = new SIPInfo(*this, params);
  if (reply == NULL)
    return transaction->Start();

  m_responses[transaction->GetTransactionID()] = reply;
  transaction->WaitForCompletion();
  return !transaction->IsFailed();
}


#if 0 // OPAL_HAS_IM

bool SIPConnection::TransmitExternalIM(const OpalMediaFormat & /*format*/, RTP_IMFrame & body)
{
#if OPAL_HAS_MSRP
  // if the call contains an MSRP connection, then use that
  for (OpalMediaStreamPtr mediaStream(mediaStreams, PSafeReference); mediaStream != NULL; ++mediaStream) {
    if (mediaStream->IsSink() && (mediaStream->GetMediaFormat() == OpalMSRP)) {
      PTRACE(3, "SIP\tSending MSRP packet within call");
      mediaStream.SetSafetyMode(PSafeReadWrite);
      int written;
      return mediaStream->WriteData(body.GetPayloadPtr(), body.GetPayloadSize(), written);
    }
  }
#endif

  if ((m_allowedMethods&(1<<SIP_PDU::Method_MESSAGE)) == 0) {
    PTRACE(2, "SIP\tRemote does not allow MESSAGE message.");
    return false;
  }

  PTRACE(3, "SIP\tSending MESSAGE within call");

  // else send as MESSAGE
  SIPMessage::Params params;
  params.m_body = body.AsString();

  PSafePtr<SIPTransaction> transaction = new SIPMessage(*this, params);
  return transaction->Start();
}

#endif

bool SIPConnection::OnMediaCommand(OpalMediaStream & stream, const OpalMediaCommand & command)
{
  bool done = OpalRTPConnection::OnMediaCommand(stream, command);

#if OPAL_VIDEO
  if (PIsDescendant(&command, OpalVideoUpdatePicture)) {
    SIPInfo::Params params(ApplicationMediaControlXMLKey,
                           "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
                           "<media_control>"
                            "<vc_primitive>"
                             "<to_encoder>"
                              "<picture_fast_update>"
                              "</picture_fast_update>"
                             "</to_encoder>"
                            "</vc_primitive>"
                           "</media_control>");
    SendINFO(params);
    done = true;
  }
#endif

  return done;
}


#if OPAL_VIDEO

PBoolean SIPConnection::OnMediaControlXML(SIP_PDU & request)
{
  // Must always send OK, even if not OK
  request.SendResponse(*transport, SIP_PDU::Successful_OK);

#if OPAL_PTLIB_EXPAT

  PXML xml;
  PXMLElement * element;
  if (xml.Load(request.GetEntityBody()) &&
      xml.GetRootElement()->GetName() == "media_control" &&
     (element =      xml.GetElement("vc_primitive")) != NULL &&
     (element = element->GetElement("to_encoder")) != NULL &&
                element->GetElement("picture_fast_update")  != NULL)

#else // OPAL_PTLIB_EXPAT

  PCaselessString body = request.GetEntityBody();
  PINDEX pos;
  if ((pos = body.Find("media_control"           )) != P_MAX_INDEX &&
      (pos = body.Find("vc_primitive"       , pos)) != P_MAX_INDEX &&
      (pos = body.Find("to_encoder"         , pos)) != P_MAX_INDEX &&
             body.Find("picture_fast_update", pos)  != P_MAX_INDEX)

#endif // OPAL_PTLIB_EXPAT

    SendVideoUpdatePicture(0, 0);
  else {
    PTRACE(3, "SIP\tUnable to parse received PictureFastUpdate");
    // Error is sent in separate INFO message as per RFC5168
    SIPInfo::Params params(ApplicationMediaControlXMLKey,
                           "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
                           "<media_control>"
                             "<general_error>"
                               "Unable to parse XML request"
                             "</general_error>"
                           "</media_control>");
    SendINFO(params);
  }

  return true;
}

#endif // OPAL_VIDEO


unsigned SIPConnection::GetAllowedMethods() const
{
  unsigned methods = endpoint.GetAllowedMethods();
  if (GetPRACKMode() == e_prackDisabled)
    methods &= ~(1<<SIP_PDU::Method_PRACK);
  else
    methods |= (1<<SIP_PDU::Method_PRACK);
  return methods;
}


/////////////////////////////////////////////////////////////////////////////

SIP_RTP_Session::SIP_RTP_Session(SIPConnection & conn)
  : connection(conn)
{
}


void SIP_RTP_Session::OnTxStatistics(const RTP_Session & session) const
{
  connection.OnRTPStatistics(session);
}


void SIP_RTP_Session::OnRxStatistics(const RTP_Session & session) const
{
  connection.OnRTPStatistics(session);
}


#if OPAL_VIDEO

void SIP_RTP_Session::OnRxIntraFrameRequest(const RTP_Session & session) const
{
  connection.SendVideoUpdatePicture(session.GetSessionID());
}


void SIP_RTP_Session::OnTxIntraFrameRequest(const RTP_Session & /*session*/) const
{
}

#endif // OPAL_VIDEO


void SIP_RTP_Session::SessionFailing(RTP_Session & session)
{
  // Some systems, e.g. Cisco, stop listening to media while doing a transfer
  // so don't fail the call while transfer is in progress
  if (!connection.m_referInProgress)
    connection.SessionFailing(session);
}


void SIPConnection::OnSessionTimeout(PTimer &, INT)
{
  //SIPTransaction * invite = new SIPInvite(*this, *transport, rtpSessions);  
  //invite->Start();  
  //sessionTimer = 10000;
}


PString SIPConnection::GetLocalPartyURL() const
{
  if (m_contactAddress.IsEmpty())
    return OpalRTPConnection::GetLocalPartyURL();

  SIPURL url = m_contactAddress;
  url.Sanitise(SIPURL::ExternalURI);
  return url.AsString();
}


#endif // OPAL_SIP


// End of file ////////////////////////////////////////////////////////////////
