/*
 * main.cxx
 *
 * OPAL application source file for testing Opal presentities
 *
 * Copyright (c) 2009 Post Increment
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

#include <ptclib/cli.h>
#include <sip/sippres.h>

#if P_EXPAT
#else
#error Cannot compile Presentity test program without XML support!
#endif

#if OPAL_SIP
#else
#error Cannot compile Presentity test program without SIP support!
#endif


//////////////////////////////////////////////////////////////

class MyManager : public OpalManager
{
  PCLASSINFO(MyManager, OpalManager)
  public:
};


class TestPresEnt : public PProcess
{
  PCLASSINFO(TestPresEnt, PProcess)

  public:
    TestPresEnt();
    ~TestPresEnt();

    PDECLARE_AuthorisationRequestNotifier(TestPresEnt, AuthorisationRequest);
    PDECLARE_PresenceChangeNotifier(TestPresEnt, PresenceChange);

    virtual void Main();

    void AddPresentity(PArgList & args);

  private:
    PDECLARE_NOTIFIER(PCLI::Arguments, TestPresEnt, CmdCreate);
    PDECLARE_NOTIFIER(PCLI::Arguments, TestPresEnt, CmdList);
    PDECLARE_NOTIFIER(PCLI::Arguments, TestPresEnt, CmdSubscribeToPresence);
    PDECLARE_NOTIFIER(PCLI::Arguments, TestPresEnt, CmdUnsubscribeToPresence);
    PDECLARE_NOTIFIER(PCLI::Arguments, TestPresEnt, CmdPresenceAuthorisation);
    PDECLARE_NOTIFIER(PCLI::Arguments, TestPresEnt, CmdSetLocalPresence);
    PDECLARE_NOTIFIER(PCLI::Arguments, TestPresEnt, CmdBuddyList);
    PDECLARE_NOTIFIER(PCLI::Arguments, TestPresEnt, CmdBuddyAdd);
    PDECLARE_NOTIFIER(PCLI::Arguments, TestPresEnt, CmdBuddyRemove);
    PDECLARE_NOTIFIER(PCLI::Arguments, TestPresEnt, CmdBuddySusbcribe);
    PDECLARE_NOTIFIER(PCLI::Arguments, TestPresEnt, CmdDelay);
    PDECLARE_NOTIFIER(PCLI::Arguments, TestPresEnt, CmdQuit);

    MyManager * m_manager;
    PString     m_presenceAgent;
    PString     m_xcapRoot;
    PString     m_xcapAuthID;
    PString     m_xcapPassword;
    PDictionary<PString, OpalPresentity> m_presentities;
};


PCREATE_PROCESS(TestPresEnt);

TestPresEnt::TestPresEnt()
  : PProcess("OPAL Test Presentity", "TestPresEnt", OPAL_MAJOR, OPAL_MINOR, ReleaseCode, OPAL_BUILD)
  , m_manager(NULL)
{
  m_presentities.DisallowDeleteObjects();
}


TestPresEnt::~TestPresEnt()
{
  m_presentities.RemoveAll();
  delete m_manager; 
}


void TestPresEnt::AddPresentity(PArgList & args)
{
  PSafePtr<OpalPresentity> presentity = m_manager->AddPresentity(args[0]);
  if (presentity == NULL) {
    cerr << "error: cannot create presentity for \"" << args[0] << '"' << endl;
    return;
  }

  presentity->GetAttributes().Set(SIP_Presentity::SubProtocolKey,
            args.GetOptionString('s', !m_xcapRoot || !m_xcapAuthID || !m_xcapPassword ? "OMA" : "Agent"));

  presentity->SetAuthorisationRequestNotifier(PCREATE_AuthorisationRequestNotifier(AuthorisationRequest));
  presentity->SetPresenceChangeNotifier(PCREATE_PresenceChangeNotifier(PresenceChange));

  presentity->GetAttributes().Set(OpalPresentity::TimeToLiveKey,      "1200");
  if (args.HasOption('a'))
    presentity->GetAttributes().Set(SIP_Presentity::AuthNameKey,      args.GetOptionString('a'));
  if (args.HasOption('p'))
    presentity->GetAttributes().Set(SIP_Presentity::AuthPasswordKey,  args.GetOptionString('p'));
  if (!m_presenceAgent.IsEmpty())
    presentity->GetAttributes().Set(SIP_Presentity::PresenceAgentKey, m_presenceAgent);
  presentity->GetAttributes().Set(SIP_Presentity::XcapRootKey,        m_xcapRoot);
  if (!m_xcapAuthID)
    presentity->GetAttributes().Set(SIP_Presentity::XcapAuthIdKey,    m_xcapAuthID);
  if (!m_xcapPassword)
    presentity->GetAttributes().Set(SIP_Presentity::XcapPasswordKey,  m_xcapPassword);

  if (presentity->Open()) {
    m_presentities.SetAt(psprintf("#%u", m_presentities.GetSize()/2+1),    presentity);
    m_presentities.SetAt(PCaselessString(presentity->GetAOR().AsString()), presentity);
  }
  else {
    cerr << "error: cannot open presentity \"" << args[0] << '"' << endl;
    m_manager->RemovePresentity(args[0]);
  }
}


void TestPresEnt::Main()
{
  PArgList & args = GetArguments();
#define URL_OPTIONS "a-auth-id:" \
                    "p-password:" \
                    "s-sub-protocol:"
  args.Parse("h-help."
             "f-file:"
             "L-listener:"
             "P-presence-agent:"
#if PTRACING
             "o-output:"
#endif
             "S-stun:"
             "T-translation:"
#if PTRACING
             "t-trace."
#endif
             "X-xcap-server:"
             "-xcap-auth-id:"
             "-xcap-password:"
             "-proxy:"
             URL_OPTIONS);

#if PTRACING
  PTrace::Initialise(args.GetOptionCount('t'),
                     args.HasOption('o') ? (const char *)args.GetOptionString('o') : NULL,
         PTrace::Blocks | PTrace::Timestamp | PTrace::Thread | PTrace::FileAndLine);
#endif

  if (args.HasOption('h')) {
    cerr << "usage: " << GetFile().GetTitle() << " [ global-options ] { [ url-options ] url } ...\n"
            "\n"
            "Available global options are:\n"
            "  -proxy URL                   : set outbound proxy.\n"
            "  -f or --file filename        : name of script file to execute.\n"
            "  -L or --listener addr        : set listener address:port.\n"
            "  -T or --translation addr     : set NAT translation address.\n"
            "  -S or --stun addr            : set STUN server address.\n"
            "  -P or --presence-agent addr  : set presence agent default address.\n"
            "  -X or --xcap-root url        : set XCAP server root URL.\n"
            "        --xcap-auth-id name    : set XCAP server authorisation ID.\n"
            "        --xcap-password pwd    : set XCAP server authorisation password.\n"
#if PTRACING
            "  -o or --output file          : file name for output of log messages\n"       
            "  -t or --trace                : degree of verbosity in error log (more times for more detail)\n"     
#endif
            "  -h or --help                 : print this help message.\n"
            "\n"
            "Available URL options are:\n"
#define URL_HELP \
            "  -a or --auth-id              : Authorisation ID, default to URL username.\n" \
            "  -p or --password             : Authorisation password.\n" \
            "  -s or --sub-protocol proto   : set sub-protocol, one of PeerToPeer, Agent, XCAP or OMA\n"
            URL_HELP
            "\n"
            "e.g. " << GetFile().GetTitle() << " -X http://xcap.bloggs.com -p passone sip:fred1@bloggs.com -p passtwo sip:fred2@bloggs.com\n"
            ;
    return;
  }

  m_manager = new MyManager();

  if (args.HasOption('T'))
    m_manager->SetTranslationHost(args.GetOptionString('T'));

  PString listeners = args.GetOptionString('L');

  if (args.HasOption('S')) {
    cout << "Executing STUN lookup..." << flush;
    m_manager->SetSTUNServer(args.GetOptionString('S'));
    PSTUNClient * stun = m_manager->GetSTUNClient();
    if (stun != NULL)
      cout << stun->GetNatTypeName();
    else
      cout << "(unknown)";
    cout << "...done" << endl;
    if ((stun != NULL) && (stun->GetNatType() != PSTUNClient::OpenNat))
      listeners = "udp$*:*";
  }

  m_presenceAgent = args.GetOptionString('P');
  m_xcapRoot = args.GetOptionString('X');
  m_xcapAuthID = args.GetOptionString("xcap-auth-id");
  m_xcapPassword = args.GetOptionString("xcap-password");

  SIPEndPoint * sip  = new SIPEndPoint(*m_manager);
  if (!sip->StartListeners(listeners)) {
    cerr << "Could not start SIP listeners." << endl;
    return;
  }

  if (args.HasOption("proxy"))
    sip->SetProxy(args.GetOptionString("proxy"));

  PTextFile scriptFile;
  if (args.HasOption('f') && !scriptFile.Open(args.GetOptionString('f'))) {
    cerr << "error: cannot open script file '" << args.GetOptionString('f') << "'" << endl;
    return;
  }

  if (args.GetCount() != 0) {
    do {
      AddPresentity(args);
    } while (args.Parse(URL_OPTIONS));

    if (m_presentities.GetSize() == 0) {
      cerr << "error: no presentities available" << endl;
      return;
    }
  }

  PCLIStandard cli;
  cli.SetPrompt("PRES> ");
  cli.SetCommand("create", PCREATE_NOTIFIER(CmdCreate),
                 "Create presentity.",
                 "[ -a -p -s ] <url>\n" URL_HELP);
  cli.SetCommand("list", PCREATE_NOTIFIER(CmdList),
                 "List presentities.");
  cli.SetCommand("subscribe", PCREATE_NOTIFIER(CmdSubscribeToPresence),
                 "Subscribe to presence state for presentity.",
                 "<url-watcher> <url-watched>");
  cli.SetCommand("unsubscribe", PCREATE_NOTIFIER(CmdUnsubscribeToPresence),
                 "Subscribe to presence state for presentity.",
                 "<url-watcher> <url-watched>");
  cli.SetCommand("authorise", PCREATE_NOTIFIER(CmdPresenceAuthorisation),
                 "Authorise a presentity to see local presence.",
                 "<url-watched> <url-watcher> [ deny | deny-politely | remove ]");
  cli.SetCommand("publish", PCREATE_NOTIFIER(CmdSetLocalPresence),
                 "Publish local presence state for presentity.",
                 "<url> { available | unavailable | busy } [ <note> ]");
  cli.SetCommand("buddy list\nshow buddies", PCREATE_NOTIFIER(CmdBuddyList),
                 "Show buddy list for presentity.",
                 "<presentity>");
  cli.SetCommand("buddy add\nadd buddy", PCREATE_NOTIFIER(CmdBuddyAdd),
                 "Add buddy to list for presentity.",
                 "<presentity> <url-buddy> <display-name>");
  cli.SetCommand("buddy remove\ndel buddy", PCREATE_NOTIFIER(CmdBuddyRemove),
                 "Delete buddy from list for presentity.",
                 "<presentity> <url-buddy>");
  cli.SetCommand("buddy subscribe", PCREATE_NOTIFIER(CmdBuddySusbcribe),
                 "Susbcribe to all URIs in the buddy list for presentity.",
                 "<presentity>");
  cli.SetCommand("delay", PCREATE_NOTIFIER(CmdDelay),
                  "Delay for n seconds",
                  "secs");
  cli.SetCommand("quit\nq\nexit", PCREATE_NOTIFIER(CmdQuit),
                  "Quit command line interpreter, note quitting from console also shuts down application.");

  // create the foreground context
  PCLI::Context * cliContext = cli.StartForeground();
  if (cliContext == NULL)
    return;

  // if there is a script file, process commands
  if (scriptFile.IsOpen()) {
    cout << "Running script '" << scriptFile.GetFilePath() << "'" << endl;
    for (;;) {
      PString line;
      if (!scriptFile.ReadLine(line))
        break;
      line = line.Trim();
      if (line.GetLength() > 0) {
        if ((line[0] != '#') && (line[0] != ';') && ((line.GetLength() < 2) || (line[0] != '/') || (line[1] != '/'))) {
          cout << line << endl;
          if (!cliContext->ProcessInput(line)) {
            cerr << "error: error occurred while processing script" << endl;
            return;
          }
        }
      }
    }
    cout << "Script complete" << endl;
  }

  scriptFile.Close();

  cli.RunContext(cliContext); // Do not spawn thread, wait till end of input

  cout << "\nExiting ..." << endl;
}


void TestPresEnt::CmdCreate(PCLI::Arguments & args, INT)
{
  if (!args.Parse(URL_OPTIONS))
    args.WriteUsage();
  else if (m_presentities.Contains(args[0]))
    args.WriteError() << "Presentity \"" << args[0] << "\" already exists." << endl;
  else
    AddPresentity(args);
}


void TestPresEnt::CmdList(PCLI::Arguments & args, INT)
{
  PINDEX i;
  for (i = 0; i < m_presentities.GetSize(); ++i) {
    PString key = m_presentities.GetKeyAt(i);
    OpalPresentity & presentity = m_presentities[key];
    OpalPresenceInfo::State state;
    PString note;
    presentity.GetLocalPresence(state, note);
    cout << key << " "
         << presentity.GetAOR() << " "
         << OpalPresenceInfo::AsString(state) << " "
         << note
         << endl;
  }
}

void TestPresEnt::CmdSubscribeToPresence(PCLI::Arguments & args, INT)
{
  if (args.GetCount() < 2)
    args.WriteUsage();
  else if (!m_presentities.Contains(args[0]))
    args.WriteError() << "Presentity \"" << args[0] << "\" does not exist." << endl;
  else
    m_presentities[args[0]].SubscribeToPresence(args[1], true, "This is test presentity");
}


void TestPresEnt::CmdUnsubscribeToPresence(PCLI::Arguments & args, INT)
{
  if (args.GetCount() < 2)
    args.WriteUsage();
  else if (!m_presentities.Contains(args[0]))
    args.WriteError() << "Presentity \"" << args[0] << "\" does not exist." << endl;
  else
    m_presentities[args[0]].UnsubscribeFromPresence(args[1]);
}


void TestPresEnt::CmdPresenceAuthorisation(PCLI::Arguments & args, INT)
{
  OpalPresentity::Authorisation auth = OpalPresentity::AuthorisationPermitted;
  if (args.GetCount() > 2) {
    if (args[2] *= "deny")
      auth = OpalPresentity::AuthorisationDenied;
    else if (args[2] *= "deny-politely")
      auth = OpalPresentity::AuthorisationDeniedPolitely;
    else if (args[2] *= "remove")
      auth = OpalPresentity::AuthorisationRemove;
    else {
      args.WriteUsage();
      return;
    }
  }

  if (args.GetCount() < 2)
    args.WriteUsage();
  else if (!m_presentities.Contains(args[0]))
    args.WriteError() << "Presentity \"" << args[0] << "\" does not exist." << endl;
  else
    m_presentities[args[0]].SetPresenceAuthorisation(args[1], auth);
}


void TestPresEnt::CmdSetLocalPresence(PCLI::Arguments & args, INT)
{
  PString note;
  if (args.GetCount() > 2)
    note = args[2];

  if (args.GetCount() < 2)
    args.WriteUsage();
  else if (!m_presentities.Contains(args[0]))
    args.WriteError() << "Presentity \"" << args[0] << "\" does not exist." << endl;
  else if (args[1] *= "available")
    m_presentities[args[0]].SetLocalPresence(OpalPresenceInfo::Available, note);
  else if (args[1] *= "unavailable")
    m_presentities[args[0]].SetLocalPresence(OpalPresenceInfo::NoPresence, note);
  else if (args[1] *= "busy")
    m_presentities[args[0]].SetLocalPresence(OpalPresenceInfo::Busy, note);
  else
    args.WriteUsage();
}


void TestPresEnt::CmdBuddyList(PCLI::Arguments & args, INT)
{
  if (args.GetCount() < 1)
    args.WriteUsage();
  else if (!m_presentities.Contains(args[0]))
    args.WriteError() << "Presentity \"" << args[0] << "\" does not exist." << endl;
  else {
    OpalPresentity::BuddyList buddies;
    if (!m_presentities[args[0]].GetBuddyList(buddies))
      args.WriteError() << "Presentity \"" << args[0] << "\" does not support buddy lists." << endl;
    else if (buddies.empty())
      args.GetContext() << "Presentity \"" << args[0] << "\" has no buddies." << endl;
    else {
      for (OpalPresentity::BuddyList::iterator it = buddies.begin(); it != buddies.end(); ++it)
        args.GetContext() << it->m_presentity << "\t\"" << it->m_displayName << '"' << endl;
    }
  }
}


void TestPresEnt::CmdBuddyAdd(PCLI::Arguments & args, INT)
{
  if (args.GetCount() < 2)
    args.WriteUsage();
  else if (!m_presentities.Contains(args[0]))
    args.WriteError() << "Presentity \"" << args[0] << "\" does not exist." << endl;
  else {
    OpalPresentity::BuddyInfo buddy(args[1]);
    for (PINDEX arg = 2; arg < args.GetCount(); ++arg)
      buddy.m_displayName &= args[arg];
    if (!m_presentities[args[0]].SetBuddy(buddy))
      args.WriteError() << "Presentity \"" << args[0] << "\" does not have a buddy list." << endl;
  }
}


void TestPresEnt::CmdBuddyRemove(PCLI::Arguments & args, INT)
{
  if (args.GetCount() < 2)
    args.WriteUsage();
  else if (!m_presentities.Contains(args[0]))
    args.WriteError() << "Presentity \"" << args[0] << "\" does not exist." << endl;
  else if (!m_presentities[args[0]].DeleteBuddy(args[1]))
    args.WriteError() << "Could not delete \"" << args[1] << "\" for presentity \"" << args[0] << '"' << endl;
}


void TestPresEnt::CmdBuddySusbcribe(PCLI::Arguments & args, INT)
{
  if (args.GetCount() < 1)
    args.WriteUsage();
  else if (!m_presentities.Contains(args[0]))
    args.WriteError() << "Presentity \"" << args[0] << "\" does not exist." << endl;
  else if (!m_presentities[args[0]].SubscribeBuddyList())
    args.WriteError() << "Could not subscribe all buddies for presentity \"" << args[0] << '"' << endl;
}


void TestPresEnt::CmdDelay(PCLI::Arguments & args, INT)
{
  if (args.GetCount() < 1)
    args.WriteUsage();
  int delay = args[0].AsInteger();
  PThread::Sleep(delay * 1000);
}


void TestPresEnt::CmdQuit(PCLI::Arguments & args, INT)
{
  args.GetContext().Stop();
}


void TestPresEnt::AuthorisationRequest(OpalPresentity & presentity, const OpalPresentity::AuthorisationRequest & request)
{
  cout << request.m_presentity << " requesting access to presence for " << presentity.GetAOR() << endl;
  if (!request.m_note.IsEmpty())
    cout << "  \"" << request.m_note << '"' << endl;
}


void TestPresEnt::PresenceChange(OpalPresentity & presentity, const OpalPresenceInfo & info)
{
  cout << "Presentity " << presentity.GetAOR();
  if (info.m_entity != info.m_target)
    cout << " received presence change from " << info.m_entity;
  else
    cout << " changed locally";
  cout << " to " << info.AsString() << endl;
}



// End of File ///////////////////////////////////////////////////////////////
