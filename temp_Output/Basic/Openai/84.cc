#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CsmaTraceUdpApp");

struct TracePacket
{
  double time;
  uint32_t size;
};

std::vector<TracePacket> ReadTrace (std::string filename)
{
  std::vector<TracePacket> trace;
  std::ifstream infile (filename);
  if (!infile.is_open())
    {
      NS_FATAL_ERROR ("Cannot open trace file: " << filename);
    }
  std::string line;
  while (std::getline (infile, line))
    {
      if (line.empty ()) continue;
      std::istringstream iss (line);
      double t; uint32_t s;
      if (iss >> t >> s)
        {
          if (t >= 0.0 && s > 0)
            trace.push_back ({t, s});
        }
    }
  infile.close ();
  return trace;
}

class UdpTraceClient : public Application
{
public:
  UdpTraceClient ();
  virtual ~UdpTraceClient ();
  void Setup (Address address, uint16_t port, std::vector<TracePacket> trace, bool isIpv6);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTransmit ();
  void SendPacket ();

  Ptr<Socket>     m_socket;
  Address         m_peerAddress;
  uint16_t        m_peerPort;
  EventId         m_sendEvent;
  std::vector<TracePacket> m_trace;
  uint32_t        m_currentIndex;
  bool            m_running;
  bool            m_isIpv6;
  Time            m_startTime;
};

UdpTraceClient::UdpTraceClient ()
  : m_socket (0), m_peerPort (0), m_currentIndex (0),
    m_running (false), m_isIpv6 (false)
{
}

UdpTraceClient::~UdpTraceClient ()
{
  m_socket = 0;
}

void
UdpTraceClient::Setup (Address address, uint16_t port, std::vector<TracePacket> trace, bool isIpv6)
{
  m_peerAddress = address;
  m_peerPort = port;
  m_trace = trace;
  m_isIpv6 = isIpv6;
}

void
UdpTraceClient::StartApplication ()
{
  m_running = true;
  if (m_isIpv6)
    {
      m_socket = Socket::CreateSocket (GetNode (), TypeId::LookupByName ("ns3::UdpSocketFactory"));
    }
  else
    {
      m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    }
  m_socket->Connect (InetSocketAddress::ConvertFrom (m_peerAddress));
  m_currentIndex = 0;
  m_startTime = Simulator::Now ();
  ScheduleTransmit ();
}

void
UdpTraceClient::StopApplication ()
{
  m_running = false;
  if (m_sendEvent.IsRunning ())
    Simulator::Cancel (m_sendEvent);
  if (m_socket)
    m_socket->Close ();
}

void
UdpTraceClient::ScheduleTransmit ()
{
  if (!m_running || m_currentIndex >= m_trace.size ())
    return;

  double t = m_trace[m_currentIndex].time;
  Time nextTime = m_startTime + Seconds (t);
  Time now = Simulator::Now ();
  Time delay = nextTime > now ? nextTime - now : Seconds (0);
  m_sendEvent = Simulator::Schedule (delay, &UdpTraceClient::SendPacket, this);
}

void
UdpTraceClient::SendPacket ()
{
  if (!m_running || m_currentIndex >= m_trace.size ())
    return;
  uint32_t size = m_trace[m_currentIndex].size;
  Ptr<Packet> packet = Create<Packet> (size);
  m_socket->Send (packet);

  ++m_currentIndex;
  ScheduleTransmit ();
}

int
main (int argc, char *argv[])
{
  std::string traceFile = "trace.txt";
  bool useIpv6 = false;
  bool enableLog = false;

  CommandLine cmd;
  cmd.AddValue ("traceFile", "Path to traffic trace file (format: time size)", traceFile);
  cmd.AddValue ("useIpv6", "Enable IPv6 addressing", useIpv6);
  cmd.AddValue ("enableLog", "Enable logging (see logs with NS_LOG=CsmaTraceUdpApp:UdpServer:UdpTraceClient)", enableLog);
  cmd.Parse (argc, argv);

  if (enableLog)
    {
      LogComponentEnable ("CsmaTraceUdpApp", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpTraceClient", LOG_LEVEL_INFO);
    }

  std::vector<TracePacket> trace = ReadTrace (traceFile);

  NodeContainer nodes;
  nodes.Create (2);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  csma.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4InterfaceContainer ifs4;
  Ipv6InterfaceContainer ifs6;

  Address serverAddress;
  Address clientAddress;
  uint16_t port = 4000;

  if (useIpv6)
    {
      Ipv6AddressHelper ipv6;
      ipv6.SetBase ("2001:db8:0:1::", 64);
      ifs6 = ipv6.Assign (devices);
      ifs6.SetForwarding (0, true);
      ifs6.SetDefaultRouteInAllNodes (0);

      serverAddress = Inet6SocketAddress (ifs6.GetAddress (1,1), port);
      clientAddress = Inet6SocketAddress (ifs6.GetAddress (0,1), 0);
    }
  else
    {
      Ipv4AddressHelper ipv4;
      ipv4.SetBase ("10.1.1.0", "255.255.255.0");
      ifs4 = ipv4.Assign (devices);
      serverAddress = InetSocketAddress (ifs4.GetAddress (1), port);
      clientAddress = InetSocketAddress (ifs4.GetAddress (0), 0);
    }

  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (nodes.Get (1));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  Ptr<UdpTraceClient> clientApp = CreateObject<UdpTraceClient> ();
  clientApp->Setup (serverAddress, port, trace, useIpv6);
  nodes.Get (0)->AddApplication (clientApp);
  clientApp->SetStartTime (Seconds (2.0));
  clientApp->SetStopTime (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}