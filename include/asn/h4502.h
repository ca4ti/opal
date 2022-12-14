//
// h4502.h
//
// Code automatically generated by asnparse.
//

#include <opal/buildopts.h>

#if ! H323_DISABLE_H4502

#ifndef __H4502_H
#define __H4502_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#include <ptclib/asner.h>

#include "h4501.h"
#include "h4501.h"
#include "h4501.h"
#include "h225.h"
#include "h4501.h"


//
// DummyArg
//

class H4502_ExtensionSeq;
class H225_NonStandardParameter;

class H4502_DummyArg : public PASN_Choice
{
#ifndef PASN_LEANANDMEAN
    PCLASSINFO(H4502_DummyArg, PASN_Choice);
#endif
  public:
    H4502_DummyArg(unsigned tag = 0, TagClass tagClass = UniversalTagClass);

    enum Choices {
      e_extensionSeq,
      e_nonStandardData
    };

#if defined(__GNUC__) && __GNUC__ <= 2 && __GNUC_MINOR__ < 9
    operator H4502_ExtensionSeq &() const;
#else
    operator H4502_ExtensionSeq &();
    operator const H4502_ExtensionSeq &() const;
#endif
#if defined(__GNUC__) && __GNUC__ <= 2 && __GNUC_MINOR__ < 9
    operator H225_NonStandardParameter &() const;
#else
    operator H225_NonStandardParameter &();
    operator const H225_NonStandardParameter &() const;
#endif

    PBoolean CreateObject();
    PObject * Clone() const;
};


//
// DummyRes
//

class H4502_ExtensionSeq;
class H225_NonStandardParameter;

class H4502_DummyRes : public PASN_Choice
{
#ifndef PASN_LEANANDMEAN
    PCLASSINFO(H4502_DummyRes, PASN_Choice);
#endif
  public:
    H4502_DummyRes(unsigned tag = 0, TagClass tagClass = UniversalTagClass);

    enum Choices {
      e_extensionSeq,
      e_nonStandardData
    };

#if defined(__GNUC__) && __GNUC__ <= 2 && __GNUC_MINOR__ < 9
    operator H4502_ExtensionSeq &() const;
#else
    operator H4502_ExtensionSeq &();
    operator const H4502_ExtensionSeq &() const;
#endif
#if defined(__GNUC__) && __GNUC__ <= 2 && __GNUC_MINOR__ < 9
    operator H225_NonStandardParameter &() const;
#else
    operator H225_NonStandardParameter &();
    operator const H225_NonStandardParameter &() const;
#endif

    PBoolean CreateObject();
    PObject * Clone() const;
};


//
// EndDesignation
//

class H4502_EndDesignation : public PASN_Enumeration
{
#ifndef PASN_LEANANDMEAN
    PCLASSINFO(H4502_EndDesignation, PASN_Enumeration);
#endif
  public:
    H4502_EndDesignation(unsigned tag = UniversalEnumeration, TagClass tagClass = UniversalTagClass);

    enum Enumerations {
      e_primaryEnd,
      e_secondaryEnd
    };

    H4502_EndDesignation & operator=(unsigned v);
    PObject * Clone() const;
};


//
// CallStatus
//

class H4502_CallStatus : public PASN_Enumeration
{
#ifndef PASN_LEANANDMEAN
    PCLASSINFO(H4502_CallStatus, PASN_Enumeration);
#endif
  public:
    H4502_CallStatus(unsigned tag = UniversalEnumeration, TagClass tagClass = UniversalTagClass);

    enum Enumerations {
      e_answered,
      e_alerting
    };

    H4502_CallStatus & operator=(unsigned v);
    PObject * Clone() const;
};


//
// CallIdentity
//

class H4502_CallIdentity : public PASN_NumericString
{
#ifndef PASN_LEANANDMEAN
    PCLASSINFO(H4502_CallIdentity, PASN_NumericString);
#endif
  public:
    H4502_CallIdentity(unsigned tag = UniversalNumericString, TagClass tagClass = UniversalTagClass);

    H4502_CallIdentity & operator=(const char * v);
    H4502_CallIdentity & operator=(const PString & v);
    PObject * Clone() const;
};


//
// ExtensionSeq
//

class H4501_Extension;

class H4502_ExtensionSeq : public PASN_Array
{
#ifndef PASN_LEANANDMEAN
    PCLASSINFO(H4502_ExtensionSeq, PASN_Array);
#endif
  public:
    H4502_ExtensionSeq(unsigned tag = UniversalSequence, TagClass tagClass = UniversalTagClass);

    PASN_Object * CreateObject() const;
    H4501_Extension & operator[](PINDEX i) const;
    PObject * Clone() const;
};


//
// CallTransferOperation
//

class H4502_CallTransferOperation : public PASN_Enumeration
{
#ifndef PASN_LEANANDMEAN
    PCLASSINFO(H4502_CallTransferOperation, PASN_Enumeration);
#endif
  public:
    H4502_CallTransferOperation(unsigned tag = UniversalEnumeration, TagClass tagClass = UniversalTagClass);

    enum Enumerations {
      e_callTransferIdentify = 7,
      e_callTransferAbandon,
      e_callTransferInitiate,
      e_callTransferSetup,
      e_callTransferUpdate = 13,
      e_subaddressTransfer,
      e_callTransferComplete = 12,
      e_callTransferActive = 11
    };

    H4502_CallTransferOperation & operator=(unsigned v);
    PObject * Clone() const;
};


//
// CallTransferErrors
//

class H4502_CallTransferErrors : public PASN_Enumeration
{
#ifndef PASN_LEANANDMEAN
    PCLASSINFO(H4502_CallTransferErrors, PASN_Enumeration);
#endif
  public:
    H4502_CallTransferErrors(unsigned tag = UniversalEnumeration, TagClass tagClass = UniversalTagClass);

    enum Enumerations {
      e_unspecified = 1008,
      e_invalidReroutingNumber = 1004,
      e_unrecognizedCallIdentity,
      e_establishmentFailure
    };

    H4502_CallTransferErrors & operator=(unsigned v);
    PObject * Clone() const;
};


//
// CTInitiateArg_argumentExtension
//

class H4502_ExtensionSeq;
class H225_NonStandardParameter;

class H4502_CTInitiateArg_argumentExtension : public PASN_Choice
{
#ifndef PASN_LEANANDMEAN
    PCLASSINFO(H4502_CTInitiateArg_argumentExtension, PASN_Choice);
#endif
  public:
    H4502_CTInitiateArg_argumentExtension(unsigned tag = 0, TagClass tagClass = UniversalTagClass);

    enum Choices {
      e_extensionSeq,
      e_nonStandardData
    };

#if defined(__GNUC__) && __GNUC__ <= 2 && __GNUC_MINOR__ < 9
    operator H4502_ExtensionSeq &() const;
#else
    operator H4502_ExtensionSeq &();
    operator const H4502_ExtensionSeq &() const;
#endif
#if defined(__GNUC__) && __GNUC__ <= 2 && __GNUC_MINOR__ < 9
    operator H225_NonStandardParameter &() const;
#else
    operator H225_NonStandardParameter &();
    operator const H225_NonStandardParameter &() const;
#endif

    PBoolean CreateObject();
    PObject * Clone() const;
};


//
// CTSetupArg_argumentExtension
//

class H4502_ExtensionSeq;
class H225_NonStandardParameter;

class H4502_CTSetupArg_argumentExtension : public PASN_Choice
{
#ifndef PASN_LEANANDMEAN
    PCLASSINFO(H4502_CTSetupArg_argumentExtension, PASN_Choice);
#endif
  public:
    H4502_CTSetupArg_argumentExtension(unsigned tag = 0, TagClass tagClass = UniversalTagClass);

    enum Choices {
      e_extensionSeq,
      e_nonStandardData
    };

#if defined(__GNUC__) && __GNUC__ <= 2 && __GNUC_MINOR__ < 9
    operator H4502_ExtensionSeq &() const;
#else
    operator H4502_ExtensionSeq &();
    operator const H4502_ExtensionSeq &() const;
#endif
#if defined(__GNUC__) && __GNUC__ <= 2 && __GNUC_MINOR__ < 9
    operator H225_NonStandardParameter &() const;
#else
    operator H225_NonStandardParameter &();
    operator const H225_NonStandardParameter &() const;
#endif

    PBoolean CreateObject();
    PObject * Clone() const;
};


//
// CTIdentifyRes_resultExtension
//

class H4502_ExtensionSeq;
class H225_NonStandardParameter;

class H4502_CTIdentifyRes_resultExtension : public PASN_Choice
{
#ifndef PASN_LEANANDMEAN
    PCLASSINFO(H4502_CTIdentifyRes_resultExtension, PASN_Choice);
#endif
  public:
    H4502_CTIdentifyRes_resultExtension(unsigned tag = 0, TagClass tagClass = UniversalTagClass);

    enum Choices {
      e_extensionSeq,
      e_nonStandardData
    };

#if defined(__GNUC__) && __GNUC__ <= 2 && __GNUC_MINOR__ < 9
    operator H4502_ExtensionSeq &() const;
#else
    operator H4502_ExtensionSeq &();
    operator const H4502_ExtensionSeq &() const;
#endif
#if defined(__GNUC__) && __GNUC__ <= 2 && __GNUC_MINOR__ < 9
    operator H225_NonStandardParameter &() const;
#else
    operator H225_NonStandardParameter &();
    operator const H225_NonStandardParameter &() const;
#endif

    PBoolean CreateObject();
    PObject * Clone() const;
};


//
// CTUpdateArg_argumentExtension
//

class H4502_ExtensionSeq;
class H225_NonStandardParameter;

class H4502_CTUpdateArg_argumentExtension : public PASN_Choice
{
#ifndef PASN_LEANANDMEAN
    PCLASSINFO(H4502_CTUpdateArg_argumentExtension, PASN_Choice);
#endif
  public:
    H4502_CTUpdateArg_argumentExtension(unsigned tag = 0, TagClass tagClass = UniversalTagClass);

    enum Choices {
      e_extensionSeq,
      e_nonStandardData
    };

#if defined(__GNUC__) && __GNUC__ <= 2 && __GNUC_MINOR__ < 9
    operator H4502_ExtensionSeq &() const;
#else
    operator H4502_ExtensionSeq &();
    operator const H4502_ExtensionSeq &() const;
#endif
#if defined(__GNUC__) && __GNUC__ <= 2 && __GNUC_MINOR__ < 9
    operator H225_NonStandardParameter &() const;
#else
    operator H225_NonStandardParameter &();
    operator const H225_NonStandardParameter &() const;
#endif

    PBoolean CreateObject();
    PObject * Clone() const;
};


//
// SubaddressTransferArg_argumentExtension
//

class H4502_ExtensionSeq;
class H225_NonStandardParameter;

class H4502_SubaddressTransferArg_argumentExtension : public PASN_Choice
{
#ifndef PASN_LEANANDMEAN
    PCLASSINFO(H4502_SubaddressTransferArg_argumentExtension, PASN_Choice);
#endif
  public:
    H4502_SubaddressTransferArg_argumentExtension(unsigned tag = 0, TagClass tagClass = UniversalTagClass);

    enum Choices {
      e_extensionSeq,
      e_nonStandardData
    };

#if defined(__GNUC__) && __GNUC__ <= 2 && __GNUC_MINOR__ < 9
    operator H4502_ExtensionSeq &() const;
#else
    operator H4502_ExtensionSeq &();
    operator const H4502_ExtensionSeq &() const;
#endif
#if defined(__GNUC__) && __GNUC__ <= 2 && __GNUC_MINOR__ < 9
    operator H225_NonStandardParameter &() const;
#else
    operator H225_NonStandardParameter &();
    operator const H225_NonStandardParameter &() const;
#endif

    PBoolean CreateObject();
    PObject * Clone() const;
};


//
// CTCompleteArg_argumentExtension
//

class H4502_ExtensionSeq;
class H225_NonStandardParameter;

class H4502_CTCompleteArg_argumentExtension : public PASN_Choice
{
#ifndef PASN_LEANANDMEAN
    PCLASSINFO(H4502_CTCompleteArg_argumentExtension, PASN_Choice);
#endif
  public:
    H4502_CTCompleteArg_argumentExtension(unsigned tag = 0, TagClass tagClass = UniversalTagClass);

    enum Choices {
      e_extensionSeq,
      e_nonStandardData
    };

#if defined(__GNUC__) && __GNUC__ <= 2 && __GNUC_MINOR__ < 9
    operator H4502_ExtensionSeq &() const;
#else
    operator H4502_ExtensionSeq &();
    operator const H4502_ExtensionSeq &() const;
#endif
#if defined(__GNUC__) && __GNUC__ <= 2 && __GNUC_MINOR__ < 9
    operator H225_NonStandardParameter &() const;
#else
    operator H225_NonStandardParameter &();
    operator const H225_NonStandardParameter &() const;
#endif

    PBoolean CreateObject();
    PObject * Clone() const;
};


//
// CTActiveArg_argumentExtension
//

class H4502_ExtensionSeq;
class H225_NonStandardParameter;

class H4502_CTActiveArg_argumentExtension : public PASN_Choice
{
#ifndef PASN_LEANANDMEAN
    PCLASSINFO(H4502_CTActiveArg_argumentExtension, PASN_Choice);
#endif
  public:
    H4502_CTActiveArg_argumentExtension(unsigned tag = 0, TagClass tagClass = UniversalTagClass);

    enum Choices {
      e_extensionSeq,
      e_nonStandardData
    };

#if defined(__GNUC__) && __GNUC__ <= 2 && __GNUC_MINOR__ < 9
    operator H4502_ExtensionSeq &() const;
#else
    operator H4502_ExtensionSeq &();
    operator const H4502_ExtensionSeq &() const;
#endif
#if defined(__GNUC__) && __GNUC__ <= 2 && __GNUC_MINOR__ < 9
    operator H225_NonStandardParameter &() const;
#else
    operator H225_NonStandardParameter &();
    operator const H225_NonStandardParameter &() const;
#endif

    PBoolean CreateObject();
    PObject * Clone() const;
};


//
// CTInitiateArg
//

class H4502_CTInitiateArg : public PASN_Sequence
{
#ifndef PASN_LEANANDMEAN
    PCLASSINFO(H4502_CTInitiateArg, PASN_Sequence);
#endif
  public:
    H4502_CTInitiateArg(unsigned tag = UniversalSequence, TagClass tagClass = UniversalTagClass);

    enum OptionalFields {
      e_argumentExtension
    };

    H4502_CallIdentity m_callIdentity;
    H4501_EndpointAddress m_reroutingNumber;
    H4502_CTInitiateArg_argumentExtension m_argumentExtension;

    PINDEX GetDataLength() const;
    PBoolean Decode(PASN_Stream & strm);
    void Encode(PASN_Stream & strm) const;
#ifndef PASN_NOPRINTON
    void PrintOn(ostream & strm) const;
#endif
    Comparison Compare(const PObject & obj) const;
    PObject * Clone() const;
};


//
// CTSetupArg
//

class H4502_CTSetupArg : public PASN_Sequence
{
#ifndef PASN_LEANANDMEAN
    PCLASSINFO(H4502_CTSetupArg, PASN_Sequence);
#endif
  public:
    H4502_CTSetupArg(unsigned tag = UniversalSequence, TagClass tagClass = UniversalTagClass);

    enum OptionalFields {
      e_transferringNumber,
      e_argumentExtension
    };

    H4502_CallIdentity m_callIdentity;
    H4501_EndpointAddress m_transferringNumber;
    H4502_CTSetupArg_argumentExtension m_argumentExtension;

    PINDEX GetDataLength() const;
    PBoolean Decode(PASN_Stream & strm);
    void Encode(PASN_Stream & strm) const;
#ifndef PASN_NOPRINTON
    void PrintOn(ostream & strm) const;
#endif
    Comparison Compare(const PObject & obj) const;
    PObject * Clone() const;
};


//
// CTIdentifyRes
//

class H4502_CTIdentifyRes : public PASN_Sequence
{
#ifndef PASN_LEANANDMEAN
    PCLASSINFO(H4502_CTIdentifyRes, PASN_Sequence);
#endif
  public:
    H4502_CTIdentifyRes(unsigned tag = UniversalSequence, TagClass tagClass = UniversalTagClass);

    enum OptionalFields {
      e_resultExtension
    };

    H4502_CallIdentity m_callIdentity;
    H4501_EndpointAddress m_reroutingNumber;
    H4502_CTIdentifyRes_resultExtension m_resultExtension;

    PINDEX GetDataLength() const;
    PBoolean Decode(PASN_Stream & strm);
    void Encode(PASN_Stream & strm) const;
#ifndef PASN_NOPRINTON
    void PrintOn(ostream & strm) const;
#endif
    Comparison Compare(const PObject & obj) const;
    PObject * Clone() const;
};


//
// CTUpdateArg
//

class H4502_CTUpdateArg : public PASN_Sequence
{
#ifndef PASN_LEANANDMEAN
    PCLASSINFO(H4502_CTUpdateArg, PASN_Sequence);
#endif
  public:
    H4502_CTUpdateArg(unsigned tag = UniversalSequence, TagClass tagClass = UniversalTagClass);

    enum OptionalFields {
      e_redirectionInfo,
      e_basicCallInfoElements,
      e_argumentExtension
    };

    H4501_EndpointAddress m_redirectionNumber;
    PASN_BMPString m_redirectionInfo;
    H4501_H225InformationElement m_basicCallInfoElements;
    H4502_CTUpdateArg_argumentExtension m_argumentExtension;

    PINDEX GetDataLength() const;
    PBoolean Decode(PASN_Stream & strm);
    void Encode(PASN_Stream & strm) const;
#ifndef PASN_NOPRINTON
    void PrintOn(ostream & strm) const;
#endif
    Comparison Compare(const PObject & obj) const;
    PObject * Clone() const;
};


//
// SubaddressTransferArg
//

class H4502_SubaddressTransferArg : public PASN_Sequence
{
#ifndef PASN_LEANANDMEAN
    PCLASSINFO(H4502_SubaddressTransferArg, PASN_Sequence);
#endif
  public:
    H4502_SubaddressTransferArg(unsigned tag = UniversalSequence, TagClass tagClass = UniversalTagClass);

    enum OptionalFields {
      e_argumentExtension
    };

    H4501_PartySubaddress m_redirectionSubaddress;
    H4502_SubaddressTransferArg_argumentExtension m_argumentExtension;

    PINDEX GetDataLength() const;
    PBoolean Decode(PASN_Stream & strm);
    void Encode(PASN_Stream & strm) const;
#ifndef PASN_NOPRINTON
    void PrintOn(ostream & strm) const;
#endif
    Comparison Compare(const PObject & obj) const;
    PObject * Clone() const;
};


//
// CTCompleteArg
//

class H4502_CTCompleteArg : public PASN_Sequence
{
#ifndef PASN_LEANANDMEAN
    PCLASSINFO(H4502_CTCompleteArg, PASN_Sequence);
#endif
  public:
    H4502_CTCompleteArg(unsigned tag = UniversalSequence, TagClass tagClass = UniversalTagClass);

    enum OptionalFields {
      e_basicCallInfoElements,
      e_redirectionInfo,
      e_argumentExtension
    };

    H4502_EndDesignation m_endDesignation;
    H4501_EndpointAddress m_redirectionNumber;
    H4501_H225InformationElement m_basicCallInfoElements;
    PASN_BMPString m_redirectionInfo;
    H4502_CallStatus m_callStatus;
    H4502_CTCompleteArg_argumentExtension m_argumentExtension;

    PINDEX GetDataLength() const;
    PBoolean Decode(PASN_Stream & strm);
    void Encode(PASN_Stream & strm) const;
#ifndef PASN_NOPRINTON
    void PrintOn(ostream & strm) const;
#endif
    Comparison Compare(const PObject & obj) const;
    PObject * Clone() const;
};


//
// CTActiveArg
//

class H4502_CTActiveArg : public PASN_Sequence
{
#ifndef PASN_LEANANDMEAN
    PCLASSINFO(H4502_CTActiveArg, PASN_Sequence);
#endif
  public:
    H4502_CTActiveArg(unsigned tag = UniversalSequence, TagClass tagClass = UniversalTagClass);

    enum OptionalFields {
      e_basicCallInfoElements,
      e_connectedInfo,
      e_argumentExtension
    };

    H4501_EndpointAddress m_connectedAddress;
    H4501_H225InformationElement m_basicCallInfoElements;
    PASN_BMPString m_connectedInfo;
    H4502_CTActiveArg_argumentExtension m_argumentExtension;

    PINDEX GetDataLength() const;
    PBoolean Decode(PASN_Stream & strm);
    void Encode(PASN_Stream & strm) const;
#ifndef PASN_NOPRINTON
    void PrintOn(ostream & strm) const;
#endif
    Comparison Compare(const PObject & obj) const;
    PObject * Clone() const;
};


#endif // __H4502_H

#endif // if ! H323_DISABLE_H4502


// End of h4502.h
