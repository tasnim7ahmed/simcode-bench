#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpStarTopology");

class MyApp : public Application
{
public:
  MyApp ();
  virtual ~MyApp ();
  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, Time pktInterval);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  Time            m_pktInterval;
  uint32_t        m_packetsSent;
  EventId         m_sendEvent;
  bool            m_running;
};

MyApp::MyApp ()
  : m_socket (0), 
    m_peer (), 
    m_packetSize (0), 
    m_nPackets (0), 
    m_pktInterval (Seconds (0)), 
    m_packetsSent (0), 
    m_sendEvent (), 
    m_running (false)
{}

MyApp::~MyApp ()
{
  m_socket = 0;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, Time pktInterval)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_pktInterval = pktInterval;
}

void
MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

void
MyApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    Simulator::Cancel (m_sendEvent);

  if (m_socket)
    m_socket->Close ();
}

void
MyApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  m_packetsSent++;
  if (m_packetsSent < m_nPackets)
    ScheduleTx ();
}

void
MyApp::ScheduleTx (void)
{
  if (m_running)
    {
      m_sendEvent = Simulator::Schedule (m_pktInterval, &MyApp::SendPacket, this);
    }
}

int 
main (int argc, char *argv[])
{
  uint32_t nClients = 5;
  uint32_t packetSize = 1024;
  uint32_t nPackets = 100;
  double dataRateMbps = 10.0;
  double delayMs = 2.0;
  double stopTime = 10.0;
  Time pktInterval = MilliSeconds (10);

  CommandLine cmd;
  cmd.AddValue ("nClients", "Number of clients", nClients);
  cmd.AddValue ("packetSize", "Size of each packet", packetSize);
  cmd.AddValue ("nPackets", "Number of packets per client", nPackets);
  cmd.Parse (argc, argv);

  NodeContainer serverNode;
  serverNode.Create (1);

  NodeContainer clientNodes;
  clientNodes.Create (nClients);

  NodeContainer allNodes;
  allNodes.Add (serverNode);
  allNodes.Add (clientNodes);

  std::vector<NodeContainer> p2pNodes;
  for (uint32_t i = 0; i < nClients; ++i)
    {
      NodeContainer pair (clientNodes.Get (i), serverNode.Get (0));
      p2pNodes.push_back (pair);
    }

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (std::to_string ((uint64_t)(dataRateMbps*1e6)) + "bps")));
  p2p.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (delayMs)));

  std::vector<NetDeviceContainer> devices;
  for (uint32_t i = 0; i < nClients; ++i)
    {
      devices.push_back (p2p.Install (p2pNodes[i]));
    }

  InternetStackHelper stack;
  stack.Install (allNodes);

  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> interfaces;
  for (uint32_t i = 0; i < nClients; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i+1 << ".0";
      address.SetBase (subnet.str ().c_str (), "255.255.255.0");
      interfaces.push_back (address.Assign (devices[i]));
    }

  uint16_t serverPort = 50000;
  Address serverAddress (InetSocketAddress (interfaces[0].GetAddress (1), serverPort));

  // Server app (PacketSink)
  ApplicationContainer serverApp;
  Address anyAddress (InetSocketAddress (Ipv4Address::GetAny (), serverPort));
  Ptr<PacketSink> sink = CreateObject<PacketSink> ();
  sink->SetAttribute ("Local", AddressValue (anyAddress));
  serverNode.Get (0)->AddApplication (sink);
  sink->SetStartTime (Seconds (0.0));
  sink->SetStopTime (Seconds (stopTime));

  // Clients
  for (uint32_t i = 0; i < nClients; ++i)
    {
      Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (clientNodes.Get (i), TcpSocketFactory::GetTypeId ());
      Ptr<MyApp> app = CreateObject<MyApp> ();
      Address serverIfAddr (InetSocketAddress (interfaces[i].GetAddress (1), serverPort));
      app->Setup (ns3TcpSocket, serverIfAddr, packetSize, nPackets, pktInterval);
      clientNodes.Get (i)->AddApplication (app);
      app->SetStartTime (Seconds (1.0 + 0.1*i));
      app->SetStopTime (Seconds (stopTime));
    }

  // NetAnim configuration
  AnimationInterface anim ("star-topology.xml");
  anim.SetConstantPosition (serverNode.Get (0), 50.0, 50.0);
  double radius = 30.0;
  for (uint32_t i = 0; i < nClients; ++i)
    {
      double angle = 2 * M_PI * i / nClients;
      double x = 50.0 + radius * std::cos (angle);
      double y = 50.0 + radius * std::sin (angle);
      anim.SetConstantPosition (clientNodes.Get (i), x, y);
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop (Seconds (stopTime));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  double totalThroughput = 0.0;
  double totalPacketLoss = 0.0;
  double totalLatency = 0.0;
  uint32_t latencyFlows = 0;

  for (auto& flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
      if (t.destinationPort == serverPort)
        {
          double throughput = flow.second.rxBytes * 8.0 / ( (flow.second.timeLastRxPacket.GetSeconds()-flow.second.timeFirstTxPacket.GetSeconds()) * 1e6 );
          double loss = flow.second.lostPackets;
          double latency = 0.0;
          if (flow.second.rxPackets > 0)
            {
              latency = (flow.second.delaySum.GetSeconds () / flow.second.rxPackets) * 1000.0;
              totalLatency += latency;
              ++latencyFlows;
            }
          totalThroughput += throughput;
          totalPacketLoss += loss;
        }
    }

  std::cout << "===== Simulation Statistics =====" << std::endl;
  std::cout << "Average Throughput  : " << totalThroughput / nClients << " Mbps" << std::endl;
  std::cout << "Average Latency     : " << (latencyFlows ? totalLatency / latencyFlows : 0.0) << " ms" << std::endl;
  std::cout << "Total Packet Loss   : " << totalPacketLoss << std::endl;

  Simulator::Destroy ();
  return 0;
}