#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/trace-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpTraceFileSimulation");

class UdpTraceClient : public Application
{
public:
  static TypeId GetTypeId (void);
  UdpTraceClient ();
  virtual ~UdpTraceClient ();

  void SetTraceFile (std::string filename);

protected:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

private:
  void SendPacket (void);
  Address m_peer;
  EventId m_sendEvent;
  bool m_running;
  Ptr<Socket> m_socket;
  DataRate m_dataRate;
  uint32_t m_packetSize;
  std::ifstream m_traceFile;
};

TypeId
UdpTraceClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("UdpTraceClient")
    .SetParent<Application> ()
    .AddConstructor<UdpTraceClient> ()
  ;
  return tid;
}

UdpTraceClient::UdpTraceClient ()
  : m_peer (),
    m_sendEvent (),
    m_running (false),
    m_socket (0),
    m_dataRate (5000000), // 5 Mbps
    m_packetSize (1024)
{
}

UdpTraceClient::~UdpTraceClient ()
{
  m_socket = 0;
  m_traceFile.close();
}

void
UdpTraceClient::SetTraceFile (std::string filename)
{
  m_traceFile.open(filename.c_str());
  if (!m_traceFile.is_open())
    {
      NS_FATAL_ERROR("Failed to open trace file: " << filename);
    }
}

void
UdpTraceClient::StartApplication (void)
{
  m_running = true;
  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());

  if (Ipv4Address::IsMatchingType (m_peer))
    {
      m_socket->Connect (m_peer);
    }
  else if (Ipv6Address::IsMatchingType (m_peer))
    {
      m_socket->Connect (m_peer);
    }

  SendPacket ();
}

void
UdpTraceClient::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
UdpTraceClient::SendPacket (void)
{
  if (!m_running)
    {
      return;
    }

  std::string line;
  if (!std::getline(m_traceFile, line))
    {
      return; // End of file
    }

  double delay = atof(line.c_str());

  Time interPacketInterval = Seconds(delay);
  Ptr<Packet> packet = Create<Packet>(m_packetSize);

  m_socket->Send (packet);

  m_sendEvent = Simulator::Schedule (interPacketInterval, &UdpTraceClient::SendPacket, this);
}

int main(int argc, char *argv[])
{
  bool enableLogging = false;
  bool useIpv6 = false;
  std::string traceFile = "trace.txt";

  CommandLine cmd (__FILE__);
  cmd.AddValue ("logging", "Enable logging", enableLogging);
  cmd.AddValue ("ipv6", "Use IPv6 addressing", useIpv6);
  cmd.AddValue ("tracefile", "Trace file name", traceFile);
  cmd.Parse (argc, argv);

  if (enableLogging)
    {
      LogComponentEnable("UdpTraceFileSimulation", LOG_LEVEL_INFO);
      LogComponentEnable("UdpTraceClient", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
  csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4InterfaceContainer ipv4Interfaces;
  Ipv6InterfaceContainer ipv6Interfaces;

  if (useIpv6)
    {
      ipv6Interfaces = Ipv6AddressHelper().Assign(devices);
      ipv6Interfaces.SetForwarding(1, true);
    }
  else
    {
      Ipv4AddressHelper ipv4;
      ipv4Interfaces = ipv4.Assign(devices);
    }

  uint16_t port = 9;

  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  InetSocketAddress remoteAddr;
  Ipv6Address remoteIpv6Addr;
  if (useIpv6)
    {
      remoteIpv6Addr = ipv6Interfaces.GetAddress(1, 1);
      remoteAddr = InetSocketAddress(remoteIpv6Addr, port);
    }
  else
    {
      remoteAddr = InetSocketAddress(ipv4Interfaces.GetAddress(1), port);
    }

  UdpTraceClient client;
  client.SetPeer(remoteAddr);
  client.SetTraceFile(traceFile);
  ApplicationContainer clientApp = client.Install(nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}